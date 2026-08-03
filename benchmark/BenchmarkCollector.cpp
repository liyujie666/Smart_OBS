#include "BenchmarkCollector.h"

#include <QFile>
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <numeric>

// 按需包含数据源头文件
#include "encoder/vencoder.h"
#include "render/cudarenderwidget.h"
#include "statusbar/fpscounter.h"
#include "monitor/netmonitor.h"
#include "monitor/systemmonitor.h"
#include "controller/dynamicbitratecontroller.h"
#include "queue/framequeue.h"
#include "queue/packetqueue.h"

BenchmarkCollector& BenchmarkCollector::instance()
{
    static BenchmarkCollector inst;
    return inst;
}

BenchmarkCollector::BenchmarkCollector(QObject* parent)
    : QObject(parent)
{
    connect(&sampleTimer_, &QTimer::timeout, this, &BenchmarkCollector::takeSample);
}

//========== 绑定数据源 ==========

void BenchmarkCollector::bindEncoder(VEncoder* encoder)
{
    encoder_ = encoder;
}

void BenchmarkCollector::bindRenderer(CudaRenderWidget* renderer)
{
    renderer_ = renderer;
}

void BenchmarkCollector::bindFpsCounter(FPSCounter* fpsCounter)
{
    fpsCounter_ = fpsCounter;
    if (fpsCounter_) {
        connect(fpsCounter_, &FPSCounter::fpsInfoUpdated, this, [this](double fps) {
            lastFps_ = fps;
        });
    }
}

void BenchmarkCollector::bindNetMonitor(NetMonitor* netMonitor)
{
    netMonitor_ = netMonitor;
}

void BenchmarkCollector::bindSystemMonitor(SystemMonitor* sysMonitor)
{
    sysMonitor_ = sysMonitor;
}

void BenchmarkCollector::bindAbrController(DynamicBitrateController* abr)
{
    abr_ = abr;
}

void BenchmarkCollector::bindVideoFrameQueue(FrameQueue* queue)
{
    videoFrameQueue_ = queue;
}

void BenchmarkCollector::bindAudioFrameQueue(FrameQueue* queue)
{
    audioFrameQueue_ = queue;
}

void BenchmarkCollector::bindVideoPacketQueue(PacketQueue* queue)
{
    videoPacketQueue_ = queue;
}

void BenchmarkCollector::bindAudioPacketQueue(PacketQueue* queue)
{
    audioPacketQueue_ = queue;
}

// ========== 控制 ==========

void BenchmarkCollector::start(int durationSec, int intervalMs)
{
    if (sampleTimer_.isActive()) {
        qWarning() << "[Benchmark] Already running, stop first.";
        return;
    }

    durationSec_ = durationSec;

    {
        std::lock_guard<std::mutex> lk(mutex_);
        snapshots_.clear();
    }
    {
        std::lock_guard<std::mutex> lk(customMutex_);
        customMetrics_.clear();
    }

    elapsed_.start();
    sampleTimer_.start(intervalMs);

    qInfo() << "[Benchmark] Started. Duration:" << durationSec << "s, Interval:" << intervalMs << "ms";
}

void BenchmarkCollector::stop()
{
    if (!sampleTimer_.isActive()) return;

    sampleTimer_.stop();
    qInfo() << "[Benchmark] Stopped. Collected" << snapshots_.size() << "snapshots.";
    emit benchmarkFinished();
}

bool BenchmarkCollector::isRunning() const
{
    return sampleTimer_.isActive();
}

// ========== 自定义插桩 ==========

void BenchmarkCollector::record(const QString& tag, double elapsedMs)
{
    std::lock_guard<std::mutex> lk(customMutex_);
    auto& samples = customMetrics_[tag.toStdString()];
    samples.push_back(elapsedMs);
    if (static_cast<int>(samples.size()) > kMaxCustomSamples) {
        samples.pop_front();
    }
}

// ========== 采样 ==========

void BenchmarkCollector::takeSample()
{
    // 检查是否超时
    if (durationSec_ > 0 && elapsed_.elapsed() >= durationSec_ * 1000LL) {
        stop();
        return;
    }

    BenchmarkSnapshot snap = collectSnapshot();

    {
        std::lock_guard<std::mutex> lk(mutex_);
        snapshots_.append(snap);
    }

    emit snapshotTaken(snap);
}

BenchmarkSnapshot BenchmarkCollector::collectSnapshot() const
{
    BenchmarkSnapshot snap;
    snap.timestampMs = elapsed_.elapsed();

    // 编码指标
    if (encoder_) {
        snap.encodeAvgDelayMs = encoder_->getAverageEncodeDelay();
        snap.encodeRecentDelayMs = encoder_->getRecentAverageDelay();
        snap.encodedFrames = encoder_->getEncodedFrameCount();
        snap.delayedEncodeFrames = encoder_->getDelayedFrameCount();
    }

    // 渲染指标
    if (renderer_) {
        snap.renderAvgDelayMs = renderer_->getRecentAverageRenderDelay();
        snap.renderedFrames = renderer_->getRenderedFrameCount();
        snap.delayedRenderFrames = renderer_->getDelayedRenderFrameCount();
    }

    // FPS
    if (fpsCounter_) {
        snap.currentFps = fpsCounter_->getFPS();
    } else {
        snap.currentFps = lastFps_;
    }

    // 系统资源
    if (sysMonitor_) {
        auto metrics = sysMonitor_->getCurrentMetrics();
        snap.processCpu = metrics.process_cpu_usage;
        snap.systemCpu = metrics.cpu_usage;
        snap.processMemoryBytes = metrics.process_memory;
    }

    // 网络/推流
    if (netMonitor_) {
        auto stats = netMonitor_->getCurrentStats();
        snap.sendThroughputBps = stats.sendThroughputBps;
        snap.encodeInputBps = stats.encodeInputBps;
        snap.bytesInflight = stats.bytesInflight;
        snap.rttMs = stats.rttMs;
        snap.videoQueueDelayMs = stats.videoQueueDelayMs;
        snap.audioQueueDelayMs = stats.audioQueueDelayMs;
        snap.socketWriteBlockMs = stats.socketWriteBlockMs;
        snap.droppedVideoFrames = stats.droppedVideoFrames;

        // 推流状态码
        snap.sessionState = stats.sessionState;
        snap.stateDetail = stats.stateDetail;
        snap.reconnectCount = stats.reconnectCount;
        snap.networkLevel = stats.networkLevel;
    }

    // ABR
    if (abr_) {
        snap.currentBitrate = abr_->getCurrentVideoBitrate();
    }

    // 队列深度
    if (videoFrameQueue_) snap.videoFrameQueueSize = videoFrameQueue_->size();
    if (audioFrameQueue_) snap.audioFrameQueueSize = audioFrameQueue_->size();
    if (videoPacketQueue_) snap.videoPacketQueueSize = videoPacketQueue_->size();
    if (audioPacketQueue_) snap.audioPacketQueueSize = audioPacketQueue_->size();

    return snap;
}

// ========== 统计计算 ==========

MetricSummary BenchmarkCollector::computeSummary(const QString& name, const std::deque<double>& samples) const
{
    MetricSummary s;
    s.name = name;
    s.count = static_cast<int>(samples.size());

    if (samples.empty()) return s;

    // 排序副本用于计算百分位
    std::vector<double> sorted(samples.begin(), samples.end());
    std::sort(sorted.begin(), sorted.end());

    s.min = sorted.front();
    s.max = sorted.back();
    s.latest = samples.back();

    double sum = std::accumulate(sorted.begin(), sorted.end(), 0.0);
    s.avg = sum / sorted.size();

    // P95
    int p95Idx = static_cast<int>(sorted.size() * 0.95);
    p95Idx = std::min(p95Idx, static_cast<int>(sorted.size()) - 1);
    s.p95 = sorted[p95Idx];

    // P99
    int p99Idx = static_cast<int>(sorted.size() * 0.99);
    p99Idx = std::min(p99Idx, static_cast<int>(sorted.size()) - 1);
    s.p99 = sorted[p99Idx];

    //标准差
    double sqSum = 0.0;
    for (double v : sorted) {
        sqSum += (v - s.avg) * (v - s.avg);
    }
    s.stddev = std::sqrt(sqSum / sorted.size());

    return s;
}

// ========== 导出 ==========

QVector<MetricSummary> BenchmarkCollector::getCustomMetrics() const
{
    std::lock_guard<std::mutex> lk(customMutex_);
    QVector<MetricSummary> result;
    for (auto& [tag, samples] : customMetrics_) {
        result.append(computeSummary(QString::fromStdString(tag), samples));
    }
    return result;
}

QVector<BenchmarkSnapshot> BenchmarkCollector::getSnapshots() const
{
    std::lock_guard<std::mutex> lk(mutex_);
    return snapshots_;
}

BenchmarkSnapshot BenchmarkCollector::getLatestSnapshot() const
{
    std::lock_guard<std::mutex> lk(mutex_);
    if (snapshots_.isEmpty()) return {};
    return snapshots_.last();
}

QJsonObject BenchmarkCollector::snapshotToJson(const BenchmarkSnapshot& snap) const
{
    QJsonObject obj;
    obj["timestamp_ms"] = snap.timestampMs;

    // 编码
    obj["encode_avg_delay_ms"] = snap.encodeAvgDelayMs;
    obj["encode_recent_delay_ms"] = snap.encodeRecentDelayMs;
    obj["encoded_frames"] = snap.encodedFrames;
    obj["delayed_encode_frames"] = snap.delayedEncodeFrames;

    // 渲染
    obj["render_avg_delay_ms"] = snap.renderAvgDelayMs;
    obj["rendered_frames"] = static_cast<qint64>(snap.renderedFrames);
    obj["delayed_render_frames"] = static_cast<qint64>(snap.delayedRenderFrames);

    // FPS / CPU
    obj["current_fps"] = snap.currentFps;
    obj["process_cpu_percent"] = snap.processCpu;
    obj["system_cpu_percent"] = snap.systemCpu;
    obj["process_memory_mb"] = static_cast<double>(snap.processMemoryBytes) / (1024.0 * 1024.0);

    // 网络
    obj["send_throughput_kbps"] = snap.sendThroughputBps / 1000;
    obj["encode_input_kbps"] = snap.encodeInputBps / 1000;
    obj["bytes_inflight_kb"] = static_cast<double>(snap.bytesInflight) / 1024.0;
    obj["rtt_ms"] = snap.rttMs;
    obj["video_queue_delay_ms"] = snap.videoQueueDelayMs;
    obj["audio_queue_delay_ms"] = snap.audioQueueDelayMs;
    obj["socket_write_block_ms"] = snap.socketWriteBlockMs;
    obj["dropped_video_frames"] = snap.droppedVideoFrames;

    // 推流状态码
    obj["session_state"] = QString::fromUtf8(rtmp::toString(snap.sessionState));
    obj["state_detail"] = snap.stateDetail;
    obj["reconnect_count"] = snap.reconnectCount;
    obj["network_level"] = snap.networkLevel;

    // ABR
    obj["current_bitrate_kbps"] = snap.currentBitrate / 1000;

    // 队列
    obj["video_frame_queue"] = snap.videoFrameQueueSize;
    obj["audio_frame_queue"] = snap.audioFrameQueueSize;
    obj["video_packet_queue"] = snap.videoPacketQueueSize;
    obj["audio_packet_queue"] = snap.audioPacketQueueSize;

    return obj;
}

QJsonObject BenchmarkCollector::summaryToJson(const MetricSummary& summary) const
{
    QJsonObject obj;
    obj["name"] = summary.name;
    obj["count"] = summary.count;
    obj["min_ms"] = QString::number(summary.min, 'f', 3).toDouble();
    obj["max_ms"] = QString::number(summary.max, 'f', 3).toDouble();
    obj["avg_ms"] = QString::number(summary.avg, 'f', 3).toDouble();
    obj["p95_ms"] = QString::number(summary.p95, 'f', 3).toDouble();
    obj["p99_ms"] = QString::number(summary.p99, 'f', 3).toDouble();
    obj["stddev_ms"] = QString::number(summary.stddev, 'f', 3).toDouble();
    obj["latest_ms"] = QString::number(summary.latest, 'f', 3).toDouble();
    return obj;
}

bool BenchmarkCollector::exportReport(const QString& filePath) const
{
    QJsonObject report;

    // 元信息
    QJsonObject meta;
    meta["total_duration_ms"] = elapsed_.elapsed();
    meta["snapshot_count"] = snapshots_.size();
    report["meta"] = meta;

    // 时序快照
    QJsonArray snapshotsArr;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        for (const auto& snap : snapshots_) {
            snapshotsArr.append(snapshotToJson(snap));
        }
    }
    report["snapshots"] = snapshotsArr;

    // 自定义指标汇总
    QJsonArray customArr;
    {
        std::lock_guard<std::mutex> lk(customMutex_);
        for (auto& [tag, samples] : customMetrics_) {
            auto summary = computeSummary(QString::fromStdString(tag), samples);
            customArr.append(summaryToJson(summary));
        }
    }
    report["custom_metrics"] = customArr;

    // 汇总统计（从快照序列中计算关键指标的平均值）
    if (!snapshots_.isEmpty()) {
        QJsonObject summary;
        double avgFps = 0, avgCpu = 0, avgEncDelay = 0, avgRenderDelay = 0;
        int maxQueueDelay = 0, totalDropped = 0;

        std::lock_guard<std::mutex> lk(mutex_);
        for (const auto& s : snapshots_) {
            avgFps += s.currentFps;
            avgCpu += s.processCpu;
            avgEncDelay += s.encodeRecentDelayMs;
            avgRenderDelay += s.renderAvgDelayMs;
            maxQueueDelay = std::max(maxQueueDelay, s.videoQueueDelayMs);
            totalDropped = std::max(totalDropped, s.droppedVideoFrames);
        }
        int n = snapshots_.size();
        summary["avg_fps"] = QString::number(avgFps / n, 'f', 1).toDouble();
        summary["avg_process_cpu_percent"] = QString::number(avgCpu / n, 'f', 1).toDouble();
        summary["avg_encode_delay_ms"] = QString::number(avgEncDelay / n, 'f', 2).toDouble();
        summary["avg_render_delay_ms"] = QString::number(avgRenderDelay / n, 'f', 2).toDouble();
        summary["max_video_queue_delay_ms"] = maxQueueDelay;
        summary["total_dropped_frames"] = totalDropped;

        if (!snapshots_.isEmpty()) {
            auto& last = snapshots_.last();
            if (last.encodedFrames > 0) {
                summary["encode_delay_ratio_percent"] =
                    QString::number(100.0 * last.delayedEncodeFrames / last.encodedFrames, 'f', 2).toDouble();
            }
            if (last.renderedFrames > 0) {
                summary["render_delay_ratio_percent"] =
                    QString::number(100.0 * last.delayedRenderFrames / last.renderedFrames, 'f', 2).toDouble();
            }
        }
        report["summary"] = summary;
    }

    // 写入文件
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[Benchmark] Failed to open file:" << filePath;
        return false;
    }

    QJsonDocument doc(report);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    qInfo() << "[Benchmark] Report exported to:" << filePath;
    return true;
}
