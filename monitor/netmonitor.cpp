#include "netmonitor.h"
#include <QDebug>
#include <cmath>

NetMonitor* NetMonitor::instance()
{
    static NetMonitor s_instance;
    return &s_instance;
}

NetMonitor::NetMonitor(QObject *parent)
    : QObject{parent}
{
    // 1秒监测一次（动态码率最佳频率）
    m_monitor_timer = new QTimer(this);
    m_monitor_timer->setInterval(1000);
    connect(m_monitor_timer, &QTimer::timeout, this, &NetMonitor::onMonitorTimer);
    m_monitor_timer->start();

    m_elapsed_timer.start();
}

// 1. 累计发送字节数（FFmpeg IO回调调用）
void NetMonitor::addSendBytes(int bytes)
{
    QMutexLocker locker(&m_mutex);
    m_total_send_bytes += bytes;
    m_total_packets++;
}

// 2. 累计发送失败（丢包）
void NetMonitor::addSendFailed()
{
    QMutexLocker locker(&m_mutex);
    m_send_failed_count++;
}

// 3. 更新推流缓冲区大小
void NetMonitor::updateBufferSize(qint64 size)
{
    QMutexLocker locker(&m_mutex);
    m_buffer_size = size;
    // 计算缓冲时长：缓冲字节 / 实时码率 * 1000
    if (m_current_stats.upload_bps > 0) {
        m_current_stats.buffer_delay_ms = (size * 8 * 1000) / m_current_stats.upload_bps;
    }
}

// 4. 更新网络延迟(RTT)
void NetMonitor::updateRTT(int rtt_ms)
{
    QMutexLocker locker(&m_mutex);
    m_current_rtt = rtt_ms;
    m_rtt_history.append(rtt_ms);
    if (m_rtt_history.size() > 10) m_rtt_history.pop_front();
}

// 定时计算所有指标
void NetMonitor::onMonitorTimer()
{
    calcUploadBps();    // 上行带宽
    calcPacketLoss();   // 丢包率
    calcJitter();       // 网络抖动
    m_current_stats.delay_ms = m_current_rtt;
    m_current_stats.buffer_size = m_buffer_size;

    // 判定网络等级（直接用于动态码率）
    if (m_current_stats.upload_bps < 500000 || m_current_stats.packet_loss_rate > 30)
        m_current_stats.level = 3; // 断网/极差
    else if (m_current_stats.packet_loss_rate > 10 || m_current_stats.delay_ms > 500)
        m_current_stats.level = 2; // 差
    else if (m_current_stats.packet_loss_rate > 0 || m_current_stats.delay_ms > 200)
        m_current_stats.level = 1; // 一般
    else
        m_current_stats.level = 0; // 优秀
}

// 计算上行带宽（bps）
void NetMonitor::calcUploadBps()
{
    QMutexLocker locker(&m_mutex);
    m_current_stats.upload_bps = (m_total_send_bytes - m_last_send_bytes) * 8;
    m_last_send_bytes = m_total_send_bytes;
}

// 计算丢包率（%）
void NetMonitor::calcPacketLoss()
{
    QMutexLocker locker(&m_mutex);
    if (m_total_packets == 0) {
        m_current_stats.packet_loss_rate = 0;
        return;
    }
    m_current_stats.packet_loss_rate = (m_send_failed_count * 100) / m_total_packets;
    // 每秒重置计数
    m_send_failed_count = 0;
    m_total_packets = 0;
}

// 计算网络抖动（延迟波动）
void NetMonitor::calcJitter()
{
    QMutexLocker locker(&m_mutex);
    if (m_rtt_history.size() < 2) {
        m_current_stats.jitter_ms = 0;
        return;
    }
    // 计算延迟方差 = 抖动
    int avg = 0;
    for (int rtt : m_rtt_history) avg += rtt;
    avg /= m_rtt_history.size();
    int var = 0;
    for (int rtt : m_rtt_history) var += pow(rtt - avg, 2);
    m_current_stats.jitter_ms = sqrt(var / m_rtt_history.size());
}

// 获取当前所有网络指标
NetworkStats NetMonitor::getCurrentStats()
{
    QMutexLocker locker(&m_mutex);
    return m_current_stats;
}
