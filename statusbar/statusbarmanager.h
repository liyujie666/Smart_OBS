#ifndef STATUSBARMANAGER_H
#define STATUSBARMANAGER_H
#include "fpscounter.h"
#include "cpumonitor.h"
#include "monitor/networkmonitor.h"
#include <QObject>
#include <QString>
#include <QTimer>
#include <QDateTime>
#include <QThread>

enum class NetworkStatus;
struct NetworkMonitorResult;
// 消息类型枚举
enum class MessageType {
    Info,       // 普通信息
    Warning,    // 警告信息
    Error,      // 错误信息
    Success,     // 成功信息
    Debug
};

// 推流/录制状态枚举
enum class StreamStatus {
    Stopped,    // 已停止
    RecPausedNOStr,     // 已暂停（未推流）
    RecPausedStr,       // 已暂停（推流中）
    Recording,  // 录制中
    Streaming,  // 推流中
    Both        // 录制+推流中
};

class StatusBarManager : public QObject
{
    Q_OBJECT
public:
    // 获取单例实例
    static StatusBarManager& getInstance();
    ~StatusBarManager();
    // 禁止复制和移动
    StatusBarManager(const StatusBarManager&) = delete;
    StatusBarManager& operator=(const StatusBarManager&) = delete;
    StatusBarManager(StatusBarManager&&) = delete;
    StatusBarManager& operator=(StatusBarManager&&) = delete;

    // 显示消息的方法
    void showMessage(const QString& text,
                     MessageType type = MessageType::Info,
                     int durationMs = 5000); // 默认显示5秒

    void updateStreamStatus(StreamStatus status);
    void bindFPSCounter(FPSCounter* fpsCounter);
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
    void statusInfoUpdated(const QString& recordStatus,const QString& streamStatus);
    void cpuInfoUpdated(double cpuUsage);
    void fpsInfoUpdated(double frameRate);
    void steamInfoUpdated(const NetworkMonitorResult& result);
    void networkStatusUpdated(NetworkStatus status);
    void showNetWorkLabel();
    void hideNetWorkLabel();


public slots:
    void updateFrameRate(double fps);
    void updateCpuUsage(double usage);
    void updateSteamInfo(const NetworkMonitorResult& result);
    void updateNetworkSatus(const NetworkMonitorResult& result);

private slots:
    void onMessageTimeout();

private:
    StatusBarManager(QObject* parent = nullptr);
    void updateStatusText();
    NetworkStatus getNetworkStatus(int delayMs);

    // 当前状态
    StreamStatus m_streamStatus;

    // 系统信息
    double m_cpuUsage;
    double m_frameRate;

    // 消息相关
    QString m_currentMessage;
    MessageType m_currentMsgType;
    QTimer* m_msgTimer;

    // 状态文本
    QString m_recordText;
    QString m_streamText;

    // FPS
    int m_maxFPS = 30;

    // CPU
    CpuMonitor* cpuMonitor_;
    QThread cpuThread_;



};

#endif // STATUSBARMANAGER_H
