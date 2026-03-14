#include "vencoder.h"
#include "pool/gloabalpool.h"
#include <QDebug>

VEncoder::VEncoder(PacketQueue* vEnPktQueue)
    :vEnPktQueue_(vEnPktQueue)
{
}

VEncoder::VEncoder()
{

}

VEncoder::~VEncoder()
{
    close();
}
bool VEncoder::init(const videoEncodeConfig &config)
{
    // 1. 保存配置并检查有效性
    config_ = config;
    resetEncodeStats();
    if (config_.width <= 0 || config_.height <= 0 || config_.framerate <= 0 || config_.format.isEmpty()) {
        qDebug() << "Invalid encoder config";
        return false;
    }
    
    // 确保重置视频PTS，这对于多次录制60fps视频至关重要
    videoPts_ = 0;

    // 2. 根据目标格式选择编码器
    QString codecName = selectEncoderByFormat(config_.format);
    codec_ = avcodec_find_encoder_by_name(codecName.toStdString().c_str());
    if (!codec_) {
        qDebug() << "Failed to find encoder:" << codecName.toStdString().c_str();
        // 尝试备选编码器
        if (codecName == "h264_nvenc") {
            qWarning() << "Trying fallback encoder: libx264";
            codec_ = avcodec_find_encoder_by_name("libx264");
        } else if (codecName == "hevc_nvenc") {
            qWarning() << "Trying fallback encoder: libx265";
            codec_ = avcodec_find_encoder_by_name("libx265");
        }
        if (!codec_) {
            qDebug() << "No suitable encoder found";
            return false;
        }
    }

    // 3. 创建编码器上下文
    codecCtx_ = avcodec_alloc_context3(codec_);
    if (!codecCtx_) {
        qDebug() << "Failed to alloc codec context";
        return false;
    }

    // 4. 配置编码器参数
    codecCtx_->width = config_.width;
    codecCtx_->height = config_.height;
    codecCtx_->bit_rate = config_.bitrate;
    codecCtx_->bit_rate_tolerance = config_.bitrate / 4; // 限制码率波动范围
    codecCtx_->time_base = {1, config_.framerate};       // 时间基与帧率匹配，优化PTS计算
    codecCtx_->framerate = {config_.framerate, 1};
    codecCtx_->gop_size = config_.framerate * 2;         // 固定2秒一个关键帧
    codecCtx_->keyint_min = config_.framerate;           // 最小关键帧间隔1秒
    codecCtx_->max_b_frames = config_.max_b_frames;                         // 禁用B帧，提升兼容性
    codecCtx_->has_b_frames = 0;
    codecCtx_->pix_fmt = AV_PIX_FMT_NV12;
    codecCtx_->codec_type = AVMEDIA_TYPE_VIDEO;
    codecCtx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // 硬件编码器特定配置

    if(codecName.endsWith("nvenc"))
    {
        av_opt_set(codecCtx_->priv_data,"preset", "p3", 0);         // 低延迟预设（合理）
        av_opt_set(codecCtx_->priv_data, "rc", "cbr", 0);           // 恒定码率（CBR）
        av_opt_set_int(codecCtx_->priv_data, "max_bitrate", config_.bitrate, 0);
        av_opt_set_int(codecCtx_->priv_data, "surfaces", 16, 0);    // 表面数设为16（适配高帧率）
    }
    // 软件编码器优化
    else if (codecName == "libx264") {
        av_opt_set(codecCtx_->priv_data, "preset", "medium", 0);    // 平衡速度与压缩
        av_opt_set(codecCtx_->priv_data, "tune", "zerolatency", 0); // 低延迟
        av_opt_set(codecCtx_->priv_data, "profile", "main", 0);     // 兼容性profile
        av_opt_set(codecCtx_->priv_data, "level", "4.1", 0);        // 通用级别
    }
    else if (codecName == "libx265") {
        av_opt_set(codecCtx_->priv_data, "preset", "medium", 0);
        av_opt_set(codecCtx_->priv_data, "tune", "fastdecode", 0);  // 优化解码速度
        av_opt_set(codecCtx_->priv_data, "profile", "main", 0);
    }

    // 6. 打开编码器
    if (avcodec_open2(codecCtx_, codec_, nullptr) < 0) {
        qDebug() << "Failed to open video encoder";
        avcodec_free_context(&codecCtx_);
        codecCtx_ = nullptr;
        return false;
    }

    // 7. 创建AVFrame用于存储NV12数据
    frame_ = GlobalPool::getFramePool().get();
    if (!frame_) {
        qDebug() << "Failed to alloc AVFrame";
        close();
        return false;
    }
    frame_->width = config_.width;
    frame_->height = config_.height;
    frame_->format = AV_PIX_FMT_NV12;
    if (av_frame_get_buffer(frame_, 0) < 0) {
        qDebug() << "Failed to alloc AVFrame buffer";
        close();
        return false;
    }

    // 8. 分配CUDA设备内存（Y和UV平面）
    y_size_ = config_.width * config_.height;
    uv_size_ = (config_.width * config_.height) / 2;

    cudaError_t cuda_err = cudaMalloc(reinterpret_cast<void**>(&d_y_plane_), y_size_);
    if (cuda_err != cudaSuccess) {
        qDebug() << "Failed to alloc Y plane:" << cudaGetErrorString(cuda_err);
        close();
        return false;
    }

    cuda_err = cudaMalloc(reinterpret_cast<void**>(&d_uv_plane_), uv_size_);
    if (cuda_err != cudaSuccess) {
        qDebug() << "Failed to alloc UV plane:" << cudaGetErrorString(cuda_err);
        close();
        return false;
    }

    is_initialized_ = true;
    qInfo() << "Encoder initialized successfully:" << codecName.toStdString().c_str()
            << "for format" << config_.format
            << "(" << config_.width << "x" << config_.height << ")";
    return true;
}

#include <cuda.h>
#include <QElapsedTimer>
bool VEncoder::encode(cudaArray_t cuda_array)
{

    QElapsedTimer frameTimer;
    frameTimer.start();

    if (!is_initialized_ || !cuda_array || !frame_ || !codecCtx_) {
        return false;
    }

    CUarray cu_array = reinterpret_cast<CUarray>(cuda_array); // 转换为驱动 API 的 CUarray
    CUDA_ARRAY_DESCRIPTOR desc;
    CUresult cu_err = cuArrayGetDescriptor_v2(&desc, cu_array);
    if (cu_err != CUDA_SUCCESS) {
        const char* err_str;
        cuGetErrorString(cu_err, &err_str); // 获取驱动 API 错误信息
        qDebug() << "获取 CUDA 数组描述符失败：" << err_str;
        return false;
    }

    int srcWidth = static_cast<int>(desc.Width);
    int srcHeight = static_cast<int>(desc.Height);

    if (srcWidth != config_.width || srcHeight != config_.height) {
        qWarning() << "Cuda 数组尺寸与配置尺寸不一致，采用Cuda默认尺寸（1920x1080）继续编码"
                   << "CUDA 数组尺寸：宽度=" << desc.Width
                   << "，高度=" << desc.Height
                    << "配置的分辨率：" << config_.width << "x" << config_.height;
    }

    frame_->pts = av_rescale_q(videoPts_,AV_TIME_BASE_Q,codecCtx_->time_base);

    // qDebug() << "Video frame PTS (encoder timebase):" << frame_->pts;

    // 分配临时RGBA设备内存
    uint8_t* d_rgba_temp = nullptr;
    size_t pitchRGBA = 0;
    cudaError_t cuda_err = cudaMallocPitch(
        &d_rgba_temp,
        &pitchRGBA,
        srcWidth * 4,
        srcHeight
        );
    if (cuda_err != cudaSuccess) {
        qDebug() << "Failed to alloc temp RGBA buffer:" << cudaGetErrorString(cuda_err);
        return false;
    }

    // 修正cudaMemcpy3D参数：用pitchRGBA作为目标行间距
    cudaMemcpy3DParms copyParams = {0};
    copyParams.srcArray = cuda_array;
    copyParams.srcPos = make_cudaPos(0, 0, 0);
    copyParams.dstPtr = make_cudaPitchedPtr(
        d_rgba_temp,
        pitchRGBA,
        srcWidth * 4,
        srcHeight
        );
    copyParams.dstPos = make_cudaPos(0, 0, 0);
    copyParams.extent = make_cudaExtent(srcWidth, srcHeight, 1);
    copyParams.kind = cudaMemcpyDeviceToDevice;

    cuda_err = cudaMemcpy3D(&copyParams);
    if (cuda_err != cudaSuccess) {
        qDebug() << "cudaMemcpy3D failed:" << cudaGetErrorString(cuda_err);
        cudaFree(d_rgba_temp);
        return false;
    }

    // 调用RGBA→NV12时，传递pitchRGBA
    launchRGBAToNV12(
        reinterpret_cast<CUdeviceptr>(d_rgba_temp),
        d_y_plane_,
        d_uv_plane_,
        srcWidth,
        srcHeight,
        pitchRGBA
        );

    // 检查转换错误
    cuda_err = cudaGetLastError();
    if (cuda_err != cudaSuccess) {
        qDebug() << "RGBAToNV12 conversion failed:" << cudaGetErrorString(cuda_err);
        cudaFree(d_rgba_temp);  // 错误路径释放
        return false;
    }

    // 释放临时RGBA内存（正常流程）
    cudaFree(d_rgba_temp);
    d_rgba_temp = nullptr;  // 避免野指针

    // 拷贝Y平面到AVFrame
    cuda_err = cudaMemcpy(frame_->data[0], reinterpret_cast<void*>(d_y_plane_), y_size_, cudaMemcpyDeviceToHost);
    if (cuda_err != cudaSuccess) {
        qDebug() << "Copy Y plane failed:" << cudaGetErrorString(cuda_err);
        return false;
    }

    // 拷贝UV平面到AVFrame
    cuda_err = cudaMemcpy(frame_->data[1], reinterpret_cast<void*>(d_uv_plane_), uv_size_, cudaMemcpyDeviceToHost);
    if (cuda_err != cudaSuccess) {
        qDebug() << "Copy UV plane failed:" << cudaGetErrorString(cuda_err);
        return false;
    }

    frame_->linesize[0] = srcWidth;
    frame_->linesize[1] = srcWidth;

    // 发送帧到编码器
    int ret = avcodec_send_frame(codecCtx_, frame_);
    if (ret < 0) {
        qDebug() << "Failed to send frame to encoder";
        return false;
    }

    // 获取编码数据包并加入队列
    AVPacket* pkt = GlobalPool::getPacketPool().get();
    while (ret >= 0) {
        ret = avcodec_receive_packet(codecCtx_, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            qDebug() << "Failed to receive packet from encoder";
            GlobalPool::getPacketPool().recycle(pkt);
            return false;
        }

        vEnPktQueue_->push(pkt);
        encodedFrameCount_++;
        pkt = GlobalPool::getPacketPool().get();
    }
    GlobalPool::getPacketPool().recycle(pkt);


    qint64 frameEncodeTime = frameTimer.elapsed();

    // 统计延迟帧（若耗时超过阈值，则视为延迟帧）
    {
        std::lock_guard<std::mutex> lock(delayStatsMutex_);
        totalEncodeTime_ += frameEncodeTime;

        // 记录最近帧的延迟，用于实时监控
        recentDelays_.push_back(frameEncodeTime);
        if (recentDelays_.size() > kRecentDelayWindow_) {
            recentDelays_.erase(recentDelays_.begin()); // 保持窗口大小
        }

        // 判断是否为延迟帧
        if (frameEncodeTime > maxAcceptableDelay_) {
            delayedFrameCount_++;
        }
    }

    return true;
}

void VEncoder::flush() {
    if (!is_initialized_ || !codecCtx_) return;

    // 发送NULL帧通知编码器结束
    avcodec_send_frame(codecCtx_, nullptr);

    // 循环获取所有缓存的数据包并加入队列
    AVPacket* pkt = GlobalPool::getPacketPool().get();
    int ret = 0;
    while ((ret = avcodec_receive_packet(codecCtx_, pkt)) >= 0) {
        vEnPktQueue_->push(pkt);
        pkt = GlobalPool::getPacketPool().get();
    }
    GlobalPool::getPacketPool().recycle(pkt);

    // 通知队列等待者
    vEnPktQueue_->close();
}

void VEncoder::close()
{
    // 清空队列
    vEnPktQueue_->clear();

    // 释放CUDA资源
    if (d_y_plane_) {
        cudaFree(reinterpret_cast<void*>(d_y_plane_));
        d_y_plane_ = 0;
    }
    if (d_uv_plane_) {
        cudaFree(reinterpret_cast<void*>(d_uv_plane_));
        d_uv_plane_ = 0;
    }

    // 释放FFmpeg资源
    GlobalPool::getFramePool().recycle(frame_);
    avcodec_flush_buffers(codecCtx_);
    avcodec_free_context(&codecCtx_);
    codec_ = nullptr;
    codecCtx_ = nullptr;

    // 重置所有状态变量，这对于多次录制至关重要
    videoPts_ = 0;
    is_initialized_ = false;
    resetEncodeStats();
}


void VEncoder::setBitrate(int targetBitrate)
{
    if(!codecCtx_) return;

    codecCtx_->bit_rate = targetBitrate;

    if(codec_ && codec_->name && QString(codec_->name).contains("nvenc")){
        av_opt_set_int(codecCtx_->priv_data, "max_bitrate", targetBitrate, 0);
        av_opt_set_int(codecCtx_->priv_data, "rate_control", targetBitrate, 0);
    }else {
        av_opt_set_int(codecCtx_->priv_data, "bitrate", targetBitrate / 1000, 0);
        av_opt_set_int(codecCtx_->priv_data, "vbv-maxrate", targetBitrate / 1000, 0);
        av_opt_set_int(codecCtx_->priv_data, "bufsize", (targetBitrate / 1000) * 2, 0);
    }
    qInfo() << "VEncoder::setBitrate ->" << targetBitrate << "bps";
}

QString VEncoder::selectEncoderByFormat(const QString &format)
{
    if (format == "mp4" || format == "mov") {
        // MP4推荐H.264或H.265
        return "h264_nvenc";  // 优先使用H.264，兼容性更好
    } else if (format == "mkv" || format == "webm") {
        // MKV支持更多编码器，推荐H.265
        return "hevc_nvenc";
    } else if (format == "flv" || format == "rtmp") {
        // 流媒体常用H.264
        return "h264_nvenc";
    } else if (format == "mpegts") {
        // TS格式两者都支持，优先H.264
        return "h264_nvenc";
    }

    // 未知格式默认使用H.264
    return "h264_nvenc";
}


double VEncoder::getAverageEncodeDelay() const {
    std::lock_guard<std::mutex> lock(delayStatsMutex_);
    if (encodedFrameCount_ == 0) return 0.0;
    return static_cast<double>(totalEncodeTime_) / encodedFrameCount_;
}

// 获取最近N帧的平均延迟（毫秒，用于实时监控）
double VEncoder::getRecentAverageDelay() const {
    std::lock_guard<std::mutex> lock(delayStatsMutex_);
    if (recentDelays_.empty()) return 0.0;
    qint64 sum = 0;
    for (qint64 d : recentDelays_) sum += d;
    return static_cast<double>(sum) / recentDelays_.size();
}

// 重置统计数据（如重新开始编码时）
void VEncoder::resetEncodeStats() {
    std::lock_guard<std::mutex> lock(delayStatsMutex_);
    encodedFrameCount_ = 0;
    delayedFrameCount_ = 0;
    totalEncodeTime_ = 0;
    recentDelays_.clear();
}

