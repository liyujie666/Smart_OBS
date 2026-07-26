#ifndef STATUSBARMANAGER_H
#define STATUSBARMANAGER_H

#include "fpscounter.h"
#include "cpumonitor.h"
#include "monitor/netmonitor.h"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QThread>

enum class MessageType {
    Info,
    Warning,
    Error,
    Success,
    Debug
};

enum class StreamStatus {
    Stopped,
    RecPausedNOStr,
    RecPausedStr,
    Recording,
    Streaming,
    Both
};

class StatusBarManager : public QObject
{
    Q_OBJECT
public:
    static StatusBarManager& getInstance();
    ~StatusBarManager();
    StatusBarManager(const StatusBarManager&) = delete;
    StatusBarManager& operator=(const StatusBarManager&) = delete;

    void showMessage(const QString& text, MessageType type = MessageType::Info,
                     int durationMs = 5000);
    void updateStreamStatus(StreamStatus status);
    void bindFPSCounter(FPSCounter* fpsCounter);
    void bindNetMonitor(NetMonitor* netMonitor);
    void setIsPushing(bool isPush);

    double cpuUsage() const { return m_cpuUsage; }
    double frameRate() const { return m_frameRate; }
    QString recordText() const { return m_recordText; }
    QString streamText() const { return m_streamText; }
    void setMaxFPS(int maxFps);
    int getMaxFPS() const { return m_maxFPS; }
    void release();

signals:
    void statusUpdated(const QString& text, MessageType type, bool showMessage);
    void statusInfoUpdated(const QString& recordStatus, const QString& streamStatus);
    void cpuInfoUpdated(double cpuUsage);
    void fpsInfoUpdated(double frameRate);
    void streamNetworkInfoUpdated(const NetworkStats& stats);
    void networkStatusUpdated(NetworkStatus status);
    void showNetWorkLabel();
    void hideNetWorkLabel();

public slots:
    void updateFrameRate(double fps);
    void updateCpuUsage(double usage);
    void updateNetworkInfo(const NetworkStats& stats);

private slots:
    void onMessageTimeout();

private:
    explicit StatusBarManager(QObject* parent = nullptr);
    void updateStatusText();
    NetworkStatus getNetworkStatus(const NetworkStats& stats) const;

    StreamStatus m_streamStatus = StreamStatus::Stopped;
    double m_cpuUsage = 0.0;
    double m_frameRate = 30.0;
    QString m_currentMessage;
    MessageType m_currentMsgType = MessageType::Info;
    QTimer* m_msgTimer = nullptr;
    QString m_recordText;
    QString m_streamText;
    int m_maxFPS = 30;
    CpuMonitor* cpuMonitor_ = nullptr;
    QThread cpuThread_;
    NetMonitor* netMonitor_ = nullptr;
};

#endif // STATUSBARMANAGER_H
