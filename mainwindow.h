#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "component/componentinitializer.h"
#include "component/outputsettingdialog.h"
#include "component/scrollablecontainer.h"
#include "source/video/videosource.h"
#include "source/video/localvideosource.h"
#include "source/video/camerasource.h"
#include "source/video/desktopsource.h"
#include "source/audio/audiosource.h"
#include "scene/audiosourcemanager.h"
#include "controller/mediasourcecontroller.h"
#include "monitor/systemmonitor.h"
#include "monitor/networkmonitor.h"
#include "thread/threadpool.h"
#include "controlbar.h"
#include <QMainWindow>
#include <QSystemTrayIcon>
#include <map>
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE


class SceneManager;
class Scene;
class AudioItemWidget;
struct LayerInfo;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    ControlBar* getControlBar();
    MediaSourceController* getMediaSourceController();

protected:
    void resizeEvent(QResizeEvent* event) override;

public slots:
    void on_screenComboBox_changed(int index);

private slots:
    void printPoolStats();
    void on_addSceneBtn_clicked();
    void on_removeSceneBtn_clicked();
    void on_settingSourceBtn_clicked();
    void on_toolSettingBtn_clicked();
    void on_removeSourceBtn_clicked();
    void on_upSourceBtn_clicked();
    void on_downSourceBtn_clicked();
    void onLayerClicked(int sourceId);
    void on_startBtn_clicked();
    void updateRecordTimerUI();
    void on_stopBtn_clicked();
    void on_settingBtn_clicked();
    void on_recordCheckBox_checkStateChanged(const Qt::CheckState &arg1);
    void on_pushCheckBox_checkStateChanged(const Qt::CheckState &arg1);
    void on_mixSettingBtn_clicked();
    void on_sceneSwitchedBefore(std::shared_ptr<Scene> oldScene,std::shared_ptr<Scene> newScene);
    void on_sceneSwitched(std::shared_ptr<Scene> newScene);

    void on_statisticBtn_clicked();
    void on_StreamPushingReconnected();
    void on_StreamPushingDisconnected();
    void on_StreamPushingClosed();
private:
    // 初始化UI
    void initUI();
    void initAddSourceButton();
    void initScenes();
    void initMixer();
    void initConfig();
    void initStatusBar();
    void initToolTip();
    void initSysMonitor();
    void initTransitionUI();
    void adjustOpenGLWidgetSize();
    void startVideoThread(VideoSource* source);
    void startAudioThread(AudioSource* source,AudioItemWidget* audioWidget);

    void handleSourceAdd(QWidget* popup,QWidget* sourceWidget,const QString& defaultName,const QIcon& icon,VideoSourceType type);
    void onSceneItemClicked(QListWidgetItem* item);
    void refreshSourceWidget(Scene* scene,int highlightSourceId = -1);
    void clearWidget(QWidget* widget);
    void setSourceSetting(VideoSource* source,QWidget* sourceItem,QIcon icon);
    void handleSourceSetting();
    void activeVideoSource(int sourceId);
    void debugStreamConfig(const StreamConfig& config);
    bool addAudioItemToMixerLayout(AudioItemWidget* audioItem);
    void showDisconnectNotify(QWidget *parent);
private:
    Ui::MainWindow *ui;
    SceneManager* m_sceneManager = nullptr;
    AudioSourceManager* m_audioSourceManager = nullptr;
    StreamController* m_streamController = nullptr;
    MediaSourceController* m_mediaController = nullptr;
    ControlBar* m_controlBar = nullptr;
    ScrollableContainer* m_mixerContainer = nullptr;
    SystemMonitor* m_sysMonitor = nullptr;
    NetworkMonitor* m_networkMonitor = nullptr;
    NetworkMonitor* m_tcpMonitor = nullptr;
    QSystemTrayIcon m_trayIcon{this};
    ThreadPool* m_threadPool = nullptr;
    int m_nextSceneId = 1;

    QElapsedTimer m_startTimer;        // 记录录制开始时间
    qint64 m_pausedDuration = 0;       // 所有暂停时长累计（毫秒）
    qint64 m_pauseStartTime = 0;       // 当前暂停起始时间点（毫秒）

    QTimer* m_updateTimer = nullptr;   // 用于刷新 UI 时间显示
    bool m_isRecording = false;
    bool m_isPaused = false;

    StreamConfig streamConfig_;
    bool isConfigInitialed = false;
};
#endif // MAINWINDOW_H
