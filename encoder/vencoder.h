#ifndef VENCODER_H
#define VENCODER_H

#include <QObject>
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <cuda.h>
#include <cuda_runtime.h>
#include "render/rgbatonv12.h"
#include "queue/packetqueue.h"

// 前向声明
class AVSyncClock;

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
}

// 编码配置参数
struct videoEncodeConfig {
    int width = 1920;          // 视频宽度（需与FBO一致）
    int height = 1080;         // 视频高度（需与FBO一致）
    int bitrate = 2500000;     // 目标比特率（bps）
    int framerate = 30;        // 帧率（fps）
    int gop_size = 60;         // 关键帧间隔（2秒@30fps）
    QString format = "mp4";            // mp4/mkv
    int max_b_frames = 0;      // B帧数量（0避免延迟）
};
class VEncoder : public QObject
{
    Q_OBJECT
public:


    VEncoder(PacketQueue* vEnPktQueue);
    VEncoder();
    ~VEncoder();

    // 禁止拷贝
    VEncoder(const VEncoder&) = delete;
    VEncoder& operator=(const VEncoder&) = delete;

    bool init(const videoEncodeConfig& config);

    bool encode(cudaArray_t cuda_array);
    void flush();
    void close();

    void setNextPts(int64_t videoPts) { videoPts_ = videoPts; }
    void setBitrate(int targetBitrate);
    // 获取编码参数（用于Muxer配置）
    const videoEncodeConfig& getConfig() const { return config_; }
    AVCodecContext* getCodecContext() const { return codecCtx_; }
    AVRational timeBase() const { return codecCtx_->time_base; }
    // 获取数据包队列
    PacketQueue* getPacketQueue() { return vEnPktQueue_; }
    int getEncodedFrameCount() const {return encodedFrameCount_.load(); }
    // 获取延迟帧总数（编码耗时超过阈值的帧）
    int getDelayedFrameCount() const {return delayedFrameCount_.load();}

    // 获取平均编码延迟（毫秒）
    double getAverageEncodeDelay() const;

    // 获取最近N帧的平均延迟（毫秒，用于实时监控）
    double getRecentAverageDelay() const;

    // 重置统计数据（如重新开始编码时）
    void resetEncodeStats();

private:

    QString selectEncoderByFormat(const QString& format);

    // 编码参数
    videoEncodeConfig config_;
    bool is_initialized_ = false;

    // FFmpeg编码器相关
    const AVCodec* codec_ = nullptr;
    AVCodecContext* codecCtx_ = nullptr;
    AVFrame* frame_ = nullptr;  // 用于存储NV12数据并送入编码器

    // CUDA设备内存（Y和UV平面）
    CUdeviceptr d_y_plane_ = 0;   // Y分量设备指针
    CUdeviceptr d_uv_plane_ = 0;  // UV分量设备指针（NV12格式，U和V交错）
    size_t y_size_ = 0;           // Y平面大小
    size_t uv_size_ = 0;          // UV平面大小

    // 数据包队列
    PacketQueue* vEnPktQueue_;

    // 帧率时间基（用于计算PTS）
    AVRational time_base_ = {1, 90000};  // 标准90kHz时间基
    int64_t videoPts_ = 0;

    // 编码统计相关
    std::atomic<int> encodedFrameCount_ = 0;       // 已编码总帧数（原子变量，支持多线程安全）
    std::atomic<int> delayedFrameCount_ = 0;       // 延迟帧总数（编码耗时超过阈值的帧）
    mutable std::mutex delayStatsMutex_;                   // 延迟统计的互斥锁
    qint64 totalEncodeTime_ = 0;                   // 累计编码总耗时（毫秒）
    int maxAcceptableDelay_ = 40;                  // 可接受的最大编码延迟（毫秒，根据需求调整，如25fps对应40ms）
    std::vector<qint64> recentDelays_;             // 最近N帧的编码延迟（用于实时监控）
    const int kRecentDelayWindow_ = 30;            // 最近帧的窗口大小（如30帧）

};



#endif // VENCODER_H
