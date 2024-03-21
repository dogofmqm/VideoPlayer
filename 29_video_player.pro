QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

RC_ICONS = favicon.ico

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    videoplayer.cpp \
    videoplayer_audio.cpp \
    videoplayer_video.cpp \
    videoslider.cpp \
    videowidget.cpp \
    condmutex.cpp

HEADERS += \
    mainwindow.h \
    videoplayer.h \
    videoslider.h \
    videowidget.h \
    condmutex.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

win32 {
    FFMPEG_HOME = E:/msys64/usr/local/ffmpeg
    SDL_HOME = E:/SOFT/FFmpeg/SDL2-devel-2.28.5-mingw/SDL2-2.28.5/x86_64-w64-mingw32
}
INCLUDEPATH += $${FFMPEG_HOME}/include
LIBS += -L $${FFMPEG_HOME}/lib \
            -lavcodec \  # 解码用的库
            -lavformat \ # 解封装用的库
            -lavutil \    # 工具
            -lswresample \
            -lswscale

INCLUDEPATH += $${SDL_HOME}/include

LIBS += -L$${SDL_HOME}/lib \
        -lSDL2

RESOURCES += \
    res.qrc \
    res.qrc \
    res.qrc


