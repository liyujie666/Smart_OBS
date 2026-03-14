#include "aencoder.h"
#include "pool/gloabalpool.h"
#include "queue/packetqueue.h"
#include "sync/avsyncclock.h"
#include "mixer/audiomixer.h"
#include <QDebug>

AEncoder::AEncoder(PacketQueue* pktQueue):aEnPktQueue_(pktQueue)
{

}

AEncoder::~AEncoder()
{
    close();
}

bool AEncoder::init(const AudioEncodeConfig& config) {
    // 查找编码器（优先使用更稳定的编码器）
    config_ = config;
    qDebug() << "sampleRate : " << config.sampleRate
             << "nbChannels : " << config.nbChannels
             << "bitRate : " << config.bitRate
             << "format : " << config.sampleFmt;

    // 明确指定使用LC-AAC profile，提高播放器兼容性
    codec_ = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!codec_) {
        qDebug() << "AAC encoder not found";
        return false;
    }

    // 分配编码上下文
    codecCtx_ = avcodec_alloc_context3(codec_);
    if(!codecCtx_)
    {
        qDebug() << "audio avcodec_alloc_context3 failed!";
        return false;
    }

    // 核心参数配置（优化同步精度）
    codecCtx_->sample_rate = config.sampleRate;
    av_channel_layout_default(&codecCtx_->ch_layout, config.nbChannels);
    codecCtx_->sample_fmt = config.sampleFmt;
    codecCtx_->bit_rate = config.bitRate;
    codecCtx_->time_base = {1, config.sampleRate};  // 时间基与采样率严格匹配
    codecCtx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // 关键优化：设置AAC特定参数，解决兼容性问题
    codecCtx_->profile = FF_PROFILE_AAC_LOW;  // 强制使用LC-AAC，所有播放器都支持
    codecCtx_->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;  // 兼容更多播放器
    codecCtx_->frame_size = 1024;  // 固定帧大小，确保音频帧时长一致（关键！）

    // 编码器选项：使用恒定码率，减少音频波动
    AVDictionary *opts = nullptr;
    av_dict_set(&opts, "aac_coder", "twoloop", 0);  // 高质量编码
    av_dict_set(&opts, "rc", "cbr", 0);  // 恒定码率，避免音量波动

    if (avcodec_open2(codecCtx_, codec_, &opts) < 0) {
        qDebug() << "Fail to open audio encoder!";
        av_dict_free(&opts);
        return false;
    }
    av_dict_free(&opts);

    // 分配AVFrame（确保样本数与编码器匹配）
    frame_ = GlobalPool::getFramePool().get();
    if (!frame_) {
        qDebug() << "Failed to get AVFrame from pool";
        return false;
    }
    frame_->nb_samples = codecCtx_->frame_size;  // 使用编码器固定帧大小
    frame_->format = codecCtx_->sample_fmt;
    frame_->ch_layout = codecCtx_->ch_layout;
    frame_->sample_rate = codecCtx_->sample_rate;
    frame_->pts = AV_NOPTS_VALUE;  // 初始化为无效值，确保后续正确设置

    // 分配帧缓冲区（增加对齐参数，提高处理效率）
    if (av_frame_get_buffer(frame_, 32) < 0) {
        qDebug() << "Failed to alloc AVFrame buffer";
        return false;
    }

    is_initialized_ = true;
    return true;
}

bool AEncoder::encode(AVFrame* frame) {
    if (!frame || !is_initialized_ || !codecCtx_) {
        qDebug() << "Invalid frame or encoder not initialized";
        return false;
    }

    int frame_size = codecCtx_->frame_size;
    if (frame_size <= 0) {
        frame_size = 1024; // AAC默认帧大小
        qDebug() << "Using default frame_size:" << frame_size;
    }

    const int total_samples = frame->nb_samples;
    const int channels = frame->ch_layout.nb_channels;
    const AVSampleFormat sample_fmt = (AVSampleFormat)frame->format;
    const int total_frames = (total_samples + frame_size - 1) / frame_size;

    int total_sent = 0;
    bool success = true;
    // 外部时钟的基准PTS（微秒），转换为音频采样数（时基：1/sample_rate）
    int64_t baseAudioPts = av_rescale_q(
        frame->pts, // 来自外部时钟的PTS（微秒）
        AV_TIME_BASE_Q, // 外部时钟时基（微秒）
        {1, codecCtx_->sample_rate} // 音频编码器时基（采样数/秒）
        );


    for (int i = 0; i < total_frames; ++i) {
        int current_samples = qMin(frame_size, total_samples - total_sent);

        AVFrame* split_frame = GlobalPool::getFramePool().get();
        split_frame->format = frame->format;
        split_frame->sample_rate = frame->sample_rate;
        av_channel_layout_copy(&split_frame->ch_layout, &frame->ch_layout);
        split_frame->nb_samples = current_samples;

        if (av_frame_get_buffer(split_frame, 0) < 0) {
            qDebug() << "Failed to allocate split frame buffer";
            GlobalPool::getFramePool().recycle(split_frame);
            success = false;
            break;
        }

        if (!copyFrameData(split_frame, frame, total_sent, current_samples)) {
            qDebug() << "Failed to copy frame data";
            GlobalPool::getFramePool().recycle(split_frame);
            success = false;
            break;
        }

        // 补静音（保持不变）
        if (current_samples < frame_size) {
            int silence_offset = current_samples;
            int silence_count = frame_size - current_samples;
            av_samples_set_silence(
                split_frame->extended_data, silence_offset, silence_count,
                channels, sample_fmt
                );
        }

        // 关键修改：分片PTS基于外部时钟基准计算
        split_frame->pts = baseAudioPts + total_sent;

        if (!sendSingleFrame(split_frame)) {
            qDebug() << "Failed to send split frame";
            GlobalPool::getFramePool().recycle(split_frame);
            success = false;
            break;
        }

        GlobalPool::getFramePool().recycle(split_frame);
        total_sent += current_samples;
    }

    return success;
}

bool AEncoder::sendSingleFrame(AVFrame* frame) {

    // 发送帧到编码器
    int ret = avcodec_send_frame(codecCtx_, frame);
    if (ret < 0) {
        char errbuf[1024];
        av_strerror(ret, errbuf, sizeof(errbuf));
        qDebug() << "Error sending frame! ret=" << ret << "msg:" << errbuf;
        return false;
    }

    // 接收编码后的数据包
    AVPacket* pkt = GlobalPool::getPacketPool().get();
    while (ret >= 0) {
        ret = avcodec_receive_packet(codecCtx_, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            char errbuf[1024];
            av_strerror(ret, errbuf, sizeof(errbuf));
            qDebug() << "Failed to receive packet! ret=" << ret << "msg:" << errbuf;
            GlobalPool::getPacketPool().recycle(pkt);
            return false;
        }

        aEnPktQueue_->push(pkt);
        // qDebug() << "audioPts" << pkt->pts;
        // qDebug() << "aEnPktQueue_ " << aEnPktQueue_ << "size : " << aEnPktQueue_->size();
        pkt = GlobalPool::getPacketPool().get();
    }

    GlobalPool::getPacketPool().recycle(pkt);
    return true;
}


bool AEncoder::copyFrameData(AVFrame* dst, AVFrame* src, int offset_samples, int nb_samples) {
    if (!dst || !src || offset_samples < 0 || nb_samples <= 0) {
        return false;
    }

    // 获取样本格式的字节数
    int bytes_per_sample = av_get_bytes_per_sample((AVSampleFormat)src->format);
    if (bytes_per_sample <= 0) {
        qDebug() << "Invalid sample format";
        return false;
    }

    // 判断是planar格式（如FLTP）还是packed格式（如S16）
    bool is_planar = av_sample_fmt_is_planar((AVSampleFormat)src->format);
    int channels = src->ch_layout.nb_channels;

    if (is_planar) {
        // Planar格式：每个声道单独存储（data[0] = 左声道，data[1] = 右声道...）
        for (int ch = 0; ch < channels; ch++) {
            // 源数据起始位置：偏移offset_samples个样本
            uint8_t* src_data = src->data[ch] + offset_samples * bytes_per_sample;
            // 目标数据位置
            uint8_t* dst_data = dst->data[ch];
            // 复制当前声道的nb_samples个样本
            memcpy(dst_data, src_data, nb_samples * bytes_per_sample);
        }
    } else {
        // Packed格式：所有声道交错存储（如LRLLRRLRLR...）
        // 每个样本块大小 = 声道数 * 字节数/样本
        int bytes_per_block = channels * bytes_per_sample;
        // 源数据起始位置：偏移offset_samples个样本块
        uint8_t* src_data = src->data[0] + offset_samples * bytes_per_block;
        // 目标数据位置
        uint8_t* dst_data = dst->data[0];
        // 复制nb_samples个样本块
        memcpy(dst_data, src_data, nb_samples * bytes_per_block);
    }

    return true;
}


bool AEncoder::encodeFlushFrame(AVFrame* frame,MuxerManager* muxerManager) {
    if (!codecCtx_ || codecCtx_->codec == nullptr) return false;

    // 检查编码器状态
    if (avcodec_is_open(codecCtx_) <= 0) {
        return false;
    }

    // qDebug() << "encodeFlushFrame pts " << frame->pts;
    int64_t baseAudioPts = av_rescale_q(
        frame->pts, // 来自外部时钟的PTS（微秒）
        AV_TIME_BASE_Q, // 外部时钟时基（微秒）
        {1, codecCtx_->sample_rate} // 音频编码器时基（采样数/秒）
        );

    frame->pts = baseAudioPts;
    int ret = avcodec_send_frame(codecCtx_, frame);
    if (ret < 0) {
        // 特殊处理EOF错误
        if (ret == AVERROR_EOF) {
            qDebug() << "Encoder already flushed, skipping frame";
            return true; // 不是致命错误
        }

        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        qWarning() << "Error sending flush frame:" << errbuf;
        return false;
    }

    // 接收所有可用包
    while (true) {
        AVPacket* pkt = GlobalPool::getPacketPool().get();
        ret = avcodec_receive_packet(codecCtx_, pkt);

        if (ret == AVERROR_EOF) {
            qDebug() << "AVERROR_EOF";
            break;
        }
        else if (ret == AVERROR(EAGAIN)) {
            qDebug() << "AVERROR(EAGAIN)";
            GlobalPool::getPacketPool().recycle(pkt);
            break;
        }

        if (ret < 0) {
            GlobalPool::getPacketPool().recycle(pkt);
            return false;
        }
        // 处理包...
        muxerManager->writePacket(pkt,AVMEDIA_TYPE_AUDIO);
        qDebug() << "last pkt write sucessfully!";
        GlobalPool::getPacketPool().recycle(pkt);
    }

    return true;
}

void AEncoder::flush(MuxerManager* muxerManager)
{
    if (!is_initialized_ || !codecCtx_) return;

    // 1. 发送NULL帧通知编码器结束输入
    int ret = avcodec_send_frame(codecCtx_, nullptr);
    if (ret < 0 && ret != AVERROR_EOF) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        qWarning() << "avcodec_send_frame(NULL) failed:" << errbuf;
        return;
    }

    // 2. 循环提取所有缓存的数据包（关键修复）
    AVPacket* pkt = GlobalPool::getPacketPool().get();
    while (true) {
        ret = avcodec_receive_packet(codecCtx_, pkt);
        if (ret == AVERROR_EOF) {
            qDebug() << "编码器缓存已完全提取";
            break;
        } else if (ret == AVERROR(EAGAIN)) {
            // 暂时无数据，等待后重试（理论上发送NULL后不会出现）
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        } else if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, sizeof(errbuf));
            qWarning() << "avcodec_receive_packet failed:" << errbuf;
            break;
        }

        // 写入复用器
        muxerManager->writePacket(pkt, AVMEDIA_TYPE_AUDIO);
        qDebug() << "冲刷编码器包，pts=" << pkt->pts;

        // 回收并获取新包
        GlobalPool::getPacketPool().recycle(pkt);
        pkt = GlobalPool::getPacketPool().get();
    }
    GlobalPool::getPacketPool().recycle(pkt);

    aEnPktQueue_->close();
}

void AEncoder::close()
{
    // 清空队列
    aEnPktQueue_->clear();

    // 释放FFmpeg资源
    GlobalPool::getFramePool().recycle(frame_);
    avcodec_free_context(&codecCtx_);
    codec_ = nullptr;
    codecCtx_ = nullptr;

    is_initialized_ = false;
}
