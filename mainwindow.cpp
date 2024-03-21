#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>

#pragma mark - 构造、析构
MainWindow::MainWindow(QWidget *parent) // 主线程
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // setText()

    // 窗口标题
    this->setWindowTitle("垃圾播放器");
    // 设置窗口的icon
    this->setWindowIcon(QIcon(":/res/videoplayer.jpeg"));

    // 注册QT不能发送信号的参数类型，保证能够发出信号
    qRegisterMetaType<VideoPlayer::VideoSwsSpec>("VideoSwsSpec&");

    // 创建播放器类型
    _player = new VideoPlayer();

    connect(_player, &VideoPlayer::stateChanged,
            this, &MainWindow::onPlayerStateChanged);
    connect(_player, &VideoPlayer::timeChanged,
            this, &MainWindow::onPlayerTimeChanged);
    connect(_player, &VideoPlayer::initFinished,
            this, &MainWindow::onPlayerInitFinished);
    connect(_player, &VideoPlayer::playFailed,
            this, &MainWindow::onPlayerPlayFailed);

    connect(_player, &VideoPlayer::frameDecoded,
            ui->videoWidget, &VideoWidget::onPlayerFrameDecoded);
    connect(_player, &VideoPlayer::stateChanged,
            ui->videoWidget, &VideoWidget::onPlayerStateChanged);

    // 监听时间滑块的点击
    connect(ui->currentSlider, &VideoSlider::clicked,
            this, &MainWindow::onSliderClicked);

    // 设置音量滑块的范围
    ui->volumnSlider->setRange(VideoPlayer::Volumn::Min,
                               VideoPlayer::Volumn::Max);
    ui->volumnSlider->setValue(ui->volumnSlider->maximum());   
}

MainWindow::~MainWindow()
{
    delete ui;
    delete _player;
}

#pragma mark - 私有方法
void MainWindow::onSliderClicked(VideoSlider *slider)
{
    _player->setTime(slider->value());
}

void MainWindow::onPlayerInitFinished(VideoPlayer *player)
{
    int duration = player->getDuration();

    // 设置一些slider的范围
    ui->currentSlider->setRange(0, duration);

    // 设置label的文字
    ui->durationLabel->setText(getTimeText(duration));

}

void MainWindow::onPlayerPlayFailed(VideoPlayer *player)
{
    QMessageBox::critical(nullptr, "提示", "蠢逼，文件是坏的都不知道?");
}

void MainWindow::onPlayerStateChanged(VideoPlayer *player)
{
    VideoPlayer::State state = player->getState();
    if (state == VideoPlayer::Playing)
    {
        ui->playBtn->setText("暂停");
    }else
    {
        ui->playBtn->setText("播放");
    }

    if (state == VideoPlayer::Stopped)
    {
        ui->playBtn->setEnabled(false);
        ui->stopBtn->setEnabled(false);
        ui->currentSlider->setEnabled(false);
        ui->volumnSlider->setEnabled(false);
        ui->muteBtn->setEnabled(false);

        ui->durationLabel->setText(getTimeText(0));
        ui->currentSlider->setValue(0);

        // 显示打开文件的页面
        ui->playWidget->setCurrentWidget(ui->openFilePage);

    }else
    {
        ui->playBtn->setEnabled(true);
        ui->stopBtn->setEnabled(true);
        ui->currentSlider->setEnabled(true);
        ui->volumnSlider->setEnabled(true);
        ui->muteBtn->setEnabled(true);

        // 显示播放视频的页面
        ui->playWidget->setCurrentWidget(ui->videoPage);
    }
}

void MainWindow::onPlayerTimeChanged(VideoPlayer *player)
{
    ui->currentSlider->setValue(player->getTime());
}


void MainWindow::on_stopBtn_clicked()
{
    // 查看当前组件有多少个页面
//    int count = ui->playWidget->count();
//    qDebug() << count;
    // 查看组件当前所处的页面
//    int idx = ui->playWidget->currentIndex();
//    qDebug() << idx;
    // 点一下停止按钮，来回切换页面
//    ui->playWidget->setCurrentIndex(++idx % count);

    _player->stop();
}

void MainWindow::on_openFileBtn_clicked()
{
    QString filename = QFileDialog::getOpenFileName(nullptr,
                                                    "选择多媒体文件",
                                                    "E:/movie",
                                                    "多媒体文件 (*.mp4 *.avi *.mkv *.mp3 *.aac)");


//                                                    "音频文件 (*.mp3 *.aac);;"
//                                                   "视频文件 (*.mp4 *.avi *.mkv)");
    if (filename.isEmpty()) return;

    // 开始播放打开的文件
    // _player->setFilename(filename.toUtf8().data());
    // 设置播放器获取打开的文件名
    _player->setFilename(filename);
    // 播放状态
    _player->play();
}

QString MainWindow::getTimeText(int value)
{
    QLatin1Char fill = QLatin1Char('0');
    // 将value / 3600 转为十进制的两位数，如果只有个位数，则十位数填充为0
       return QString("%1:%2:%3")
              .arg(value / 3600, 2, 10, fill)
              .arg((value / 60) % 60, 2, 10, fill)
              .arg(value % 60, 2, 10, fill);
}

void MainWindow::on_currentSlider_valueChanged(int value)
{
    ui->currentLabel->setText(getTimeText(value));
}

void MainWindow::on_volumnSlider_valueChanged(int value)
{
    ui->volumnLabel->setText(QString("%1").arg(value));
    _player->setVolumn(value);
}

void MainWindow::on_playBtn_clicked()
{
    VideoPlayer::State state = _player->getState();
    if (state == VideoPlayer::Playing)
    {
        _player->pause();
    }else
    {
        _player->play();
    }
}



void MainWindow::on_muteBtn_clicked()
{
    if (_player->isMute())
    {
        _player->setMute(false);
        ui->muteBtn->setText("静音");
    } else
    {
        _player->setMute(true);
        ui->muteBtn->setText("外音");
    }
}
