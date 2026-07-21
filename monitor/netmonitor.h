#ifndef NetMonitor_H
#define NetMonitor_H

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QMutex>

// 网络状态结构体（所有你需要的指标）
struct NetworkStats {
    // 1. 上行带宽（推流专用，单位：bps）
    qint64 upload_bps;
    // 2. 网络延迟/RTT（单位：ms）
    int delay_ms;
    // 3. 丢包率（0~100%）
    int packet_loss_rate;
    // 4. 推流缓冲区大小（单位：字节）
    qint64 buffer_size;
    // 5. 缓冲区等待时长（单位：ms，卡顿核心指标）
    int buffer_delay_ms;
    // 6. 网络抖动（延迟波动，单位：ms）
    int jitter_ms;
    // 7. 网络质量等级（0优秀/1一般/2差/3断网）
    int level;
};

class NetMonitor : public QObject
{
    Q_OBJECT
public:
    explicit NetMonitor(QObject *parent = nullptr);
    static NetMonitor* instance(); // 单例

    // 外部传入数据（FFmpeg回调调用）
    void addSendBytes(int bytes);        // 累计发送字节数
    void addSendFailed();                // 累计发送失败次数
    void updateBufferSize(qint64 size);  // 更新推流缓冲区大小
    void updateRTT(int rtt_ms);          // 更新网络延迟

    // 获取当前网络状态
    NetworkStats getCurrentStats();

private slots:
    void onMonitorTimer(); // 1秒定时计算指标
    void calcJitter();     // 计算网络抖动

private:
    // 统计变量
    qint64 m_total_send_bytes = 0;
    qint64 m_last_send_bytes = 0;
    int m_send_failed_count = 0;
    int m_total_packets = 0;

    // 缓冲/延迟
    qint64 m_buffer_size = 0;
    QList<int> m_rtt_history; // 延迟历史（计算抖动）
    int m_current_rtt = 0;

    // 工具
    QTimer* m_monitor_timer;
    QElapsedTimer m_elapsed_timer;
    QMutex m_mutex;

    // 计算指标
    void calcUploadBps();
    void calcPacketLoss();
    NetworkStats m_current_stats;
};

#endif // NetMonitor_H
