#include "videoplayer.h"
#include <QDebug>
#include <thread>

extern "C"
{
#include <libavutil/imgutils.h>
}
int VideoPlayer::initVideoInfo()
{
    // 初始化解码器(解码上下文)
    int ret = initDecoder(&_vDecodeCtx, &_vStream, AVMEDIA_TYPE_VIDEO);
    RET(initDecoder);

    // 初始化视频像素格式转换
    ret = initSws();
    RET(initSws);

    return 0;
}

int VideoPlayer::initSws()
{
    int inW = _vDecodeCtx->width;
    int inH = _vDecodeCtx->height;

    _vSwsOutSpec.width = inW >> 4 << 4;
    _vSwsOutSpec.height = inH >> 4 << 4;
    _vSwsOutSpec.pixFmt = AV_PIX_FMT_RGB24;
    // 一帧的大小（一张图片的大小）
    _vSwsOutSpec.size = av_image_get_buffer_size(_vSwsOutSpec.pixFmt,
                                                 _vSwsOutSpec.width,
                                                 _vSwsOutSpec.height, 1);

    // 初始化视频像素格式转换的上下文
    _vSwsCtx = sws_getContext(inW, inH, _vDecodeCtx->pix_fmt,
                              _vSwsOutSpec.width, _vSwsOutSpec.height, _vSwsOutSpec.pixFmt,
                              SWS_BILINEAR, nullptr, nullptr, nullptr);

    if (!_vSwsCtx)
    {
        qDebug() << "sws_getContext error";
        return -1;
    }

    // 初始化视频像素格式转换的输入frame
    _vSwsInFrame = av_frame_alloc();
    if (!_vSwsInFrame)
    {
        qDebug() << "av_frame_alloc()";
        return -1;
    }

    // 初始化视频像素格式转换的输出frame
    _vSwsOutFrame = av_frame_alloc();
    if (!_vSwsOutFrame)
    {
        qDebug() << "av_frame_alloc()";
        return -1;
    }

    // 初始化重采样的输出frame的data[0]空间，
    // 因为_vSwsOutFrame->data[0]本来是空指针，需要初始化让data[0]指向一块堆空间
    int ret = av_image_alloc(_vSwsOutFrame->data, _vSwsOutFrame->linesize,
                             _vSwsOutSpec.width, _vSwsOutSpec.height, _vSwsOutSpec.pixFmt,
                             1);
    RET(av_image_alloc);

    return 0;
}

void VideoPlayer::addVideoPkt(AVPacket &pkt)
{
    _vMutex.lock();
    _vPktList.push_back(pkt);
    _vMutex.signal();
    _vMutex.unlock();
}

void VideoPlayer::clearVideoPktList()
{
    _vMutex.lock();
    for (AVPacket &pkt : _vPktList)
    {
        av_packet_unref(&pkt);
    }
    _vPktList.clear();
    _vMutex.unlock();
}

void VideoPlayer::freeVideo()
{
    clearVideoPktList();
    avcodec_free_context(&_vDecodeCtx);
    av_frame_free(&_vSwsInFrame);
    if (_vSwsOutFrame)
    {
        av_freep(&_vSwsOutFrame->data[0]);
        av_frame_free(&_vSwsOutFrame);
    }
    sws_freeContext(_vSwsCtx);
    _vSwsCtx = nullptr;
    _vStream = nullptr;
    _vTime = 0;
    _vCanFree = false;
    _vSeekTime = -1;
}

// 视频解码要一次性取出来，所以要用while循环
void VideoPlayer::decodeVideo()
{
    while (true)
    {
        // 如果是暂停，并且不需要seek
        if (_state == Paused && _vSeekTime == -1)
        {
            continue;
        }
        if (_state == Stopped)
        {
            _vCanFree = true;
            break;
        }
        _vMutex.lock();

        if (_vPktList.empty())
        {
            _vMutex.unlock();
            continue;
        }

        // 取出头部的视频包
        AVPacket pkt = _vPktList.front();
        _vPktList.pop_front();

        _vMutex.unlock();


        // 视频时钟
        if (pkt.dts != AV_NOPTS_VALUE)
        {
            _vTime = av_q2d(_vStream->time_base) * pkt.dts;
        }


        // 从pkt发送压缩数据到解码上下文
        int ret = avcodec_send_packet(_vDecodeCtx, &pkt);

        // 释放pkt
        av_packet_unref(&pkt);
        CONTINUE(avcodec_send_packet);

        while (true)
        {
            // 从_vDecodeCtx获取解码后的yuv420p数据到_vSwsInFrame(视频只拿到一帧的数据，音频拿到的是多个样本数据)
            ret = avcodec_receive_frame(_vDecodeCtx, _vSwsInFrame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) { // 表示数据已经读完
                break;
            } else BREAK(avcodec_receive_frame);

            // 一定要在解码成功后再进行判断
            // 发现视频的时间是早于seekTime的，直接丢弃
            if (_vSeekTime >= 0)
            {
                if (_vTime < _vSeekTime)
                {
                    continue;
                } else
                {
                    _vSeekTime = -1;
                }

            }

            // TODO表示未完成的工作  假停顿
//            SDL_Delay(33);

            // 像素格式的转换
            // _vSwsInFrame(yuv420p或其他格式的) -> _vSwsOutFrame(rgb24)
            // 转换 (stride就是linesize)
            sws_scale(_vSwsCtx, _vSwsInFrame->data, _vSwsInFrame->linesize,
                      0, _vDecodeCtx->height, _vSwsOutFrame->data, _vSwsOutFrame->linesize);

            // data[0]指向的数据就是RGB数据或者指向yuv420p格式的y平面
            // qDebug() << _vSwsOutFrame->data[0];

            if (_hasAudio) // 有音频
            {
                // 如果视频包过早被解码出来，那就需要等待对应的音频时钟到达
                while (_vTime > _aTime && _state == Playing)
                {
                     // SDL_Delay(5);
                }
            } else // 没有音频的情况
            {

            }


            // 把像素格式转换后的图片数据，拷贝一份出来
            uint8_t *data = (uint8_t *) av_malloc(_vSwsOutSpec.size);
            memcpy(data, _vSwsOutFrame->data[0], _vSwsOutSpec.size);

            // 解码完，发出信号
            emit frameDecoded(this, data, _vSwsOutSpec);
        }
    }
}
