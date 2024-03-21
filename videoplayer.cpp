#include "videoplayer.h"
#include <thread>
#include <QDebug>

extern "C" {
#include <libavutil/imgutils.h>
}

#define AUDIO_MAX_PKT_SIZE 1000
#define VIDEO_MAX_PKT_SIZE 500

#pragma mark - 构造、析构
VideoPlayer::VideoPlayer(QObject *parent) : QObject(parent)
{
    // 初始化Audio子系统
    if (SDL_Init(SDL_INIT_AUDIO)) {
        // 返回值不是0，就代表失败
        qDebug() << "SDL_Init Error" << SDL_GetError();
        emit playFailed(this);
        return;
    }
}

VideoPlayer::~VideoPlayer()
{
    // 这个对象不会发出任何消息
    disconnect();

    stop();

    // 退出所有的子系统
    SDL_Quit();
}

#pragma mark - 公共方法
// play在主线程
void VideoPlayer::play()
{
    if (_state == Playing) return;

    // 解封装、解码、播放、音视频同步

    if (_state == Stopped)
    {
        // 开启子线程: 读取解析文件   detach(): 表示readFile函数结束就会自动死亡
        // [] 捕获外面的变量 () 函数的参数 {} 函数体
        std::thread([this](){
            readFile();
        }).detach();
    }else
    {
        // 改变状态
        setState(Playing);
    }

}

void VideoPlayer::pause()
{
    if (_state != Playing) return;

    setState(Paused);
}

void VideoPlayer::stop()
{
    if (_state == Stopped) return;

    // 改变状态
    //setState(Stopped);

    _state = Stopped;
    // 释放资源
//    std::thread([this](){
//        SDL_Delay(100);
//        free();
//    }).detach();

    free();

    emit stateChanged(this);

}


bool VideoPlayer::isPlaying()
{
    // return _state == Playing;
   return _state == Playing;
}

VideoPlayer::State VideoPlayer::getState()
{
    return _state;
}

void VideoPlayer::setFilename(QString &filename)
{
    const char *name = filename.toUtf8().data();
    memcpy(_filename, name, strlen(name) + 1);
}

int VideoPlayer::getDuration()
{
    // round是四舍五入
    return _fmtCtx ? round(_fmtCtx->duration * av_q2d(AV_TIME_BASE_Q)) : 0;
}

int VideoPlayer::getTime()
{
    return round(_aTime);
}

void VideoPlayer::setTime(int seekTime)
{
    _seekTime = seekTime;
}

void VideoPlayer::setVolumn(int volumn)
{
    _volumn = volumn;
}

int VideoPlayer::getVolumn()
{
    return _volumn;
}

void VideoPlayer::setMute(bool mute)
{
    _mute = mute;
}

bool VideoPlayer::isMute()
{
    return _mute;
}


#pragma mark - 私有方法

void VideoPlayer::readFile()
{
    // 返回结果
    int ret = 0;

    // 创建解封装上下文、打开文件
    ret = avformat_open_input(&_fmtCtx, _filename, nullptr, nullptr);
    END(avformat_open_input);

    // 检索流信息
    ret = avformat_find_stream_info(_fmtCtx, nullptr);
    END(avformat_find_stream_info);

    // 打印流信息到控制台
    av_dump_format(_fmtCtx, 0, _filename, 0);
    fflush(stderr);

    // 初始化音频信息
    _hasAudio = initAudioInfo() >= 0;
    // 初始化视频信息
    _hasVideo = initVideoInfo() >= 0;
    if (!_hasAudio && !_hasVideo)
    {
        fataError();
        return;
    }

//    // 初始化音频信息
//    initAudioInfo();
//    // 初始化视频信息
//    initVideoInfo();
//    if (!_aStream && !_vStream)
//    {
//        fataError();
//        return;
//    }


    // 到此为止，初始化完毕
    // 虽然在子线程发送的信号，但是在主线程接收信号，所以initFinished在主线程执行
    emit initFinished(this);

    // 改变状态
    setState(Playing);

    // 音频解码子线程：开始播放
    // 一旦开始播放就会调用回调函数sdlAudioCallBackFunc
    SDL_PauseAudio(0);

    // 视频解码子线程：开启新的线程去解码视频数据
    std::thread([this](){
        decodeVideo();
    }).detach();

    // 读取文件数据在一个子线程，解码音频在另外一个子线程，解码视频在其他子线程
    // pkt.data[0];存的就是从输入文件读取的数据
    // pkt是对象不用初始化，如果是*pkt 指针变量则要初始化
    // while循环中addAudioPkt调用push_back()会调用拷贝构造，插到集合里面的是拷贝出来的对象，所以pkt不用放在while循环里面
    AVPacket pkt;
    while (_state != Stopped)
    {
        // _seekTime >= 0：说明需要滑动滑条
        if (_seekTime >= 0)
        {
            int streamIdx;
            if (_hasAudio) // 优先使用音频流索引
            {
                streamIdx = _aStream->index;
            } else
            {
                streamIdx = _vStream->index;
            }

            // 现实时间 -> 时间戳
            int64_t ts = _seekTime / av_q2d(_fmtCtx->streams[streamIdx]->time_base);
            ret = av_seek_frame(_fmtCtx, streamIdx, ts, AVSEEK_FLAG_BACKWARD);
            if (ret < 0)
            {
                // qDebug() << "seek失败" << _seekTime << ts << streamIdx;
                _seekTime = -1;
            } else
            {
                // qDebug() << "seek成功" << _seekTime << ts << streamIdx;
                _vSeekTime = _seekTime;
                _aSeekTime = _seekTime;
                _seekTime = -1;
                // 恢复时钟，重新计算时钟
                // 不恢复的话，可能_vTime比_aTime大，会阻塞一段时间
                _aTime = 0;
                _vTime = 0;
                // 清空点击滑条之前读取的数据包，不然seek没反应
                clearAudioPktList();
                clearVideoPktList();

            }
        }

        // 限制音频包和视频包的大小，防止包过大出问题
        if (_vPktList.size() >= VIDEO_MAX_PKT_SIZE ||
            _aPktList.size() >= AUDIO_MAX_PKT_SIZE)
        {
            // SDL_Delay(10);
            continue;
        }

        // 从输入文件中读取数据到pkt里面
        ret = av_read_frame(_fmtCtx, &pkt);

        // 读取到的是音频流的数据
        if (ret == 0)
        {
            if (pkt.stream_index == _aStream->index)
            {
                addAudioPkt(pkt);
            }else if(pkt.stream_index == _vStream->index){ // 读取到的是视频流的数据
                addVideoPkt(pkt);
            } else // 如果不是音频或者视频流， 直接释放
            {
                // 将pkt.data清0，并将pkt.data[0]清0
                av_packet_unref(&pkt);
            }
            // 读到了文件的尾部，不能break退出循环，要等待用户是否进行seek操作
        } else if (ret == AVERROR_EOF) {
            if (_vPktList.size() == 0 && _aPktList.size() == 0)
            {
                // 说明文件正常播放完毕
                _fmtCtxCanFree = true;
                break;
            }

        }else {
            ERROR_BUF;
            qDebug() << "av_read_frame error" << errbuf;
            continue;
        }
    }

    if (_fmtCtxCanFree) // 文件正常播放完毕
    {
        stop();
    } else
    {
        // 标记一下：_fmtCtx可以释放了
        _fmtCtxCanFree = true;
    }

}

// 初始化解码器
int VideoPlayer::initDecoder(AVCodecContext **decodeCtx,
                         AVStream **stream,
                         AVMediaType type)
{
    // 寻找最合适的流的信息
    // 返回值是流索引
    int ret = av_find_best_stream(_fmtCtx, type, -1, -1, nullptr, 0);
    RET(av_find_best_stream);

    // 检验流是否有效
    int streamIdx = ret;
    *stream = _fmtCtx->streams[streamIdx];
    if (!*stream)
    {
        qDebug() << " stream is empty";
        return -1;
    }

    // 为当前流找到合适的解码器
//    const AVCodec *decoder = nullptr;
//    if (stream->codecpar->codec_id == AV_CODEC_ID_AAC)
//    {
//        decoder = avcodec_find_decoder_by_name("libfdk_aac");
//    }
//    else
//    {
//        decoder = avcodec_find_decoder(stream->codecpar->codec_id);
//    }

    // 默认的解码器是aac解码器
    const AVCodec *decoder = avcodec_find_decoder((*stream)->codecpar->codec_id);
    if (!decoder)
    {
        qDebug() << "avcodec_find_decoder error" << (*stream)->codecpar->codec_id;
        return -1;
    }

    // 初始化解码上下文
    *decodeCtx = avcodec_alloc_context3(decoder);
    if (!decodeCtx)
    {
        qDebug() << "avcodec_alloc_context3 error";
        return -1;
    }

    // 从流中拷贝参数到解码上下文中
    ret = avcodec_parameters_to_context(*decodeCtx, (*stream)->codecpar);
    RET(avcodec_parameters_to_context);

    // 打开解码器
    ret = avcodec_open2(*decodeCtx, decoder, nullptr);
    RET(avcodec_open2);

    return 0;
}

void VideoPlayer:: setState(State state)
{
    if (state == _state) return;

    _state = state;
    // this指向的是VideoPlayer类
    emit stateChanged(this);
}


void VideoPlayer::free()
{
    while (_hasAudio && !_aCanFree);
    while (_hasVideo && !_vCanFree);
    while(!_fmtCtxCanFree);
    avformat_close_input(&_fmtCtx);
    _fmtCtxCanFree = false;
    _seekTime = -1;

    freeAudio();
    freeVideo();
}

void VideoPlayer::fataError()
{
    setState(Stopped);
    emit playFailed(this);
    free();
}



