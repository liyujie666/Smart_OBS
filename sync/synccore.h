#ifndef SYNC_CORE_H
#define SYNC_CORE_H

#include "queue/cudaframequeue.h"
#include "queue/framequeue.h"  // 假设你的音频帧队列头文件
#include <memory>
#include <cstdint>

// 同步配置参数（可通过UI调整）
struct SyncConfig {
    int64_t maxSyncDiff = 30000;    // 最大可接受音视频时差（微秒，默认30ms）
    int64_t maxVideoLag = 200000;   // 视频最大滞后阈值（微秒，默认200ms）
    int64_t maxVideoLead = 100000;  // 视频最大超前阈值（微秒，默认100ms）
    int audioBufferDuration = 200;  // 音频缓冲区目标时长（毫秒）
};

// 同步结果状态
enum class SyncStatus {
    Success,        // 成功获取同步帧
    NoAudioFrame,   // 无音频帧可用
    NoVideoFrame,   // 无视频帧可用
    SyncFailed      // 超出同步阈值
};

// 同步后的音视频帧对
struct SyncedFrames {
    AVFrame* audioFrame = nullptr;   // 混音后的音频帧
    CudaFrameInfo videoFrame;        // 叠加后的视频帧
    SyncStatus status = SyncStatus::NoAudioFrame;
};

class SyncCore {
public:
    // 构造函数：接收混音后的音频队列和叠加后的视频队列
    SyncCore(std::shared_ptr<FrameQueue> audioQueue,
             std::shared_ptr<CudaFrameQueue> videoQueue,
             const SyncConfig& config = SyncConfig());

    // 析构函数：清理缓存帧
    ~SyncCore();

    // 获取一组成对的同步帧
    SyncedFrames getSyncedFrames();

    // 更新同步配置
    void updateConfig(const SyncConfig& config);

    // 重置同步状态（用于暂停/重启场景）
    void reset();

private:
    // 查找与目标音频时间戳匹配的视频帧
    bool findMatchingVideoFrame(int64_t targetAudioTs, CudaFrameInfo& outFrame);

    // 清理过期的视频帧（滞后于当前音频太多的帧）
    void cleanupStaleVideoFrames(int64_t currentAudioTs);

    // 成员变量
    std::shared_ptr<FrameQueue> audioQueue_;       // 混音后的音频帧队列
    std::shared_ptr<CudaFrameQueue> videoQueue_;   // 叠加后的视频帧队列
    SyncConfig config_;                            // 同步配置参数
    CudaFrameInfo lastVideoFrame_;                 // 上一帧视频（用于复用）
    int64_t lastAudioTs_ = 0;                      // 上一帧音频时间戳
    mutable std::mutex mtx_;                       // 同步锁
};

#endif // SYNC_CORE_H
