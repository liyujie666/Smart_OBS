#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "scene/scenemanager.h"
#include "component/camerasettingdialog.h"
#include "component/filesettingdialog.h"
#include "component/screensettingdialog.h"
#include "component/textsettingdialog.h"
#include "component/audiomixerdialog.h"
#include "component/audioitemwidget.h"
#include "component/statisticsdialog.h"
#include "source/video/camerasource.h"
#include "source/video/desktopsource.h"
#include "source/video/localvideosource.h"
#include "source/audio/desktopaudiosource.h"
#include "source/audio/microphoneaudiosource.h"
#include "source/audio/mediaaudiosource.h"
#include "source/video/textsource.h"
#include "statusbar/mystatusbar.h"
#include "scene/scene.h"
#include "filter/selectioneventfilter.h"
#include "pool/gloabalpool.h"
#include "sync/offsetmanager.h"
#include "transition/transitionmanager.h"
#include "ffmpegutils.h"
#include <QScrollArea>
#include <QApplication>
#include <QScreen>
#include <QStandardPaths>
#include <QComboBox>
#include <QTimer>
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)

{
    ui->setupUi(this);
    adjustOpenGLWidgetSize();
    setWindowTitle("Smart_OBS");

    m_sceneManager = new SceneManager(this);
    m_audioSourceManager = new AudioSourceManager(this);
    m_threadPool = new ThreadPool(this);
    ComponentInitializer::getInstance()->init(this,m_sceneManager);
    ui->openGLWidget->setSceneManager(m_sceneManager);
    m_networkMonitor = new NetworkMonitor(this);
    m_tcpMonitor = new NetworkMonitor(this);
    initUI();
    m_trayIcon.setIcon(QIcon(":/sources/app_logo.ico"));
    m_trayIcon.show();
    connect(ui->openGLWidget,&CudaRenderWidget::layerClicked,this,&MainWindow::onLayerClicked);
    connect(m_tcpMonitor, &NetworkMonitor::serverClosed,this,&MainWindow::on_StreamPushingClosed);
    // QTimer *poolTimer = new QTimer(this);
    // poolTimer->setInterval(2000);
    // connect(poolTimer,&QTimer::timeout,this,&MainWindow::printPoolStats);
    // poolTimer->start();

}

MainWindow::~MainWindow()
{
    if (m_streamController) {
        m_streamController->stop();
        delete m_streamController;
        m_streamController = nullptr;
    }

    if (m_mediaController) {
        m_mediaController->stopAllMediaSources();
        delete m_mediaController;
        m_mediaController = nullptr;
    }


    if (m_threadPool) {
        m_threadPool->stopATasks();
        m_threadPool->stopVTasks();
        delete m_threadPool;
        m_threadPool = nullptr;
    }

    if (m_audioSourceManager) {
        delete m_audioSourceManager;
        m_audioSourceManager = nullptr;
    }

    if (m_sceneManager) {
        m_sceneManager->release();
        delete m_sceneManager;
        m_sceneManager = nullptr;
    }

    ComponentInitializer::destroyInstance();
    AudioMixerDialog::destroyInstance();
    StatusBarManager::getInstance().release();


    if (ui->openGLWidget) {
        ui->openGLWidget->release();
    }

    if (m_mixerContainer) {
        delete m_mixerContainer;
        m_mixerContainer = nullptr;
    }
    if (m_sysMonitor) {
        delete m_sysMonitor;
        m_sysMonitor = nullptr;
    }
    if (m_networkMonitor) {
        delete m_networkMonitor;
        m_networkMonitor = nullptr;
    }
    if (m_tcpMonitor) {
        delete m_tcpMonitor;
        m_tcpMonitor = nullptr;
    }
    if (m_updateTimer) {
        m_updateTimer->stop();
        delete m_updateTimer;
        m_updateTimer = nullptr;
    }
    if (m_controlBar) {
        delete m_controlBar;
        m_controlBar = nullptr;
    }


    GlobalPool::getFramePool().clear();    // 释放所有缓存帧
    GlobalPool::getPacketPool().clear();   // 释放所有缓存包

    delete ui;

}

ControlBar *MainWindow::getControlBar()
{
    return m_controlBar;
}

MediaSourceController *MainWindow::getMediaSourceController()
{
    return m_mediaController;
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    adjustOpenGLWidgetSize();
}
void MainWindow::initUI()
{
    initAddSourceButton();
    initScenes();
    initMixer();
    initConfig();
    initStatusBar();
    initToolTip();
    initTransitionUI();
    initSysMonitor();
}

void MainWindow::initAddSourceButton()
{
    QPushButton* addButton = findChild<QPushButton*>("addSourceBtn");
    QWidget* sourceWidget = findChild<QWidget*>("sourceWidget");
    QVBoxLayout* sourceLayout = new QVBoxLayout(sourceWidget);
    sourceLayout->setContentsMargins(6, 6, 6, 6);
    sourceLayout->setSpacing(3);
    sourceLayout->setAlignment(Qt::AlignTop);

    auto* screenBtn = ComponentInitializer::getInstance()->createIconButton("显示器捕获",this,QIcon(":/sources/screen.png"));
    auto* camBtn = ComponentInitializer::getInstance()->createIconButton("摄像头",this,QIcon(":/sources/camera.png"));
    auto* fileBtn = ComponentInitializer::getInstance()->createIconButton("媒体文件",this,QIcon(":/sources/video_file.png"));
    auto* windowBtn = ComponentInitializer::getInstance()->createIconButton("窗口捕获",this,QIcon(":/sources/window.png"));
    auto* textBtn = ComponentInitializer::getInstance()->createIconButton("文本",this,QIcon(":/sources/text2.png"));
    auto* colorBtn = ComponentInitializer::getInstance()->createIconButton("颜色板",this,QIcon(":/sources/color_plate2.png"));

    QList<QPushButton*> buttons = {screenBtn, camBtn, fileBtn,windowBtn,textBtn,colorBtn};
    auto* popup = ComponentInitializer::getInstance()->createIconPopup(this, buttons);

    connect(screenBtn, &QPushButton::clicked, this, [=]() {
        handleSourceAdd(popup,sourceWidget, "显示器",QIcon(":/sources/screen.png"),VideoSourceType::Desktop);
    });

    connect(camBtn, &QPushButton::clicked, this, [=]() {
        handleSourceAdd(popup,sourceWidget, "摄像头",QIcon(":/sources/camera.png"),VideoSourceType::Camera);
    });

    connect(fileBtn, &QPushButton::clicked, this, [=]() {
        handleSourceAdd(popup,sourceWidget, "媒体源",QIcon(":/sources/video_file.png"),VideoSourceType::Media);
    });

    connect(textBtn, &QPushButton::clicked, this, [=]() {
        handleSourceAdd(popup,sourceWidget, "文本",QIcon(":/sources/text2.png"),VideoSourceType::Text);
    });

    // 处理弹出按钮
    connect(addButton, &QPushButton::clicked, this, [=]() {
        if (popup->isVisible()) {
            popup->hide();
            return;
        }

        popup->adjustSize();
        QPoint globalPos = addButton->mapToGlobal(QPoint(0, -popup->height()));
        QRect screenRect = QApplication::primaryScreen()->geometry();
        if (QScreen* screen = QGuiApplication::screenAt(globalPos)) {
            screenRect = screen->geometry();
        }

        int x = qBound(screenRect.left(), globalPos.x(), screenRect.right() - popup->width());
        int y = qBound(screenRect.top(), globalPos.y(), screenRect.bottom() - popup->height());

        popup->move(x, y);
        popup->show();
    });
}

void MainWindow::initScenes()
{
    // 场景点击信号
    connect(ui->sceneListWidget, &QListWidget::itemClicked,
            this, &MainWindow::onSceneItemClicked);

    connect(m_sceneManager,&SceneManager::sceneSwitchBefore,this,&MainWindow::on_sceneSwitchedBefore);
    connect(m_sceneManager,&SceneManager::sceneSwitched,this,&MainWindow::on_sceneSwitched);
    
    // 创建默认场景
    QString defaultSceneName = "场景 1";
    QListWidgetItem* item = new QListWidgetItem(defaultSceneName);
    item->setData(Qt::UserRole, m_nextSceneId);
    ui->sceneListWidget->addItem(item);

    std::shared_ptr<Scene> scene = std::make_shared<Scene>(m_nextSceneId, defaultSceneName);
    m_sceneManager->addScene(scene);
    m_sceneManager->switchScene(m_nextSceneId);
    ui->sceneListWidget->setCurrentRow(0);
    m_nextSceneId++;
}

void MainWindow::initMixer()
{
    m_mixerContainer = new ScrollableContainer(Qt::ScrollBarAsNeeded,Qt::ScrollBarAlwaysOff,Qt::AlignTop,ui->mixerWidget);
    QVBoxLayout *mixLayout = new QVBoxLayout(ui->mixerWidget);
    mixLayout->setContentsMargins(0,0,0,0);
    mixLayout->addWidget(m_mixerContainer);
    int sceneId = m_sceneManager->currentScene()->id();
    int microId = m_audioSourceManager->allocteSourceId();
    // AudioDeviceParams microParams;
    // microParams.deviceName = "麦克风 (1080P USB Camera-Audio)";
    // microParams.nickName = "麦克风";
    // microParams.channels = 2;
    // microParams.sampleRate = 48000;
    // AudioSource* microSource = new MicrophoneAudioSource(microId,sceneId,microParams,this);
    // AudioItemWidget* microWidget = nullptr;
    // if(microSource->open() == 0)
    // {
    //     qDebug() << "麦克风打开成功！";
    //     m_audioSourceManager->addGlobalSource(microSource);
    //     microWidget = new AudioItemWidget(microSource,this);
    //     m_mixerContainer->addWidget(microWidget);
    //     startAudioThread(microSource,microWidget);
    //     qDebug() << "麦克风打开失败！";
    // }

    int desktopId = m_audioSourceManager->allocteSourceId();
    AudioDeviceParams desktopParams;
    desktopParams.deviceName = "CABLE Output (VB-Audio Virtual Cable)";
    desktopParams.nickName = "桌面音频";
    desktopParams.channels = 2;
    desktopParams.sampleRate = 48000;
    AudioSource* desktopSource = new DesktopAudioSource(desktopId,sceneId,desktopParams,this);
    AudioItemWidget* desktopWidget = nullptr;
    if(desktopSource->open() == 0)
    {
        qDebug() << "桌面音频打开成功！";
        m_audioSourceManager->addGlobalSource(desktopSource);
        desktopWidget = new AudioItemWidget(desktopSource,this);
        m_mixerContainer->addWidget(desktopWidget);
        startAudioThread(desktopSource,desktopWidget);
    }else{
        qDebug() << "桌面音频打开失败！";
    }



    // 同步混音器设置参数
    AudioMixerConfig microConfig,desktopConfig;
    microConfig.sourceId = microId;
    microConfig.nickName = "麦克风";
    microConfig.iconUrl = ":/sources/microphone.png";

    desktopConfig.sourceId = desktopId;
    desktopConfig.nickName = "桌面音频";
    desktopConfig.iconUrl = ":/sources/screen.png";

    //AudioMixerDialog::getInstance()->addAudioItem(microId,microConfig,microWidget);
    AudioMixerDialog::getInstance()->addAudioItem(desktopId,desktopConfig,desktopWidget);


    auto* cancedlHideBtn = ComponentInitializer::getInstance()->createIconButton("取消全部隐藏",this,QIcon());
    QList<QPushButton*> buttons = {cancedlHideBtn};
    auto* popup = ComponentInitializer::getInstance()->createIconPopup(this, buttons);

    connect(cancedlHideBtn, &QPushButton::clicked, this, [=]() {
        foreach (QWidget* widget, m_mixerContainer->allWidgets()) {
            if(AudioItemWidget::isAudioItemWidget(widget)){
                AudioItemWidget *audioWidget = qobject_cast<AudioItemWidget*>(widget);
                audioWidget->setHided(false);
            }
            if (popup->isVisible()) {
                popup->hide();
            }
        }
    });

    connect(ui->mixerToolsBtn, &QPushButton::clicked, this, [=]() {
        if (popup->isVisible()) {
            popup->hide();
            return;
        }

        popup->adjustSize();
        QPoint globalPos = ui->mixerToolsBtn->mapToGlobal(QPoint(0, -popup->height()));
        QRect screenRect = QApplication::primaryScreen()->geometry();
        if (QScreen* screen = QGuiApplication::screenAt(globalPos)) {
            screenRect = screen->geometry();
        }

        int x = qBound(screenRect.left(), globalPos.x(), screenRect.right() - popup->width());
        int y = qBound(screenRect.top(), globalPos.y(), screenRect.bottom() - popup->height());

        popup->move(x, y);
        popup->show();
    });

}

void MainWindow::initConfig()
{
    streamConfig_.streamUrl = "rtmp://192.168.48.128:1935/live/stream_key";
    streamConfig_.filePath = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    int width = streamConfig_.vEnConfig.width;
    int height = streamConfig_.vEnConfig.height;
    double fps = streamConfig_.vEnConfig.framerate;
    ui->openGLWidget->setVideoConfig(width,height,fps);
}

void MainWindow::initStatusBar()
{

    StatusBarManager::getInstance().showMessage("系统初始化成功！",MessageType::Success,3000);
}

void MainWindow::initToolTip()
{
    qApp->setStyleSheet(qApp->styleSheet() + QStringLiteral(
                            "QToolTip {"
                            "background-color: #1E1F29;"
                            "color: white;"
                            "border-radius: 4px;"
                            "padding: 3px 2px;"
                            "font-size: 12px;"
                            "}"
                            ));
}
std::string formatBytes(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = bytes;

    while (size >= 1024 && unitIndex < 4) {
        size /= 1024;
        unitIndex++;
    }

    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];
    return ss.str();
}

// 格式化比特率显示
std::string formatBitrate(uint64_t bytes_per_sec) {
    double bits_per_sec = bytes_per_sec * 8.0;
    const char* units[] = {"bps", "Kbps", "Mbps", "Gbps"};
    int unitIndex = 0;

    while (bits_per_sec >= 1024 && unitIndex < 3) {
        bits_per_sec /= 1024;
        unitIndex++;
    }

    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << bits_per_sec << " " << units[unitIndex];
    return ss.str();
}
void MainWindow::initSysMonitor()
{
    m_sysMonitor = new SystemMonitor();
    m_sysMonitor->setThresholds(
        75.0f,   // CPU高负载阈值(%)
        80.0f,   // 内存高负载阈值(%)
        100      // 网络高负载阈值(Mbps)
        );

    // 启动监测器，设置回调函数处理负载变化
    m_sysMonitor->start(1000, [this](LoadStatus status, const SystemMetrics& metrics) {
        // qDebug() << "\n===== 系统负载更新 =====";
        // qDebug() << "时间: " << QDateTime::fromSecsSinceEpoch(std::chrono::system_clock::to_time_t(metrics.timestamp)).toString();

        // // CPU使用率（使用Qt的格式化方式）
        // qDebug() << QString("CPU使用率: %1% (进程: %2%)")
        //                 .arg(metrics.cpu_usage, 0, 'f', 1)
        //                 .arg(metrics.process_cpu_usage, 0, 'f', 1);

        // // 内存使用（转换std::string为QString）
        // qDebug() << QString("内存使用: %1 / %2 (%3%)")
        //                 .arg(QString::fromStdString(formatBytes(metrics.used_memory)))
        //                 .arg(QString::fromStdString(formatBytes(metrics.total_memory)))
        //                 .arg(metrics.memory_usage, 0, 'f', 1);

        // // 进程内存
        // qDebug() << "进程内存: " << QString::fromStdString(formatBytes(metrics.process_memory));

        // // 网络速度
        // qDebug() << QString("网络速度: 发送 %1, 接收 %2")
        //                 .arg(QString::fromStdString(formatBitrate(metrics.sent_speed)))
        //                 .arg(QString::fromStdString(formatBitrate(metrics.recv_speed)));

        // 根据负载状态调整资源池
        switch (status) {
        case LoadStatus::HIGH:
            // qDebug() << "负载状态: 高负载";
            GlobalPool::setFramePoolMaxSize(256);
            GlobalPool::setPacketPoolMaxSize(256);
            break;

        case LoadStatus::LOW:
            // qDebug() << "负载状态: 低负载";
            GlobalPool::setFramePoolMaxSize(64);
            GlobalPool::setPacketPoolMaxSize(64);
            break;

        case LoadStatus::NORMAL:
            // qDebug() << "负载状态: 正常";
            GlobalPool::setFramePoolMaxSize(128);
            GlobalPool::setPacketPoolMaxSize(128);
            break;
        }
    });

    StatisticsDialog::getInstance()->setSystemMonitors(m_sysMonitor);
}

void MainWindow::initTransitionUI()
{
    TransitionManager* transitionMgr = TransitionManager::getInstance();
    QMap<int, QString> transitionList = transitionMgr->getTransitionList();

    ui->transitionTypeCBBox->clear();
    for (auto it = transitionList.begin(); it != transitionList.end(); ++it) {
        ui->transitionTypeCBBox->addItem(it.value(), it.key());  // 文本=转场名，数据=转场ID
    }

    if(!transitionList.isEmpty()){
        ui->transitionTypeCBBox->setCurrentIndex(0);
        transitionMgr->setCurrentTransition(transitionList.firstKey());
    }

    ui->tranDurationSpinBox->setRange(100,3000);
    ui->tranDurationSpinBox->setValue(200);
    transitionMgr->setTransitionDuration(200);

    connect(ui->transitionTypeCBBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int index) {
        qDebug() << "index" << index;
        int transitionId = ui->transitionTypeCBBox->itemData(index).toInt();
        if(transitionId == 2){
            ui->tranDurationSpinBox->setVisible(false);
            ui->tranDurationLabel->setVisible(false);
        }

        else{
            ui->tranDurationSpinBox->setVisible(true);
            ui->tranDurationLabel->setVisible(true);
        }


        transitionMgr->setCurrentTransition(transitionId);
    });

    connect(ui->tranDurationSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [=](int value) {
        transitionMgr->setTransitionDuration(value);
    });


}

void MainWindow::adjustOpenGLWidgetSize()
{
    if (!ui->screenWidget || !ui->openGLWidget)
        return;
    int margin = 10;
    int w = ui->screenWidget->width();
    int h = ui->screenWidget->height() - 2 * margin; // 减去上下留白

    int newWidth = w;
    int newHeight = w * 9 / 16;

    if (newHeight > h) {
        newHeight = h;
        newWidth = h * 16 / 9;
    }

    int x = (w - newWidth) / 2;
    int y = margin + (h - newHeight) / 2; // 顶部留 margin，居中显示

    ui->openGLWidget->setGeometry(x, y, newWidth, newHeight);
}

void MainWindow::startVideoThread(VideoSource* source)
{
    VSourceTaskThread* vThread = new VSourceTaskThread(source,this);
    int sceneId = m_sceneManager->currentScene() ->id();
    //TODO 绑定渲染槽函数
    vThread->setOpenGLWidget(ui->openGLWidget);
    vThread->setFPS(streamConfig_.vEnConfig.framerate);
    m_threadPool->addVideoTask(sceneId,source->sourceId(),vThread);
    connect(m_sceneManager,&SceneManager::sceneIdUpdate,vThread,&VSourceTaskThread::onSceneChanged);
    m_threadPool->startVTasks(sceneId);
}

void MainWindow::startAudioThread(AudioSource *source,AudioItemWidget* audioWidget)
{
    ASourceTaskThread* aThread = new ASourceTaskThread(source,this);
    int sceneId = m_sceneManager->currentScene() ->id();
    m_threadPool->addAudioTask(sceneId,source->sourceId(),aThread);
    connect(aThread,&ASourceTaskThread::volumeLevelChanged,audioWidget,&AudioItemWidget::setVolumeLevel);
    connect(audioWidget,&AudioItemWidget::volumeGainChanged,aThread,&ASourceTaskThread::setVolumeGain);
    m_threadPool->startATasks(sceneId);
}


void MainWindow::handleSourceAdd(QWidget *popup, QWidget *sourceWidget, const QString &defaultName, const QIcon &icon,VideoSourceType type)
{
    QString name = ComponentInitializer::getInstance()->requestSourceName(this, m_sceneManager, "创建源", "编辑源名称", defaultName);

    if (!name.isEmpty()) {

        int ret = 0;
        VideoSource* newSource = nullptr;
        std::vector<VideoSource*> videoSources = m_sceneManager->currentScene()->videoSources();
        int sceneId = m_sceneManager->currentScene()->id();
        int newPriority = videoSources.empty() ? 0 : videoSources.back()->priority() + 1;

        switch (type) {
        case VideoSourceType::Camera:{

            CameraSettingDialog cameraDialog(name,this);
            ret = cameraDialog.exec();

            if (ret == QDialog::Accepted) {
                CameraSetting cameraSetting = cameraDialog.getCameraSetting();
                cameraSetting.nickName = name;
                int sourceId = m_sceneManager->currentScene()->allocteSourceId();

                newSource = new CameraSource(sourceId,sceneId,cameraSetting);
                newSource->setPriority(newPriority);
                if(newSource->open() < 0)
                {
                    QString info("摄像头参数错误或被占用！");
                    qDebug() << info;
                    StatusBarManager::getInstance().showMessage(info,MessageType::Warning,3000);
                    delete newSource;
                    return;
                }
                m_sceneManager->currentScene()->addVideoSource(newSource);
                qDebug() << "摄像头打开成功！";

            } else {

            }
            break;
        }
        case VideoSourceType::Desktop:{

            ScreenSettingDialog screenDialog(name,this);
            ret = screenDialog.exec();

            if (ret == QDialog::Accepted) {
                ScreenSetting screenSetting = screenDialog.getCurrentScreenSetting();

                qDebug() << "screenName : " << screenSetting.screenName
                         << "nickName : " << screenSetting.nickName
                         << "resolution : " << screenSetting.resolution
                         << "screenIndex : " << screenSetting.screenIndex
                         << "offset_X : " << screenSetting.offset_X
                         << "offset_Y : " << screenSetting.offset_Y
                         << "enableDrawMouse : " << screenSetting.enaleDrawMouse;

                screenSetting.nickName = name;
                int sourceId = m_sceneManager->currentScene()->allocteSourceId();

                newSource = new DesktopSource(sourceId,sceneId,screenSetting);
                newSource->setPriority(newPriority);
                if(newSource->open() < 0)
                {
                    QString info("桌面采集打开失败！");
                    qDebug() << info;
                    StatusBarManager::getInstance().showMessage(info,MessageType::Warning,3000);
                    delete newSource;
                    return;
                }
                m_sceneManager->currentScene()->addVideoSource(newSource);
                qDebug() << "桌面采集打开成功！";

            } else {

            }
            break;
        }
        case VideoSourceType::Media:{
            FileSettingDialog fileDialog(name,this);
            ret = fileDialog.exec();

            if (ret == QDialog::Accepted) {

                // 参数设置
                FileSetting fileSetting = fileDialog.getFileSetting();
                fileSetting.nickName = name;
                QString filePath = fileSetting.filePath;

                // 流判断
                StreamInfo streamInfo = FFmpegUtils::probeFileStreams(filePath);

                if (!streamInfo.hasVideo && !streamInfo.hasAudio) {
                    qCritical() << "文件无有效音视频流:" << filePath;
                    avformat_close_input(&streamInfo.fmtCtx);
                    break;
                }

                // 视频
                int sourceId = m_sceneManager->currentScene()->allocteSourceId();
                LocalVideoSource* videoSource = nullptr;
                if(streamInfo.hasVideo)
                {
                    newSource = new LocalVideoSource(sourceId,sceneId,fileSetting);
                    newSource->setPriority(newPriority);
                    videoSource = dynamic_cast<LocalVideoSource*>(newSource);
                    videoSource->setFormatContext(streamInfo.fmtCtx);
                    m_sceneManager->currentScene()->addVideoSource(newSource);
                }

                // 音频
                MediaAudioSource* audioSource = nullptr;
                AudioItemWidget* mediaWidget = nullptr;
                if (streamInfo.hasAudio) {
                    AudioDeviceParams audioParams;
                    audioParams.deviceName = filePath;
                    audioParams.nickName = fileSetting.nickName;
                    audioParams.sampleRate = streamConfig_.aEnConfig.sampleRate;
                    audioParams.channels = streamConfig_.aEnConfig.nbChannels;
                    audioSource = new MediaAudioSource(sourceId,sceneId, audioParams, this);
                    audioSource->setFormatContext(streamInfo.fmtCtx);
                    int mediaASourceId = m_audioSourceManager->allocteSourceId();
                    audioSource->setAudioSourceId(mediaASourceId);
                    m_sceneManager->currentScene()->addAudioSource(audioSource);

                    // 创建音频UI组件
                    mediaWidget = new AudioItemWidget(audioSource,this);
                    m_mixerContainer->addWidget(mediaWidget);

                    AudioMixerConfig mediaConfig;
                    mediaConfig.sourceId = mediaASourceId;
                    mediaConfig.nickName = fileSetting.nickName;
                    mediaConfig.iconUrl = ":/sources/video_file.png";
                    AudioMixerDialog::getInstance()->addAudioItem(mediaASourceId,mediaConfig,mediaWidget);
                }

                if(videoSource) videoSource->setMediaASource(audioSource);
                if(audioSource) audioSource->setVieoSource(videoSource);
                // 媒体控制器初始化
                if (!m_mediaController) {
                    m_mediaController = new MediaSourceController(m_threadPool, this);
                }
                m_mediaController->setRenderWidget(ui->openGLWidget);
                m_mediaController->setSceneManager(m_sceneManager);
                bool addSuccess = m_mediaController->addMediaSource(videoSource, audioSource, mediaWidget);
                if (addSuccess) {
                    m_mediaController->play(sourceId);
                    m_mediaController->setRecording(m_isRecording,sourceId);
                }
            }
            break;
        }

        case VideoSourceType::Text:{
            TextSettingDialog textDialog(name,this);
            ret = textDialog.exec();

            if(ret == QDialog::Accepted)
            {
                TextSetting textSetting = textDialog.getTextSetting();
                textSetting.nickName = name;
                int sourceId = m_sceneManager->currentScene()->allocteSourceId();

                newSource = new TextSource(sourceId,sceneId,textSetting);
                newSource->setPriority(newPriority);

                if(newSource->open() < 0)
                {
                    QString info("文本输入源打开失败！");
                    qDebug() << info;
                    StatusBarManager::getInstance().showMessage(info,MessageType::Warning,3000);
                    delete newSource;
                    break;
                }
                m_sceneManager->currentScene()->addVideoSource(newSource);
                qDebug() << "文本输入源打开成功！";

            }
            break;
        }

        default:
            break;
        }

        if (newSource) {
            if(newSource->type() != VideoSourceType::Media)
            {
                startVideoThread(newSource);
            }

            // 更新源列表
            QWidget* sourceItem = ComponentInitializer::getInstance()->createSourceItem(sourceWidget, newSource, icon);
            // 设置属性
            ComponentInitializer::getInstance()->registerSourceItem(newSource->sourceId(),sourceItem);
            setSourceSetting(newSource,sourceItem,icon);
            connect(newSource,&VideoSource::lockStateChanged,ui->openGLWidget,&CudaRenderWidget::setLayerLocked);
            connect(newSource,&VideoSource::visibilityChanged,ui->openGLWidget,&CudaRenderWidget::setLayerVisible);
            connect(newSource,&VideoSource::activeChanged,this,&MainWindow::activeVideoSource);
        }
    }

    popup->hide();
}

void MainWindow::onSceneItemClicked(QListWidgetItem *item)
{

    int sceneId = item->data(Qt::UserRole).toInt();
    std::shared_ptr<Scene> scene = m_sceneManager->findSceneById(sceneId);
    if(!scene) return;
    m_sceneManager->switchScene(sceneId);
    refreshSourceWidget(scene.get());

}

void MainWindow::refreshSourceWidget(Scene* scene,int highlightSourceId)
{
    ComponentInitializer::getInstance()->clearWidget(ui->sourceWidget);
    ComponentInitializer::getInstance()->clearToolsWidget(this);
    ComponentInitializer::getInstance()->clearSourceItem();

    std::vector<VideoSource*> videoSources = scene->videoSources();
    if(videoSources.empty()){
        qDebug() << "videoSource is null";
        return;
    }

    QWidget* toHighlight = nullptr;
    for(auto& source : videoSources)
    {
        if (!source) {
            qDebug() << "source is nullptr!";
            continue;
        }

        QIcon icon;
        switch (source->type()) {
        case VideoSourceType::Camera:
            icon = QIcon(":/sources/camera.png");
            break;
        case VideoSourceType::Desktop:
            icon = QIcon(":/sources/screen.png");
            break;
        case VideoSourceType::Media:
            icon = QIcon(":/sources/video_file.png");
            break;
        case VideoSourceType::Text:
            icon = QIcon(":/sources/text2.png");
            break;
        default:
            break;
        }

        if(!icon.isNull())
        {
            QWidget* sourceItem = ComponentInitializer::getInstance()->createSourceItem(ui->sourceWidget, source, icon);
            ComponentInitializer::getInstance()->registerSourceItem(source->sourceId(),sourceItem);
            setSourceSetting(source,sourceItem,icon);

            if (source->sourceId() == highlightSourceId)
            {
                toHighlight = sourceItem;
            }
        }
    }

    if (toHighlight) {

        auto filter = ComponentInitializer::getInstance()->sourceSelectionFilter();
        filter->setSelected(toHighlight);
        ComponentInitializer::getInstance()->setCurrentSourceItem(toHighlight);
        emit filter->widgetSelected(toHighlight);

    }
}

void MainWindow::clearWidget(QWidget *widget)
{
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(widget->layout());
    if (layout) {
        QLayoutItem* item;
        while ((item = layout->takeAt(0)) != nullptr) {
            QWidget* widget = item->widget();
            if (widget) {
                widget->deleteLater();
            }
            delete item;
        }
    }
}

void MainWindow::setSourceSetting(VideoSource* source,QWidget* sourceItem,QIcon icon)
{
    if(!source || !sourceItem) return;

    sourceItem->setProperty("nickName",source->name());
    sourceItem->setProperty("sourceId",source->sourceId());
    sourceItem->setProperty("sourceIcon",icon);


    switch (source->type()) {
    case VideoSourceType::Camera:{
        CameraSource* cameraSource = dynamic_cast<CameraSource*>(source);
        sourceItem->setProperty("cameraSetting", QVariant::fromValue(cameraSource->cameraSetting()));
        break;
    }
    case VideoSourceType::Desktop:{
        DesktopSource* screenSource = dynamic_cast<DesktopSource*>(source);
        sourceItem->setProperty("screenSetting", QVariant::fromValue(screenSource->screenSetting()));
        break;
    }
    case VideoSourceType::Media:{
        LocalVideoSource* fileSource = dynamic_cast<LocalVideoSource*>(source);
        sourceItem->setProperty("fileSetting", QVariant::fromValue(fileSource->fileSetting()));
        break;
    }
    case VideoSourceType::Text:{
        TextSource* textSource = dynamic_cast<TextSource*>(source);
        sourceItem->setProperty("textSetting",QVariant::fromValue(textSource->textSetting()));
    }
    default:
        break;
    }
}

void MainWindow::handleSourceSetting()
{
    QWidget* currentSourceItem = ComponentInitializer::getInstance()->getCurrentSourceItem();
    if(!currentSourceItem) return;

    int sourceId = currentSourceItem->property("sourceId").toInt();
    int sceneId = m_sceneManager->currentScene()->id();
    QString nickName = currentSourceItem->property("nickName").toString();
    VideoSource* source = m_sceneManager->currentScene()->findVideoSourceById(sourceId);
    // TODO 没有视频源，只有音频源
    int ret = 0;
    switch (source->type()) {
    case VideoSourceType::Camera:{

        CameraSettingDialog cameraDialog(nickName,this);
        if(currentSourceItem->property("cameraSetting").isValid())
        {
            CameraSetting setting = currentSourceItem->property("cameraSetting").value<CameraSetting>();
            cameraDialog.setInitialSetting(setting);
        }

        ret = cameraDialog.exec();
        if(ret == QDialog::Accepted)
        {
            CameraSetting newSetting = cameraDialog.getCameraSetting();
            currentSourceItem->setProperty("cameraSetting", QVariant::fromValue(newSetting));

            // 重新启动摄像头输入源
            CameraSource* cameraSource = dynamic_cast<CameraSource*>(source);
            cameraSource->serSetting(newSetting);
            m_threadPool->removeVTask(sourceId,sceneId);
            if(cameraSource->open() < 0)
            {
                qDebug() << "摄像头打开失败";
                return;
            }
            startVideoThread(cameraSource);
        }
        break;
    }
    case VideoSourceType::Desktop:{
        ScreenSettingDialog screenDialog(nickName,this);
        if(currentSourceItem->property("screenSetting").isValid())
        {
            ScreenSetting setting = currentSourceItem->property("screenSetting").value<ScreenSetting>();
            screenDialog.setInitialSetting(setting);
        }

        ret = screenDialog.exec();
        if(ret == QDialog::Accepted)
        {
            ScreenSetting newSetting = screenDialog.getCurrentScreenSetting();
            currentSourceItem->setProperty("screenSetting",QVariant::fromValue(newSetting));

            // 重新启动显示器输入源
            DesktopSource* desktopSource = dynamic_cast<DesktopSource*>(source);
            desktopSource->serSetting(newSetting);
            m_threadPool->removeVTask(sourceId,sceneId);
            if(desktopSource->open() < 0)
            {
                qDebug() << "显示器打开失败";
                return;
            }
            startVideoThread(desktopSource);
        }
        break;
    }
    case VideoSourceType::Media:{
        FileSettingDialog fileDialog(nickName,this);
        if(currentSourceItem->property("fileSetting").isValid())
        {
            FileSetting setting = currentSourceItem->property("fileSetting").value<FileSetting>();
            fileDialog.setInitialSetting(setting);
        }
        ret = fileDialog.exec();
        if(ret == QDialog::Accepted)
        {
            FileSetting newSetting = fileDialog.getFileSetting();
            currentSourceItem->setProperty("fileSetting",QVariant::fromValue(newSetting));

            QString newFilePath = newSetting.filePath;
            int newSpeed = newSetting.speed;
            LocalVideoSource* mediaSource = dynamic_cast<LocalVideoSource*>(source);
            QString oldFilePath = mediaSource->fileSetting().filePath;
            int oldSpeed = mediaSource->fileSetting().speed;

            if (newFilePath.isEmpty()) break;

            if(newFilePath != oldFilePath)
            {
                // 探测新文件流信息
                StreamInfo streamInfo = FFmpegUtils::probeFileStreams(newFilePath);
                if (!streamInfo.hasVideo && !streamInfo.hasAudio) {
                    qCritical() << "新文件无有效音视频流:" << newFilePath;
                    avformat_close_input(&streamInfo.fmtCtx);
                    break;
                }

                // 移除旧媒体源
                m_mediaController->removeMediaSource(sourceId);
                AudioSource* aSource = m_sceneManager->currentScene()->findAudioSourceById(sourceId);
                VideoSource* vSource = m_sceneManager->currentScene()->findVideoSourceById(sourceId);
                if(aSource)
                {
                    MediaAudioSource* mediaSource = dynamic_cast<MediaAudioSource*>(aSource);
                    AudioMixerDialog::getInstance()->removeAudioItem(mediaSource->AudioSourceId());
                    m_sceneManager->currentScene()->removeAudioSource(sourceId);
                }

                if(vSource)
                    m_sceneManager->currentScene()->removeVideoSource(sourceId);



                // 创建新的音视频源
                LocalVideoSource* newVideoSource = nullptr;
                MediaAudioSource* newAudioSource = nullptr;
                AudioItemWidget* newAudioWidget = nullptr;

                // 视频源
                if (streamInfo.hasVideo) {
                    newVideoSource = new LocalVideoSource(sourceId,sceneId, newSetting);
                    newVideoSource->setFormatContext(streamInfo.fmtCtx);
                    m_sceneManager->currentScene()->addVideoSource(newVideoSource);
                }

                // 音频源及UI
                if (streamInfo.hasAudio) {
                    AudioDeviceParams audioParams;
                    audioParams.deviceName = newFilePath;
                    audioParams.nickName = newSetting.nickName;
                    audioParams.sampleRate = streamConfig_.aEnConfig.sampleRate;
                    audioParams.channels = streamConfig_.aEnConfig.nbChannels;

                    int mediaASourceId = m_audioSourceManager->allocteSourceId();
                    newAudioSource = new MediaAudioSource(sourceId,sceneId, audioParams, this);
                    newAudioSource->setFormatContext(streamInfo.fmtCtx);
                    newAudioSource->setAudioSourceId(mediaASourceId);
                    m_sceneManager->currentScene()->addAudioSource(newAudioSource);

                    newAudioWidget = new AudioItemWidget(newAudioSource,this);
                    m_mixerContainer->addWidget(newAudioWidget);

                    AudioMixerConfig mediaConfig;

                    mediaConfig.sourceId = mediaASourceId;
                    mediaConfig.nickName = newSetting.nickName;
                    mediaConfig.iconUrl = ":/sources/video_file.png";
                    AudioMixerDialog::getInstance()->addAudioItem(mediaASourceId,mediaConfig,newAudioWidget);

                }

                // 关联音视频源
                if (newVideoSource) newVideoSource->setMediaASource(newAudioSource);
                if (newAudioSource) newAudioSource->setVieoSource(newVideoSource);

                // 更新媒体控制器
                if (!m_mediaController) {
                    m_mediaController = new MediaSourceController(m_threadPool, this);
                }
                m_mediaController->setRenderWidget(ui->openGLWidget);
                m_mediaController->setSceneManager(m_sceneManager);
                bool addSuccess = m_mediaController->addMediaSource(newVideoSource, newAudioSource, newAudioWidget);
                if (addSuccess) {
                    // 更新UI属性
                    currentSourceItem->setProperty("fileSetting", QVariant::fromValue(newSetting));
                    currentSourceItem->setProperty("nickName", newSetting.nickName);
                    // 保持原播放状态（如果之前在播放则继续播放）
                    m_mediaController->play(sourceId);
                }
            }else if(newFilePath == oldFilePath && newSpeed != oldSpeed)
            {
                // if(m_mediaController)
                // {
                //     MediaSpeedFilter::Speed targetSpeed;

                //     switch (newSpeed) {
                //     case 5:
                //         targetSpeed = MediaSpeedFilter::Speed::S0_5;
                //         break;
                //     case 10:
                //         targetSpeed = MediaSpeedFilter::Speed::S1_0;
                //         break;
                //     case 15:
                //         targetSpeed = MediaSpeedFilter::Speed::S1_5;
                //         break;
                //     case 20:
                //         targetSpeed = MediaSpeedFilter::Speed::S2_0;
                //         break;
                //     default:
                //         targetSpeed = MediaSpeedFilter::Speed::S1_0;
                //         break;
                //     }
                //     m_mediaController->setSpeed(sourceId,targetSpeed);
                // }
            }

        }
        break;
    }

    case VideoSourceType::Text:{
        TextSettingDialog textDialog(nickName,this);
        TextSource* textSource = dynamic_cast<TextSource*>(source);
        if(currentSourceItem->property("textSetting").isValid())
        {
            // TextSetting setting = currentSourceItem->property("textSetting").value<TextSetting>();

            textDialog.setInitialSetting(textSource->textSetting());
        }

        ret = textDialog.exec();

        if(ret == QDialog::Accepted)
        {
            TextSetting newSetting = textDialog.getTextSetting();
            currentSourceItem->setProperty("textSetting",QVariant::fromValue(newSetting));
            textSource->setTextSetting(newSetting);
        }
    }
    default:
        break;
    }

}

void MainWindow::activeVideoSource(int sourceId)
{
    VideoSource* source = m_sceneManager->currentScene()->findVideoSourceById(sourceId);
    int sceneId = m_sceneManager->currentScene()->id();
    if(source->isActive())
    {
        if(source->open() < 0)
        {
            qDebug() << "摄像头打开失败！";
            return;
        }
        startVideoThread(source);

    }
    else
    {
        m_threadPool->removeVTask(sourceId,sceneId);

    }
}



void MainWindow::on_addSceneBtn_clicked()
{
    QString sceneName = ComponentInitializer::getInstance()->showCreateSceneDialog(this);
    if(sceneName.isNull()) return;

    qDebug() << sceneName;
    QListWidgetItem* item = new QListWidgetItem(sceneName);
    item->setData(Qt::UserRole,m_nextSceneId);
    ui->sceneListWidget->addItem(item);

    auto scene = std::make_shared<Scene>(m_nextSceneId, sceneName);
    m_sceneManager->addScene(scene);
    m_sceneManager->switchScene(m_nextSceneId);

    ui->sceneListWidget->setCurrentRow(ui->sceneListWidget->count() - 1);
    ComponentInitializer::getInstance()->clearWidget(ui->sourceWidget);
    ComponentInitializer::getInstance()->clearToolsWidget(this);
    m_nextSceneId++;
}


void MainWindow::on_removeSceneBtn_clicked()
{
    QListWidgetItem* currentSceneItem = ui->sceneListWidget->currentItem();
    if(!currentSceneItem) return;

    // 检查是否只有一个场景，如果是则不允许删除
    if(ui->sceneListWidget->count() <= 1)
    {
        ComponentInitializer::getInstance()->showMessageBox(this,"无法删除","至少需要保留一个场景",QMessageBox::Information);
        return;
    }

    QString msg = "确定要删除 " + currentSceneItem->text() + " 吗?";
    bool isRemove = ComponentInitializer::getInstance()->showMessageBox(this,"确认删除",msg,QMessageBox::Information,true);
    if(isRemove)
    {
        // int sceneId = currentSceneItem->data(Qt::UserRole).toInt();
        int sceneId = m_sceneManager->currentScene()->id();
        m_threadPool->stopVTasks(sceneId);
        if(m_mediaController) m_mediaController->removeMediaSource(-1,sceneId);
        std::shared_ptr<Scene> sceneToRemove = m_sceneManager->findSceneById(sceneId);
        if (!sceneToRemove) return;

        // 先切换到新场景
        if(ui->sceneListWidget->count() > 1)
        {
            QListWidgetItem* firstItem = ui->sceneListWidget->item(0);
            int firstSceneId = firstItem->data(Qt::UserRole).toInt();
            m_sceneManager->switchScene(firstSceneId); // 此时会触发sceneSwitchBefore/After，oldScene是有效的sceneToRemove
            ui->sceneListWidget->setCurrentItem(firstItem);
        }

        // 再清理并删除要移除的场景
        sceneToRemove->clearVideoSource();
        sceneToRemove->clearAudioSource();
        m_sceneManager->removeSceneById(sceneId); // 这里才真正删除场景

        // 后续UI清理
        ui->sceneListWidget->removeItemWidget(currentSceneItem);
        delete ui->sceneListWidget->takeItem(ui->sceneListWidget->row(currentSceneItem));
        ComponentInitializer::getInstance()->clearWidget(ui->sourceWidget);
        ComponentInitializer::getInstance()->clearToolsWidget(this);
        ComponentInitializer::getInstance()->clearSourceItem();
        ComponentInitializer::getInstance()->clear();
        AudioMixerDialog::getInstance()->clearAMediaItem();
    }
}


void MainWindow::on_settingSourceBtn_clicked()
{
    handleSourceSetting();
}


void MainWindow::on_toolSettingBtn_clicked()
{
    handleSourceSetting();
}


void MainWindow::on_removeSourceBtn_clicked()
{
    QWidget* currentSourceItem = ComponentInitializer::getInstance()->getCurrentSourceItem();
    std::shared_ptr<Scene> currentScene = m_sceneManager->currentScene();
    if(!currentSourceItem || !currentScene) return;

    int sourceId = currentSourceItem->property("sourceId").toInt();
    QString nickName = currentSourceItem->property("nickName").toString();

    QString msg = "确定要删除 " + nickName + " 吗?";
    bool isRemove = ComponentInitializer::getInstance()->showMessageBox(this,"确认删除",msg,QMessageBox::Information,true);
    if(isRemove)
    {
        VideoSource* vSource = currentScene->findVideoSourceById(sourceId);
        AudioSource* aSource = currentScene->findAudioSourceById(sourceId);

        if(aSource)
        {
            m_mediaController->removeMediaSource(sourceId,currentScene->id());
            MediaAudioSource* mediaSource = dynamic_cast<MediaAudioSource*>(aSource);
            m_mixerContainer->removeWidget(AudioMixerDialog::getInstance()->getAudioItemWidget(mediaSource->AudioSourceId()));
            AudioMixerDialog::getInstance()->removeAudioItem(mediaSource->AudioSourceId());
            currentScene->removeAudioSource(sourceId);
        }

        if(vSource)
        {
            if(vSource->type() == VideoSourceType::Media)
            {
                m_mediaController->removeMediaSource(sourceId);
                currentScene->removeVideoSource(sourceId);
                ComponentInitializer::getInstance()->removeControlBarCaches(sourceId);
            }else
            {
                m_threadPool->removeVTask(sourceId,currentScene->id());
                currentScene->removeVideoSource(sourceId);
            }
        }

        ComponentInitializer::getInstance()->removeSourceItem(ui->sourceWidget,currentSourceItem);
        ComponentInitializer::getInstance()->clearToolsWidget(this);
        ComponentInitializer::getInstance()->unregisterSourceItem(sourceId);
    }
}


void MainWindow::on_upSourceBtn_clicked()
{
    QWidget* currentSourceItem = ComponentInitializer::getInstance()->getCurrentSourceItem();
    if(!currentSourceItem) return;

    int sourceId = currentSourceItem->property("sourceId").toInt();
    m_sceneManager->currentScene()->moveSourceUp(sourceId);

    refreshSourceWidget(m_sceneManager->currentScene().get(),sourceId);

}


void MainWindow::on_downSourceBtn_clicked()
{
    QWidget* currentSourceItem = ComponentInitializer::getInstance()->getCurrentSourceItem();
    if(!currentSourceItem) return;

    int sourceId = currentSourceItem->property("sourceId").toInt();
    m_sceneManager->currentScene()->moveSourceDown(sourceId);

    refreshSourceWidget(m_sceneManager->currentScene().get(),sourceId);
}

void MainWindow::onLayerClicked(int sourceId)
{
    QWidget* item = ComponentInitializer::getInstance()->getSourceItemById(sourceId);
    if (!item) return;
    auto filter = ComponentInitializer::getInstance()->sourceSelectionFilter();
    filter->setSelected(item);
    ComponentInitializer::getInstance()->setCurrentSourceItem(item);
    emit filter->widgetSelected(item);
}


void MainWindow::on_startBtn_clicked()
{
    if (!m_isRecording) {

        if(!streamConfig_.enableStream && !streamConfig_.enableRecord)
        {
            ComponentInitializer::getInstance()->showMessageBox(this,"警告","请选择工作模式(推流/录制)",QMessageBox::Warning);
            return;
        }

        if(streamConfig_.enableStream && streamConfig_.streamUrl.isEmpty())
        {
            ComponentInitializer::getInstance()->showMessageBox(this,"警告","请设置推流地址",QMessageBox::Warning);
            return;
        }

        if(!m_sceneManager->currentScene())
        {
            ComponentInitializer::getInstance()->showMessageBox(this,"警告","请先添加一个场景",QMessageBox::Warning);
            return;
        }

        if(m_sceneManager->currentScene()->videoSources().empty())
        {
            bool isContinue = ComponentInitializer::getInstance()->showMessageBox(this,"无视频源","当前未打开任何视频源，所以只能输出黑色画面。您确定要这样做吗？ (建议先添加视频源)",QMessageBox::Information,true);
            if(!isContinue) return;
        }

        if(streamConfig_.enableStream){
            NetworkMonitorResult result = m_tcpMonitor->testTcpConnectivity("192.168.48.128",1935);
            if(!result.isSuccess){
                qDebug() << "result error:" << result.errorMsg;
                StatusBarManager::getInstance().showMessage(QString("服务器未连接！"),MessageType::Warning,3000);
                return;
            }
            m_networkMonitor->setMonitorParams(MonitorType::ZLMEDIAKIT,"192.168.48.128",80,"stream_key");
            m_tcpMonitor->setMonitorParams(MonitorType::TCP_CONNECT,"192.168.48.128",1935,"",3, 5000);
            connect(m_networkMonitor,&NetworkMonitor::monitorRealTimeResult,&StatusBarManager::getInstance(),&StatusBarManager::updateSteamInfo);
            connect(m_tcpMonitor,&NetworkMonitor::monitorRealTimeResult,&StatusBarManager::getInstance(),&StatusBarManager::updateNetworkSatus);
            StatusBarManager::getInstance().showNetWorkLabel();
            m_networkMonitor->start(0, 3000); // 网络监测：无限次，每3秒一次
            m_tcpMonitor->start(0,2000);
        }

        // 计时器UI更新
        if (!m_updateTimer) {
            m_updateTimer = new QTimer(this);
            connect(m_updateTimer, &QTimer::timeout, this, &MainWindow::updateRecordTimerUI);
        }

        debugStreamConfig(streamConfig_);

        // 初始化并打开流控制器
        if(!m_streamController) {
            m_streamController = new StreamController(m_threadPool,ui->openGLWidget,m_networkMonitor,this);
            connect(m_tcpMonitor, &NetworkMonitor::serverDisconnected, this, [this](MonitorType type, const QString& errorMsg) {
                m_trayIcon.showMessage("连接断开","服务器已断开，请检查网络",QSystemTrayIcon::Warning,3000);
                qDebug() << "[推流停止] 服务器断开（类型：" << static_cast<int>(type) << "），原因：" << errorMsg;
                on_StreamPushingDisconnected();
            });
            connect(m_tcpMonitor, &NetworkMonitor::tcpReconnected, this, [this](const QString& ip, quint16 port) {
                m_trayIcon.showMessage("连接成功","服务器已连接",QSystemTrayIcon::Information,3000);
                qDebug() << "[推流恢复] TCP重连成功，准备恢复推流（IP：" << ip << "，端口：" << port << "）";
                on_StreamPushingReconnected();
            });
        }
        m_streamController->init(streamConfig_);
        if(!m_streamController->start()) return;
        m_threadPool->startAddAudioFrame();     // TODO 录制时，暂时录制所有场景下的音频
        if(m_mediaController) m_mediaController->setRecording(true);
        ui->openGLWidget->setVideoConfig(streamConfig_.vEnConfig.width,streamConfig_.vEnConfig.height,streamConfig_.vEnConfig.framerate);
        ui->openGLWidget->setRecording(true);

        // 初次点击：开始录制
        m_isRecording = true;
        m_isPaused = false;
        m_pausedDuration = 0;
        m_startTimer.start();
        m_updateTimer->start(200);

        connect(ui->openGLWidget, &CudaRenderWidget::frameRecorded,
                m_streamController, &StreamController::onNewFrameAvailable,
                Qt::QueuedConnection);

        // 更新statusBar
        if(streamConfig_.enableStream && !streamConfig_.enableRecord)
        {
            StatusBarManager::getInstance().updateStreamStatus(StreamStatus::Streaming);
            ui->startBtn->setIcon(QIcon(":/sources/pause_grey.png"));
            ui->startBtn->setDisabled(true);
        }else if(streamConfig_.enableRecord && !streamConfig_.enableStream)
        {
            StatusBarManager::getInstance().updateStreamStatus(StreamStatus::Recording);
            ui->startBtn->setIcon(QIcon(":/sources/pause.png"));
        }else
        {
            StatusBarManager::getInstance().updateStreamStatus(StreamStatus::Both);
            ui->startBtn->setIcon(QIcon(":/sources/pause_grey.png"));
            ui->startBtn->setDisabled(true);
        }

    }
    else if (!m_isPaused) {
        if(streamConfig_.enableStream) return;
        // 正在录制 → 点击暂停
        m_isPaused = true;
        m_pauseStartTime = m_startTimer.elapsed();
        m_threadPool->stopAddAudioFrame();      // TODO 录制时，暂时停止所有场景下的音频
        m_streamController->pause();
        ui->startBtn->setIcon(QIcon(":/sources/play.png")); // 切换为开始图标
        if(streamConfig_.enableRecord && !streamConfig_.enableStream)
        {
            StatusBarManager::getInstance().updateStreamStatus(StreamStatus::RecPausedNOStr);
        }

    }
    else {
        if(streamConfig_.enableStream) return;
        // 当前暂停中 → 点击恢复
        m_isPaused = false;
        qint64 now = m_startTimer.elapsed();
        m_pausedDuration += (now - m_pauseStartTime);
        m_pauseStartTime = 0;
        m_threadPool->startAddAudioFrame();     // TODO 录制时，暂时录制所有场景下的音频
        m_streamController->resume();
        ui->startBtn->setIcon(QIcon(":/sources/pause.png")); // 切换为暂停图标
        if(streamConfig_.enableRecord && !streamConfig_.enableStream)
        {
            StatusBarManager::getInstance().updateStreamStatus(StreamStatus::Recording);
        }
    }
}


void MainWindow::on_stopBtn_clicked()
{
    m_isRecording = false;
    m_isPaused = false;
    m_pausedDuration = 0;
    m_pauseStartTime = 0;

    if (m_updateTimer)
        m_updateTimer->stop();

    m_streamController->stop();
    if (m_networkMonitor) m_networkMonitor->stop();
    if(m_tcpMonitor) m_tcpMonitor->stop();
    m_threadPool->stopAddAudioFrame();      // TODO 录制时，暂时停止所有场景下的音频
    if(m_mediaController) m_mediaController->setRecording(false);
    ui->openGLWidget->setRecording(false);
    ui->timerLabel->setText("00:00:00");
    ui->startBtn->setIcon(QIcon(":/sources/play.png"));
    ui->startBtn->setDisabled(false);
    StatusBarManager::getInstance().updateStreamStatus(StreamStatus::Stopped);
    StatusBarManager::getInstance().hideNetWorkLabel();
}


void MainWindow::on_StreamPushingReconnected()
{
    m_streamController->init(streamConfig_);
    if(!m_streamController->start()) return;
    m_threadPool->startAddAudioFrame();     // TODO 录制时，暂时录制所有场景下的音频
    if(m_mediaController) m_mediaController->setRecording(true);
    ui->openGLWidget->setVideoConfig(streamConfig_.vEnConfig.width,streamConfig_.vEnConfig.height,streamConfig_.vEnConfig.framerate);
    ui->openGLWidget->setRecording(true);

    connect(ui->openGLWidget, &CudaRenderWidget::frameRecorded,m_streamController, &StreamController::onNewFrameAvailable,Qt::QueuedConnection);
}

void MainWindow::on_StreamPushingDisconnected()
{
    m_streamController->stop();
    m_threadPool->stopAddAudioFrame();
    if(m_mediaController) m_mediaController->setRecording(false);
    ui->openGLWidget->setRecording(false);

}

void MainWindow::on_StreamPushingClosed()
{
    if (m_networkMonitor) m_networkMonitor->stop();
    if(m_tcpMonitor) m_tcpMonitor->stop();

    m_isRecording = false;
    m_isPaused = false;
    m_pausedDuration = 0;
    m_pauseStartTime = 0;

    if (m_updateTimer)
        m_updateTimer->stop();

    ui->timerLabel->setText("00:00:00");
    ui->startBtn->setIcon(QIcon(":/sources/play.png"));
    ui->startBtn->setDisabled(false);
    StatusBarManager::getInstance().updateStreamStatus(StreamStatus::Stopped);StatusBarManager::getInstance().hideNetWorkLabel();
    StatusBarManager::getInstance().hideNetWorkLabel();
}
void MainWindow::on_settingBtn_clicked()
{
    outputSettingDialog dialog(this);
    dialog.init(streamConfig_);

    int ret = dialog.exec();
    if(ret == QDialog::Accepted)
    {
        streamConfig_ = dialog.getStreamConfig();
        StatusBarManager::getInstance().setMaxFPS(streamConfig_.vEnConfig.framerate);
        m_threadPool->setVFps(streamConfig_.vEnConfig.framerate);       // TODO 录制时，暂时同步设置所有场景FPS
        ui->openGLWidget->setVideoConfig(streamConfig_.vEnConfig.width,streamConfig_.vEnConfig.height,streamConfig_.vEnConfig.framerate);
    }
}


void MainWindow::on_recordCheckBox_checkStateChanged(const Qt::CheckState &arg1)
{
    if(arg1 == Qt::Checked)
    {
        streamConfig_.enableRecord = true;
    }else if(arg1 == Qt::Unchecked)
    {
        streamConfig_.enableRecord = false;
    }
}


void MainWindow::on_pushCheckBox_checkStateChanged(const Qt::CheckState &arg1)
{

    if(arg1 == Qt::Checked)
    {
        streamConfig_.enableStream = true;
    }else if(arg1 == Qt::Unchecked)
    {
        streamConfig_.enableStream = false;
    }
}

void MainWindow::on_screenComboBox_changed(int index)
{
    QComboBox* comboBox = qobject_cast<QComboBox*>(sender());
    if (!comboBox) return;

    int sourceId = comboBox->property("sourceId").toInt();

    int sceneId = m_sceneManager->currentScene()->id();
    VideoSource* source = m_sceneManager->currentScene()->findVideoSourceById(sourceId);
    if (!source || source->type() != VideoSourceType::Desktop) return;

    DesktopSource* desktopSource = dynamic_cast<DesktopSource*>(source);
    if(!desktopSource) return;

    int newIndex = comboBox->currentData().toInt();
    ScreenSetting newSetting = desktopSource->screenSetting();
    newSetting.screenIndex = newIndex;

    m_threadPool->removeVTask(sourceId,sceneId);
    desktopSource->serSetting(newSetting);

    if(desktopSource->open() < 0)
    {
        qDebug() << "切换显示器失败：无法打开选中的显示器";
        comboBox->setCurrentIndex(desktopSource->screenSetting().screenIndex);
        return;
    }

    startVideoThread(desktopSource);

    QWidget* sourceItem = ComponentInitializer::getInstance()->getCurrentSourceItem();
    if (sourceItem) {
        sourceItem->setProperty("screenSetting", QVariant::fromValue(newSetting));
    }
}

void MainWindow::printPoolStats()
{
    qDebug() << "===== FramePool Stats =====";
    GlobalPool::getFramePool().printStats();  // 调用全局FramePool的打印方法

    qDebug() << "===== PacketPool Stats =====";
    GlobalPool::getPacketPool().printStats();  // 调用全局PacketPool的打印方法
    qDebug() << "---------------------------\n";
}


void MainWindow::on_mixSettingBtn_clicked()
{
    AudioMixerDialog::getInstance()->initTableItem();
    AudioMixerDialog::getInstance()->show();
}

void MainWindow::on_sceneSwitchedBefore(std::shared_ptr<Scene> oldScene, std::shared_ptr<Scene> newScene)
{
    Q_UNUSED(newScene);
    if (!oldScene) return;

    // 1. 取消激活旧场景的视频源线程
    std::set<int> videoSourceIds = oldScene->videoSourceIds();
    for(int sourceId : videoSourceIds)
    {
        // 暂停媒体源
        if(m_mediaController){
            m_mediaController->pause(sourceId,oldScene->id());
        }

    }
}

void MainWindow::on_sceneSwitched(std::shared_ptr<Scene> newScene)
{

    if (!newScene || !ui->openGLWidget) return;

    std::set<int> videoSourceIds = newScene->videoSourceIds();
    for(int sourceId : videoSourceIds)
    {
        // 开启媒体源
        if(m_mediaController){
            m_mediaController->play(sourceId,newScene->id());
        }
    }

    ui->openGLWidget->update();
}


void MainWindow::on_statisticBtn_clicked()
{
    StatisticsDialog::getInstance()->show();
}


void MainWindow::debugStreamConfig(const StreamConfig& config) {
    qDebug() << "StreamConfig:";
    qDebug() << "  enableRecord:" << config.enableRecord;
    qDebug() << "  enableStream:" << config.enableStream;
    qDebug() << "  filePath:" << config.filePath;
    qDebug() << "  streamUrl:" << config.streamUrl;

    qDebug() << "  videoEncodeConfig:";
    qDebug() << "    width:" << config.vEnConfig.width;
    qDebug() << "    height:" << config.vEnConfig.height;
    qDebug() << "    bitrate:" << config.vEnConfig.bitrate;
    qDebug() << "    framerate:" << config.vEnConfig.framerate;
    qDebug() << "    gop_size:" << config.vEnConfig.gop_size;
    qDebug() << "    format:" << config.vEnConfig.format;
    qDebug() << "    max_b_frames:" << config.vEnConfig.max_b_frames;

    qDebug() << "  AudioEncodeConfig:";
    qDebug() << "    sampleRate:" << config.aEnConfig.sampleRate;
    qDebug() << "    nbChannels:" << config.aEnConfig.nbChannels;
    qDebug() << "    bitRate:" << config.aEnConfig.bitRate;

    // 处理枚举类型（假设AVSampleFormat可以直接输出）
    qDebug() << "    sampleFmt:" << config.aEnConfig.sampleFmt;
}

bool MainWindow::addAudioItemToMixerLayout(AudioItemWidget* audioItem)
{
    if (!audioItem) {
        qDebug() << "AudioItemWidget为空，无法添加到混音器";
        return false;
    }

    // 查找混音器中的滚动区域
    QScrollArea* scrollArea = ui->mixerWidget->findChild<QScrollArea*>();
    if (!scrollArea) {
        qDebug() << "未找到混音器的滚动区域";
        return false;
    }

    // 查找滚动区域内部的内容部件
    QWidget* scrollContent = scrollArea->widget();
    if (!scrollContent) {
        qDebug() << "滚动区域内容部件不存在";
        return false;
    }

    // 查找内容部件中的垂直布局
    QVBoxLayout* mixerLayout = qobject_cast<QVBoxLayout*>(scrollContent->layout());
    if (!mixerLayout) {
        qDebug() << "混音器内部布局不存在或不是QVBoxLayout";
        return false;
    }

    // 将音频项部件添加到布局
    int insertIndex = mixerLayout->count() - 1;
    mixerLayout->insertWidget(insertIndex, audioItem);


    // if (audioItem->audioSource()) { // 假设AudioItemWidget有获取音频源的方法
    //     audioItem->setProperty("sourceId", audioItem->audioSource()->sourceId());
    // }

    return true;
}

void MainWindow::showDisconnectNotify(QWidget *parent)
{
    m_trayIcon.showMessage(
        "连接断开",
        "服务器已断开，请检查网络",
        QSystemTrayIcon::Warning,
        3000);
}

void MainWindow::updateRecordTimerUI()
{
    if (!m_isRecording) return;

    qint64 rawElapsed = m_startTimer.elapsed();
    qint64 elapsed;

    if (m_isPaused) {
        elapsed = m_pauseStartTime - m_pausedDuration;
    } else {
        elapsed = rawElapsed - m_pausedDuration;
    }

    int seconds = (elapsed / 1000) % 60;
    int minutes = (elapsed / 1000) / 60;
    int hours = minutes / 60;
    minutes %= 60;

    QString timeStr = QString("%1:%2:%3")
                          .arg(hours, 2, 10, QLatin1Char('0'))
                          .arg(minutes, 2, 10, QLatin1Char('0'))
                          .arg(seconds, 2, 10, QLatin1Char('0'));

    ui->timerLabel->setText(timeStr);
}
