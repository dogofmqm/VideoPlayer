#include "mainwindow.h"

#include <QApplication>

#undef main

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    // MainWindow 的内存在栈空间
    MainWindow w;
    w.show();
    return a.exec();
}

/*
音视频同步：
1、视频同步到音频（一般选用这个）因为SDL已经定好调用音频播放的时间

2、音频同步到视频
*/

/*
FFmpeg时间 与 现实时间
1、时间戳(timestamp)，类型是int64_t
2、时间基(time base\unit)，是时间戳的单位，类型是AVRational

FFmpeg的时间 与 现实时间的转换
1、现实时间 = 时间戳 * (时间基的分子 / 时间基的分母)
2、现实时间 = 时间戳 * av_q2d(时间基)
3、时间戳 = 现实时间 * 时间基的分母 / 时间基的分子
4、时间戳 = 现实时间 / av_q2d(时间基)
*/
