#ifndef OFFSETMANAGER_H
#define OFFSETMANAGER_H
#include <mutex>
#include <unordered_map>
#include "infotools.h""
extern "C" {
#include <libavutil/frame.h>
}
struct SyncOffsetConfig {
    // 全局偏移（对所有源生效）
    int globalAudioOffsetMs = 0;  // 音频全局偏移
    int globalVideoOffsetMs = 0;  // 视频全局偏移

    // 源级偏移（key：sourceId，value：该源的偏移量）
    std::unordered_map<int, int> audioSourceOffsets;  // 单个音频源的偏移
    std::unordered_map<int, int> videoSourceOffsets;  // 单个视频源的偏移
};
class OffsetManager {
public:
    static OffsetManager& getInstance() {
        static OffsetManager instance;
        return instance;
    }

    // 设置全局偏移
    void setGlobalOffset(bool isAudio, int offsetMs) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (isAudio) {
            config_.globalAudioOffsetMs = offsetMs;
        } else {
            config_.globalVideoOffsetMs = offsetMs;
        }
    }

    // 设置单个源的偏移
    void setSourceOffset(bool isAudio, int sourceId, int offsetMs) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (isAudio) {
            config_.audioSourceOffsets[sourceId] = offsetMs;
        } else {
            config_.videoSourceOffsets[sourceId] = offsetMs;
        }
    }

    // 获取当前配置（供处理帧时使用）
    SyncOffsetConfig getConfig() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return config_;
    }

    // 视频帧偏移处理
    void processVideoOffset(CudaFrameInfo& frame, const SyncOffsetConfig& config) {
        // 原始时间戳（微秒）
        int64_t originalTsUs = frame.timestamp;

        // 计算总偏移（毫秒→微秒）
        int64_t totalOffsetUs = 0;
        totalOffsetUs += config.globalVideoOffsetMs * 1000;
        auto it = config.videoSourceOffsets.find(frame.sourceId);
        if (it != config.videoSourceOffsets.end()) {
            totalOffsetUs += it->second * 1000;
        }

        // 确保PTS单调递增（按源ID跟踪）
        int64_t adjustedPtsUs = originalTsUs + totalOffsetUs;
        adjustedPtsUs = std::max(adjustedPtsUs, lastVSourcePts_[frame.sourceId]);
        lastVSourcePts_[frame.sourceId] = adjustedPtsUs;

        // 直接修改输入参数的pts
        frame.timestamp = adjustedPtsUs;
    }



    // 音频帧偏移处理
    void processAudioOffset(AVFrame* frame, int sourceId, const SyncOffsetConfig& config) {
        if (!frame) return; // 增加空指针检查，提升健壮性

        // 原始时间戳（微秒）
        int64_t originalTsUs = (int64_t)frame->opaque;

        // 计算总偏移（毫秒→微秒）
        int64_t totalOffsetUs = 0;
        totalOffsetUs += config.globalAudioOffsetMs * 1000;
        auto it = config.audioSourceOffsets.find(sourceId);
        if (it != config.audioSourceOffsets.end()) {
            qDebug() << "asourceId " << sourceId << "aOffset is " << it->second;
            totalOffsetUs += it->second * 1000;
        }

        // 确保PTS单调递增
        int64_t adjustedPtsUs = originalTsUs + totalOffsetUs;
        adjustedPtsUs = std::max(adjustedPtsUs, lastAPtsUs_);
        lastAPtsUs_ = adjustedPtsUs;

        // 直接修改输入指针指向的frame的pts
        frame->opaque = (void*)adjustedPtsUs;
    }

private:
    OffsetManager() = default;
    mutable std::mutex mtx_;
    SyncOffsetConfig config_;
    std::unordered_map<int, int64_t> lastVSourcePts_;
    int64_t lastAPtsUs_ = 0;
};
#endif // OFFSETMANAGER_H
