QT       += core gui opengl openglwidgets multimedia network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17 cuda
CONFIG += c++2a
QMAKE_CXXFLAGS += -mavx2 -mfma
DEFINES += __STDC_CONSTANT_MACROS

WINSDK_INCLUDE = $$(WindowsSdkDir)/Include/$$(WindowsSDKVersion)/winrt
WINSDK_UM_INCLUDE = $$(WindowsSdkDir)/Include/$$(WindowsSDKVersion)/um
# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    component/audioitemwidget.cpp \
    component/audiolevelbar.cpp \
    component/audiomixerdialog.cpp \
    component/outputsettingdialog.cpp \
    component/camerasettingdialog.cpp \
    component/filesettingdialog.cpp \
    component/screensettingdialog.cpp \
    component/componentinitializer.cpp \
    component/scrollablecontainer.cpp \
    component/statisticsdialog.cpp \
    component/textsettingdialog.cpp \
    controller/dynamicbitratecontroller.cpp \
    controller/mediasourcecontroller.cpp \
    controller/streamcontroller.cpp \
    decoder/adecoder.cpp \
    dxgi/wgccapturecore.cpp \
    encoder/aencoder.cpp \
    encoder/vencoder.cpp \
    filter/audiofiltergraph.cpp \
    filter/mediaspeedfilter.cpp \
    mixer/audiomixer.cpp \
    mixer/audiomixprocessor.cpp \
    monitor/audiomonitor.cpp \
    monitor/networkmonitor.cpp \
    monitor/systemmonitor.cpp \
    muxer/muxer.cpp \
    muxer/muxermanager.cpp \
    pool/framepool.cpp \
    pool/packetpool.cpp \
    queue/cudaframequeue.cpp \
    resample/aresampler.cpp \
    scene/audiosourcemanager.cpp \
    source/audio/desktopaudiosource.cpp \
    source/audio/mediaaudiosource.cpp \
    source/audio/microphoneaudiosource.cpp \
    controlbar.cpp \
    decoder/vdecoder.cpp \
    demux/demuxer.cpp \
    dxgi/dxgidesktopduplicator.cpp \
    render/cudarenderwidget.cpp \
    render/cudainterophelper.cpp \
    resample/vrescaler.cpp \
    scene/scene.cpp \
    scene/scenemanager.cpp \
    source/video/textsource.cpp \
    sync/avsyncclock.cpp \
    sync/globalclock.cpp \
    sync/mediaclock.cpp \
    sync/offsetmanager.cpp \
    sync/synccore.cpp \
    thread/asourcetaskthread.cpp \
    thread/mediasourcetaskthread.cpp \
    thread/threadpool.cpp \
    thread/vencodethread.cpp \
    thread/vsourcetaskthread.cpp \
    source/video/camerasource.cpp \
    source/video/desktopsource.cpp \
    source/video/localvideosource.cpp \
    source/video/videosource.cpp \
    queue/framequeue.cpp \
    queue/packetqueue.cpp \
    filter/selectioneventfilter.cpp \
    statusbar/mystatusbar.cpp \
    statusbar/statusbarmanager.cpp \
    timer/rendertimer.cpp \
    main.cpp \
    mainwindow.cpp \
    transition/directtransition.cpp \
    transition/fadetransition.cpp \
    transition/slidetransition.cpp \
    transition/transitionmanager.cpp


HEADERS += \
    component/audioitemwidget.h \
    component/audiolevelbar.h \
    component/audiomixerdialog.h \
    component/outputsettingdialog.h \
    component/camerasettingdialog.h \
    component/filesettingdialog.h \
    component/screensettingdialog.h \
    component/componentinitializer.h \
    component/scrollablecontainer.h \
    component/statisticsdialog.h \
    component/textsettingdialog.h \
    controller/dynamicbitratecontroller.h \
    controller/mediasourcecontroller.h \
    controller/streamcontroller.h \
    decoder/adecoder.h \
    dxgi/cudacursorrenderer.h \
    dxgi/cursorinfo.h \
    dxgi/wgccapturecore.h \
    encoder/aencoder.h \
    encoder/vencoder.h \
    ffmpegutils.h \
    filter/audiofiltergraph.h \
    filter/mediaspeedfilter.h \
    mixer/audiomixer.h \
    mixer/audiomixprocessor.h \
    monitor/audiomonitor.h \
    monitor/networkmonitor.h \
    monitor/systemmonitor.h \
    muxer/muxer.h \
    muxer/muxermanager.h \
    pool/framepool.h \
    pool/gloabalpool.h \
    pool/packetpool.h \
    queue/cudaframequeue.h \
    resample/aresampler.h \
    scene/audiosourcemanager.h \
    source/audio/audiosource.h \
    source/audio/desktopaudiosource.h \
    source/audio/mediaaudiosource.h \
    source/audio/microphoneaudiosource.h \
    controlbar.h \
    decoder/vdecoder.h \
    demux/demuxer.h \
    dxgi/dxgidesktopduplicator.h \
    infotools.h \
    render/rgbatonv12.h \
    render/bgratorgba.h \
    render/cudainterop_cuda.h \
    render/cudainterophelperimpl.h \
    render/cudarenderwidget.h \
    render/cudainterophelper.h \
    render/framelayer.h \
    render/nv12torgba.h \
    resample/vrescaler.h \
    scene/scene.h \
    scene/scenemanager.h \
    source/video/textsource.h \
    statusbar/cpumonitor.h \
    sync/avsyncclock.h \
    sync/globalclock.h \
    sync/mediaclock.h \
    sync/offsetmanager.h \
    sync/seeksync.h \
    sync/synccore.h \
    thread/asourcetaskthread.h \
    thread/mediasourcetaskthread.h \
    thread/threadpool.h \
    thread/vencodethread.h \
    thread/vsourcetaskthread.h \
    source/video/camerasource.h \
    source/video/desktopsource.h \
    source/video/localvideosource.h \
    source/video/videosource.h \
    queue/framequeue.h \
    queue/packetqueue.h \
    statusbar/mystatusbar.h \
    statusbar/statusbarmanager.h \
    statusbar/fpscounter.h \
    filter/selectioneventfilter.h \
    timer/rendertimer.h \
    VideoFrame.h \
    mainwindow.h \
    transition/directtransition.h \
    transition/fadetransition.h \
    transition/slidetransition.h \
    transition/transitionbase.h \
    transition/transitionmanager.h



FORMS += \
    component/audioitemwidget.ui \
    component/audiomixerdialog.ui \
    component/camerasettingdialog.ui \
    component/filesettingdialog.ui \
    component/outputsettingdialog.ui \
    component/screensettingdialog.ui \
    component/statisticsdialog.ui \
    component/textsettingdialog.ui \
    controlbar.ui \
    mainwindow.ui


INCLUDEPATH += $$PWD/include \

LIBS        += -L$$PWD/lib -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lpostproc -lswresample -lswscale -lvld \

# WinRT核心库（WGC依赖）
LIBS += -lwindowsapp
# D3D库
LIBS += -ld3d11 -ldxgi
# Win32库
LIBS += -luser32 -lole32 -luuid
# 排除不必要的警告（WinRT编译可能产生大量警告）
QMAKE_CXXFLAGS += /wd4819 /wd4467


# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    Resources.qrc \
    app.rc \
    applogo.rc

RC_FILE += applogo.rc
RC_FILE += app.rc

# ---------------------------------------
# CUDA CONFIGURATION
# ---------------------------------------


OBJECTS += $$PWD/cuda_objects/cudainterop_cuda.obj \
           $$PWD/cuda_objects/nv12torgba.obj \
           $$PWD/cuda_objects/bgratorgba.obj \
           $$PWD/cuda_objects/rgbatonv12.obj \
           $$PWD/cuda_objects/cudacursorrenderer.obj \

# CUDA 相关包含路径
INCLUDEPATH += "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.6/include" \
               $$PWD/render

# CUDA 运行时库路径和库文件
LIBS += -L"C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.6/lib/x64" \
        -lcudart -lcuda

# MSVC 运行时链接选项，注意与主项目保持一致（Debug/Release）
QMAKE_CXXFLAGS_DEBUG += /MDd
QMAKE_CXXFLAGS_RELEASE += /MD

DISTFILES += \
    sources/app_logo.ico
