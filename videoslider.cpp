 #include "videoslider.h"
#include <QMouseEvent>
#include <QStyle>

VideoSlider::VideoSlider(QWidget *parent) : QSlider(parent)
{

}

void VideoSlider::mouseMoveEvent(QMouseEvent *ev)
{
//    int value = minimum() + (ev->pos().x() * 1.0 / width()) * (maximum() - minimum());

//    setValue(value);

    int value = QStyle::sliderValueFromPosition(minimum(),
                                                maximum(),
                                                ev->pos().x(),
                                                width());
    setValue(value);

    // 防止覆盖父类的同名函数mousePressEvent
    QSlider::mousePressEvent(ev);

    // 正常播放的时候滑条的值就在改变，所以得单独发出信号进行改变点击滑条的值
    emit clicked(this);
}


