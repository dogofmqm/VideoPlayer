#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QWidget>
#include <QImage>
#include "videoplayer.h"

// 显示、渲染视频

class VideoWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VideoWidget(QWidget *parent = nullptr);
    ~VideoWidget();

public slots:
    // VideoPlayer::VideoSwsSpec：因为VideoSwsSpec在VideoPlayer类中定义的
    void onPlayerFrameDecoded(VideoPlayer *player,
                              uint8_t *data,
                              VideoPlayer::VideoSwsSpec &spec); // 传引用，防止拷贝构造
    void onPlayerStateChanged(VideoPlayer *player);
private:
   QImage *_image = nullptr;
   QRect _rect;
   void paintEvent(QPaintEvent *event) override;

   void freeImage();

signals:

};

#endif // VIDEOWIDGET_H
