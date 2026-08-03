#ifndef BENCHMARKCOLLECTOR_H
#define BENCHMARKCOLLECTOR_H

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

#include <mutex>
#include <atomic>
#include <unordered_map>
#include <deque>
#include <string>
#include <functional>
#include <chrono>

#include "rtmp/types.h"

/**
 * @brief 单个指标的统计摘要
 */
struct MetricSummary {
    QString name;
    int count = 0;
    double min = 0.0;
    double max = 0.0;
    double avg = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
    double stddev = 0.0;
    double latest = 0.0;
};

/**
 * @brief Benchmark 快照，包含某一时刻的所有性能指标
 */
struct BenchmarkSnapshot {
    int64_t timestampMs = 0;          // 距启动的毫秒数

    // 编码
    double encodeAvgDelayMs = 0.0;
    double encodeRecentDelayMs = 0.0;
    int encodedFrames = 0;
    int delayedEncodeFrames = 0;

    // 渲染
    double renderAvgDelayMs = 0.0;
    uint64_t renderedFrames = 0;
    uint64_t delayedRenderFrames = 0;

    // FPS
    double currentFps = 0.0;

    // CPU
    double processCpu = 0.0;
    double systemCpu = 0.0;

    // 内存
    uint64_t processMemoryBytes = 0;

    // 网络/推流
    int sendThroughputBps = 0;
    int encodeInputBps = 0;
    int64_t bytesInflight = 0;
    int rttMs = 0;
    int videoQueueDelayMs = 0;
    int audioQueueDelayMs = 0;
    int socketWriteBlockMs = 0;
    int droppedVideoFrames = 0;

    // 推流状态码
    rtmp::SessionState sessionState = rtmp::SessionState::Idle;
    QString stateDetail;
    int reconnectCount = 0;
    int networkLevel = 3;

    // ABR
    int currentBitrate = 0;

    // 队列深度
    int videoFrameQueueSize = 0;
    int audioFrameQueueSize = 0;
    int videoPacketQueueSize = 0;
    int audioPacketQueueSize = 0;
};

// 前向声明
class VEncoder;
class CudaRenderWidget;
class FPSCounter;
class NetMonitor;
class SystemMonitor;
class DynamicBitrateController;
class FrameQueue;
class PacketQueue;

/**
 * @brief 轻量级 Benchmark 性能采集工具
 *
 * 功能：
 * 1. 定时采集各模块的性能指标（编码、渲染、FPS、CPU、网络、队列等）
 * 2. 支持自定义插桩计时（ScopedTimer 配合使用）
 * 3. 将结果导出为 JSON 报告
 *
 * 用法：
 *auto& bench = BenchmarkCollector::instance();
 *   bench.bindEncoder(vEncoder);
 *   bench.bindRenderer(renderWidget);
 *   bench.start(60);// 采集60秒
 *   // ... 运行结束后
 *   bench.exportReport("benchmark_result.json");
 */
class BenchmarkCollector : public QObject
{
    Q_OBJECT

public:
    static BenchmarkCollector& instance();

    //========== 绑定数据源 ==========
    void bindEncoder(VEncoder* encoder);
    void bindRenderer(CudaRenderWidget* renderer);
    void bindFpsCounter(FPSCounter* fpsCounter);
    void bindNetMonitor(NetMonitor* netMonitor);
    void bindSystemMonitor(SystemMonitor* sysMonitor);
    void bindAbrController(DynamicBitrateController* abr);

    // 绑定队列（可多次调用，按名字区分）
    void bindVideoFrameQueue(FrameQueue* queue);
    void bindAudioFrameQueue(FrameQueue* queue);
    void bindVideoPacketQueue(PacketQueue* queue);
    void bindAudioPacketQueue(PacketQueue* queue);

    // ========== 控制 ==========

    /**
     * @brief 启动benchmark采集
     * @param durationSec 采集时长（秒），0 表示手动停止
     * @param intervalMs 采样间隔（毫秒），默认 1000ms
     */
    void start(int durationSec = 0, int intervalMs = 1000);

    /**
     * @brief 停止采集
     */
    void stop();

    /**
     * @brief 是否正在采集
     */
    bool isRunning() const;

    // ========== 自定义插桩 ==========

    /**
     * @brief 记录一次自定义计时结果
     * @param tag 标签名（如 "cuda_nv12_to_rgba", "amix_process"）
     * @param elapsedMs 耗时（毫秒）
     */
    void record(const QString& tag, double elapsedMs);

    // ========== 导出 ==========

    /**
     * @brief 导出 JSON 报告到文件
     * @param filePath 输出路径
     * @return 是否成功
     */
    bool exportReport(const QString& filePath) const;

    /**
     * @brief 获取所有自定义标签的统计摘要
     */
    QVector<MetricSummary> getCustomMetrics() const;

    /**
     * @brief 获取所有快照
     */
    QVector<BenchmarkSnapshot> getSnapshots() const;

    /**
     * @brief 获取当前最新快照
     */
    BenchmarkSnapshot getLatestSnapshot() const;

signals:
    void snapshotTaken(const BenchmarkSnapshot& snapshot);
    void benchmarkFinished();

private:
    explicit BenchmarkCollector(QObject* parent = nullptr);
    ~BenchmarkCollector() override = default;
    Q_DISABLE_COPY(BenchmarkCollector)

    void takeSample();
    BenchmarkSnapshot collectSnapshot() const;
    MetricSummary computeSummary(const QString& name, const std::deque<double>& samples) const;
    QJsonObject snapshotToJson(const BenchmarkSnapshot& snap) const;
    QJsonObject summaryToJson(const MetricSummary& summary) const;

private:
    QTimer sampleTimer_;
    QElapsedTimer elapsed_;
    int durationSec_ = 0;

    // 数据源绑定
    VEncoder* encoder_ = nullptr;
    CudaRenderWidget* renderer_ = nullptr;
    FPSCounter* fpsCounter_ = nullptr;
    NetMonitor* netMonitor_ = nullptr;
    SystemMonitor* sysMonitor_ = nullptr;
    DynamicBitrateController* abr_ = nullptr;

    FrameQueue* videoFrameQueue_ = nullptr;
    FrameQueue* audioFrameQueue_ = nullptr;
    PacketQueue* videoPacketQueue_ = nullptr;
    PacketQueue* audioPacketQueue_ = nullptr;

    // CPU缓存（从信号更新）
    double lastProcessCpu_ = 0.0;
    double lastSystemCpu_ = 0.0;
    double lastFps_ = 0.0;

    // 采集结果
    mutable std::mutex mutex_;
    QVector<BenchmarkSnapshot> snapshots_;

    // 自定义插桩数据: tag -> samples
    mutable std::mutex customMutex_;
    std::unordered_map<std::string, std::deque<double>> customMetrics_;
    static constexpr int kMaxCustomSamples = 10000;
};

#endif // BENCHMARKCOLLECTOR_H
