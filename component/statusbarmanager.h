#ifndef STATUSBARMANAGER_H
#define STATUSBARMANAGER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QDateTime>

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

    // 禁止复制和移动
    StatusBarManager(const StatusBarManager&) = delete;
    StatusBarManager& operator=(const StatusBarManager&) = delete;
    StatusBarManager(StatusBarManager&&) = delete;
    StatusBarManager& operator=(StatusBarManager&&) = delete;

    // 显示消息的方法
    void showMessage(const QString& text,
                     MessageType type = MessageType::Info,
                     int durationMs = 5000); // 默认显示5秒

    // 更新推流/录制状态
    void updateStreamStatus(StreamStatus status);

    // 更新CPU占用率
    void updateCpuUsage(double usage);

    // 更新视频帧率
    void updateFrameRate(double fps);

    double cpuUsage() const { return m_cpuUsage; }
    double frameRate() const { return m_frameRate; }
    QString recordText() const { return m_recordText; }
    QString streamText() const { return m_streamText; }

signals:
    // 当有新消息需要显示时发射此信号
    // 参数: 消息文本, 消息类型, 是否显示消息(否则显示系统信息)
    void statusUpdated(const QString& text, MessageType type, bool showMessage);

    // 当系统信息更新时发射此信号
    // 参数: CPU占用率, 帧率, 状态文本
    void statusInfoUpdated(const QString& recordStatus,const QString& streamStatus);
    void cpuInfoUpdated(double cpuUsage);
    void fpsInfoUpdated(double frameRate);

private slots:
    // 消息超时处理
    void onMessageTimeout();

private:
    // 私有构造函数
    StatusBarManager(QObject* parent = nullptr);

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

    // 更新状态文本
    void updateStatusText();
};

#endif // STATUSBARMANAGER_H
