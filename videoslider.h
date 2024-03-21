#ifndef VIDEOSLIDER_H
#define VIDEOSLIDER_H

#include <QSlider>

class VideoSlider : public QSlider
{
    Q_OBJECT
public:
    explicit VideoSlider(QWidget *parent = nullptr);

signals:
    /** 手动点击 */
    void clicked(VideoSlider *slider);

private:
    // override：表示在父类中也有同样的函数声明
    void mouseMoveEvent(QMouseEvent *ev) override;

};

#endif // VIDEOSLIDER_H
