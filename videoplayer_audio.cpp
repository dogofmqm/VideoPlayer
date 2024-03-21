#include "videoplayer.h"
#include <QDebug>

int VideoPlayer::initAudioInfo()
{
    // 初始化解码器
    int ret = initDecoder(&_aDecodeCtx, &_aStream, AVMEDIA_TYPE_AUDIO);
    RET(initDecoder);

    // 初始化音频重采样
    ret = initSwr();
    RET(initSwr);

    // 初始化SDL
    ret = initSDL();
    RET(initSDL);

    return 0;
}

int VideoPlayer::initSwr()
{
    // 重采样输入参数
//    _aSwrInSpec.sampleFmt = _aDecodeCtx->sample_fmt;
//    _aSwrInSpec.sampleRate = _aDecodeCtx->sample_rate;
//    _aSwrInSpec.chLayout = _aDecodeCtx->channel_layout;
//    _aSwrInSpec.chs = _aDecodeCtx->channels;

    // 重采样输出参数
//    _aSwrOutSpec.sampleFmt = AV_SAMPLE_FMT_S16;
//    _aSwrOutSpec.sampleRate = 44100;
//    _aSwrOutSpec.chLayout = AV_CH_LAYOUT_STEREO;
//    _aSwrOutSpec.chs = av_get_channel_layout_nb_channels(_aSwrOutSpec.chLayout);
//    _aSwrOutSpec.bytesPerSampleFrame = _aSwrOutSpec.chs * av_get_bytes_per_sample(_aSwrOutSpec.sampleFmt);

//    // 创建重采样上下文
     // _aSwrCtx = swr_alloc_set_opts(nullptr,
//                                         // 输出参数
//                                         _aSwrOutSpec.chLayout,
//                                         _aSwrOutSpec.sampleFmt,
//                                         _aSwrOutSpec.sampleRate,
//                                         // 输入参数
//                                         _aSwrInSpec.chLayout,
//                                         _aSwrInSpec.sampleFmt,
//                                         _aSwrInSpec.sampleRate,
//                                         0, nullptr);

    // 输入参数
    _aSwrInSpec.sampleFmt = _aDecodeCtx->sample_fmt;
    _aSwrInSpec.sampleRate = _aDecodeCtx->sample_rate;
    _aSwrInSpec.chLayout = _aDecodeCtx->ch_layout;
    _aSwrInSpec.chs = _aDecodeCtx->ch_layout.nb_channels;
    // 输出参数
    _aSwrOutSpec.sampleFmt = AV_SAMPLE_FMT_S16;
    _aSwrOutSpec.sampleRate = 44100;
    _aSwrOutSpec.chLayout = _aSwrInSpec.chLayout;
    _aSwrOutSpec.chs = _aSwrOutSpec.chLayout.nb_channels;
    _aSwrOutSpec.bytesPerSampleFrame = _aSwrOutSpec.chs * av_get_bytes_per_sample(_aSwrOutSpec.sampleFmt);

    // 创建重采样上下文
    int ret  = swr_alloc_set_opts2(&_aSwrCtx,
                                             // 输出参数
                                             &_aSwrOutSpec.chLayout,
                                             _aSwrOutSpec.sampleFmt,
                                             _aSwrOutSpec.sampleRate,
                                             // 输入参数
                                             &_aSwrInSpec.chLayout,
                                             _aSwrInSpec.sampleFmt,
                                             _aSwrInSpec.sampleRate,
                                             0, nullptr);
    RET(swr_alloc_set_opts2);

    // 初始化重采样上下文
    ret = swr_init(_aSwrCtx);
    RET(swr_init);

    // 初始化输入frame
    _aSwrInFrame = av_frame_alloc();
    if (!_aSwrInFrame)
    {
        qDebug() << "av_frame_alloc()";
        return -1;
    }

    // 初始化输出frame
    _aSwrOutFrame = av_frame_alloc();
    if (!_aSwrOutFrame)
    {
        qDebug() << "av_frame_alloc()";
        return -1;
    }

    // 初始化重采样的输出frame的data[0]空间，因为没初始化之前data[0]是空指针
    // data[0]指向的就是pcm数据
    ret = av_samples_alloc(_aSwrOutFrame->data,
                     _aSwrOutFrame->linesize,
                     _aSwrOutSpec.chs,
                     4096,
                     _aSwrOutSpec.sampleFmt,
                     1);
    RET(av_samples_alloc);

    return 0;
}

// 回调函数是c语言函数，要在声明前面加static
void VideoPlayer::sdlAudioCallBackFunc(void *userdata, Uint8 *stream, int len)
{
    VideoPlayer *player = (VideoPlayer *)userdata;
    player->sdlAudioCallBack(stream, len);
}

// 在回调函数里面进行解码，得到pcm数据后，填充到缓冲区里面，最后SDL进行播放。SDL会让回调函数在子线程调用
void VideoPlayer::sdlAudioCallBack(Uint8 *stream, int len)
{
    // 清零（靜音处理）
    SDL_memset(stream, 0, len);
    // stream: 指向SDL音频缓冲区
    // len: SDL音频缓冲区剩余的大小，也就是还未填充的大小
    while (len > 0)
    {
        if (_state == Paused) break;

        if (_state == Stopped)
        {
            _aCanFree = true;
            break;
        }
        // 当前pcm的数据全部拷贝到SDL音频缓冲区了
        // 需要解码下一个pkt，获取新的pcm数据
        if (_aSwrOutIdx >= _aSwrOutSize)
        {
            _aSwrOutSize = decodeAudio();
            // 索引清0
            _aSwrOutIdx = 0;
            // 没有解码出pcm数据,那就静音处理
            if (_aSwrOutSize <= 0)
            {
                // 假定pcm的大小
                _aSwrOutSize = 1024;
                // 给pcm填充0（静音处理）
                memset(_aSwrOutFrame->data[0], 0, _aSwrOutSize);
            }

        }

        // fillLen: 本次需要填充到stream中(SDL音频缓冲区)的pcm数据大小
        int fillLen = _aSwrOutSize - _aSwrOutIdx;
        fillLen = std::min(fillLen, len);

        int volumn = _mute ? 0 : ((_volumn * 1.0 / Max) * SDL_MIX_MAXVOLUME);

        // 将一个pkt包解码后的pcm数据填充到SDL的音频缓冲区stream
        // _aSwrOutFrame->data[0]: 指向的是填充解码后pcm数据的缓冲区
        // 在这里设置音频播放的音量大小
        SDL_MixAudio(stream,
                     _aSwrOutFrame->data[0] + _aSwrOutIdx,
                    fillLen,
                    volumn);

        // 移动偏移量
        len -= fillLen;
        stream += fillLen;
        _aSwrOutIdx += fillLen;

    }
}

int VideoPlayer::initSDL()
{
    // 音频参数
    SDL_AudioSpec spec;
    // 采样率
    spec.freq = _aSwrOutSpec.sampleRate;
    // 采样格式（s16le）
    spec.format = AUDIO_S16LSB;
    // 声道数
    spec.channels = _aSwrOutSpec.chs;
    // 音频缓冲区的样本数量（这个值必须是2的幂）
    spec.samples = 512;
    // 回调
    spec.callback = sdlAudioCallBackFunc;
    // 传递给回调的参数
    // this指向的是VideoPlayer类
    spec.userdata = this;

    // 打开音频设备
    if (SDL_OpenAudio(&spec, nullptr)) {
        qDebug() << "SDL_OpenAudio Error" << SDL_GetError();
        return -1;
    }

    return 0;
}

void VideoPlayer::addAudioPkt(AVPacket &pkt)
{
    // 防止添加数据和取数据同时进行
    _aMutex.lock();
    // 调用push_back()会调用拷贝构造，插到集合里面的是拷贝出来的对象
    _aPktList.push_back(pkt);
    _aMutex.signal();
    _aMutex.unlock();
}

// 点击停止按钮后，将列表剩余的数据清空
void VideoPlayer::clearAudioPktList()
{
    _aMutex.lock();
    // 遍历列表
    for (AVPacket &pkt : _aPktList)
    {

        av_packet_unref(&pkt);
    }
    _aPktList.clear();
    _aMutex.unlock();
}


// 音频解码是解码一次就返回一次，不能用循环
int VideoPlayer::decodeAudio()
{
    // 加锁
    _aMutex.lock();

    if (_aPktList.empty())
    {
        _aMutex.unlock();
        return 0;
    }

    // 取出头部的数据包
    // 会发生拷贝构造，产生两个pkt对象
    AVPacket pkt = _aPktList.front();

    // 从头部中删除
    // 将原来的pkt对象从集合中删除，但data指向的数据没有删除
    _aPktList.pop_front();

    // 解锁
    _aMutex.unlock();

    // 保存音频时钟
    if (pkt.pts != AV_NOPTS_VALUE)
    {
         _aTime = av_q2d(_aStream->time_base) * pkt.pts;
         // 通知外界：播放时间点发生了改变
         emit timeChanged(this);
    }

    // 发现音频的时间是早于seekTime的，直接丢弃
    if (_aSeekTime >= 0)
    {
        if (_aTime < _aSeekTime)
        {
            av_packet_unref(&pkt);

            return 0;
        } else
        {
            _aSeekTime = -1;
        }

    }

    // 发送压缩数据到解码上下文
    // 这里的pkt是复制的pkt对象
    int ret = avcodec_send_packet(_aDecodeCtx, &pkt);
    // 释放复制的pkt.data和pkt.data[0]，后面用不到pkt，所以在这里释放
    // 由于pkt是在栈中定义的，所以函数结束后pkt就会自动销毁
    av_packet_unref(&pkt);
    RET(avcodec_send_packet)


    // 从解码上下文获取解码后的数据到_aSwrInFrame
    // _aSwrInFrame中的data[0]指向的内存空间，avcodec_receive_frame函数内部会初始化
    ret = avcodec_receive_frame(_aDecodeCtx, _aSwrInFrame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
         return 0;
    } else RET(avcodec_receive_frame)

    // 输出的样本数量（AV_ROUND_UP是向上取整）
    int outSamples = av_rescale_rnd(_aSwrOutSpec.sampleRate,
                                    _aSwrInFrame->nb_samples,
                                    _aSwrInSpec.sampleRate,
                                    AV_ROUND_UP);
    // 由于解码出来的pcm和SDL要求的pcm格式可能不一致
    // 需要进行音频重采样
    // 重采样(返回值ret是 重采样后的一个声道的样本数量)
    // _aSwrOutFrame->data: 重采样后输出的数据
    ret = swr_convert(_aSwrCtx,
                      _aSwrOutFrame->data,
                      outSamples,
                      (const uint8_t **)_aSwrInFrame->data,
                      _aSwrInFrame->nb_samples
                     );

    RET(swr_convert);

    return ret * _aSwrOutSpec.bytesPerSampleFrame;

}

void VideoPlayer::freeAudio()
{
    _aTime = 0;
    _aSwrOutIdx = 0;
    _aSwrOutSize = 0;
    _aStream = nullptr;
    _aCanFree = false;
    _aSeekTime = -1;

    clearAudioPktList();
    avcodec_free_context(&_aDecodeCtx);
    swr_free(&_aSwrCtx);
    av_frame_free(&_aSwrInFrame);
    if (_aSwrOutFrame)
    {
        av_freep(&_aSwrOutFrame->data[0]);
        av_frame_free(&_aSwrOutFrame);
    }

    // 停止播放
    SDL_PauseAudio(1);
    // 关掉音频子系统
    SDL_CloseAudio();
}
