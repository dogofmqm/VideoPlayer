#include "videowidget.h"
#include <QDebug>
#include <QPainter>

VideoWidget::VideoWidget(QWidget *parent) : QWidget(parent)
{
    // 设置背景颜色
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet("background: black");
}

VideoWidget::~VideoWidget()
{
    freeImage();;
}

// 点击停止时，释放最后一帧的图片数据
void VideoWidget::onPlayerStateChanged(VideoPlayer *player)
{
    if (player->getState() != VideoPlayer::Stopped) return;

    freeImage();

    // 打开下一个视频时，画面变为黑色
    update();
}

void VideoWidget::paintEvent(QPaintEvent *event)
{
    if (!_image) return;

    // QPainter painter(this);

    // 将图片绘制到当前组件VideoWidget上
    QPainter(this).drawImage(_rect, *_image);
}

void VideoWidget::freeImage()
{
    // 每次调用都要释放之前存的一帧图片数据
    if(_image)
    {
        // bits指向的是rgb像素数据
        av_free(_image->bits());
        // 释放图片
        delete _image;
        _image = nullptr;
    }
}

void VideoWidget::onPlayerFrameDecoded(VideoPlayer *player, uint8_t *data, VideoPlayer::VideoSwsSpec &spec)
{
    if (player->getState() == VideoPlayer::Stopped) return;
    // 释放之前的图片
    freeImage();

    // 创建新的图片
    if (data != nullptr)
    {
        // data指向的是rgb数据
        _image = new QImage(data,
                            spec.width,
                            spec.height,
                            QImage::Format_RGB888);

        // 计算图片最终的尺寸
        // 组件的尺寸
        int w = width();
        int h = height();

        // 计算rect
        int dx = 0;
        int dy = 0;
        int dw = spec.width;
        int dh = spec.height;

        // 计算目标尺寸
        if (dw > w || dh > h)
        {
            // 缩放
            if (dw * h > w * dh) // 视频的宽高比 > 播放器的宽高比
            {
                dh = w * dh / dw;
                dw = w;
            }
            else
            {
                dw = h * dw / dh;
                dh = h;
            }
        }

        // 居中
        dx = (w - dw) >> 1;
        dy = (h - dh) >> 1;

        _rect = QRect(dx, dy, dw, dh);
    }

    // 调用updata就会触发paintEvent绘制图片
    update();

}
