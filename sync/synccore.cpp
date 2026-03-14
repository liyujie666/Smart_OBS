#include "synccore.h"
#include "pool/gloabalpool.h"
#include <algorithm>
#include <cmath>
#include <iostream>

// 辅助函数：计算音频帧时长（微秒）
static int64_t calculateAudioFrameDuration(AVFrame* frame) {
    if (!frame) return 0;
    // 时长 = 样本数 / 采样率 * 1e6（微秒）
    return (int64_t)((double)frame->nb_samples / frame->sample_rate * 1e6);
}

SyncCore::SyncCore(std::shared_ptr<FrameQueue> audioQueue,
                   std::shared_ptr<CudaFrameQueue> videoQueue,
                   const SyncConfig& config)
    : audioQueue_(audioQueue),
    videoQueue_(videoQueue),
    config_(config) {
    // 初始化最后一帧视频的时间戳为0
    lastVideoFrame_.timestamp = 0;
}

SyncCore::~SyncCore() {
    // 释放缓存的音频帧（如果有未处理的）
    reset();
}

void SyncCore::updateConfig(const SyncConfig& config) {
    std::lock_guard<std::mutex> lock(mtx_);
    config_ = config;
}

void SyncCore::reset() {
    std::lock_guard<std::mutex> lock(mtx_);
    lastVideoFrame_.timestamp = 0;
    lastAudioTs_ = 0;
}

SyncedFrames SyncCore::getSyncedFrames() {
    SyncedFrames result;
    std::lock_guard<std::mutex> lock(mtx_);

    // 1. 获取混音后的音频帧（作为基准）
    AVFrame* audioFrame = audioQueue_->pop();
    if (!audioFrame) {  // 从音频队列取出一帧
        result.status = SyncStatus::NoAudioFrame;
        return result;
    }

    // 2. 提取音频帧的全局时间戳
    int64_t audioTs = (int64_t)audioFrame->opaque;
    if (audioTs <= 0) {  // 无效时间戳检查
        GlobalPool::getFramePool().recycle(audioFrame);
        result.status = SyncStatus::SyncFailed;
        return result;
    }

    // 3. 清理滞后过多的视频帧（避免队列堆积）
    cleanupStaleVideoFrames(audioTs);

    // 4. 查找匹配的视频帧
    CudaFrameInfo videoFrame;
    bool found = findMatchingVideoFrame(audioTs, videoFrame);

    if (found) {
        // 5. 成功找到匹配帧
        result.audioFrame = audioFrame;
        result.videoFrame = videoFrame;
        result.status = SyncStatus::Success;
        lastVideoFrame_ = videoFrame;  // 更新缓存的上一帧视频
        lastAudioTs_ = audioTs;        // 更新上一帧音频时间戳
    } else {
        // 6. 未找到匹配帧，尝试复用最近的视频帧
        if (lastVideoFrame_.timestamp != 0) {
            int64_t reuseDiff = std::abs(lastVideoFrame_.timestamp - audioTs);
            if (reuseDiff <= config_.maxSyncDiff * 2) {  // 允许两倍阈值的复用
                result.audioFrame = audioFrame;
                result.videoFrame = lastVideoFrame_;
                result.status = SyncStatus::Success;
                lastAudioTs_ = audioTs;  // 仅更新音频时间戳
            } else {
                // 复用帧差异过大，放弃
                GlobalPool::getFramePool().recycle(audioFrame);
                result.status = SyncStatus::NoVideoFrame;
            }
        } else {
            // 无缓存帧可用
            GlobalPool::getFramePool().recycle(audioFrame);
            result.status = SyncStatus::NoVideoFrame;
        }
    }

    return result;
}

bool SyncCore::findMatchingVideoFrame(int64_t targetAudioTs, CudaFrameInfo& outFrame) {
    // 遍历视频队列查找最佳匹配帧
    bool found = false;
    int64_t minDiff = INT64_MAX;
    CudaFrameInfo bestFrame;

    // 使用forEach避免全量拷贝，直接在回调中计算
    videoQueue_->forEach([&](const CudaFrameInfo& frame) {
        int64_t diff = std::abs(frame.timestamp - targetAudioTs);
        // 找到差异最小且在阈值内的帧
        if (diff < minDiff && diff <= config_.maxSyncDiff) {
            minDiff = diff;
            bestFrame = frame;
            found = true;
        }
    });

    if (found) {
        // 从队列中弹出找到的最佳帧
        // （这里需要逐个弹出前面的帧，直到找到目标帧）
        CudaFrameInfo temp;
        while (videoQueue_->pop(temp)) {
            if (temp.timestamp == bestFrame.timestamp &&
                temp.sourceId == bestFrame.sourceId) {  // 确保是同一帧
                outFrame = temp;
                return true;
            }
            // 前面的帧不是目标帧，说明已过期，直接丢弃
        }
    }

    return false;
}

void SyncCore::cleanupStaleVideoFrames(int64_t currentAudioTs) {
    // 清理所有滞后超过maxVideoLag的视频帧
    int64_t threshold = currentAudioTs - config_.maxVideoLag;
    size_t removed = videoQueue_->removeOlderThan(threshold);
    if (removed > 0) {
        // 可在此处添加调试日志
        std::cout << "Removed " << removed << " stale video frames" << std::endl;
    }
}
