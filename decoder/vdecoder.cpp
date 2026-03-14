#include "decoder/vdecoder.h"
#include "cuda.h"
#include "cuda_gl_interop.h"
#include "resample/vrescaler.h"
#include "queue/framequeue.h"
#include "pool/gloabalpool.h"
#include "sync/globalclock.h"
#include "sync/mediaclock.h"
#include "source/video/localvideosource.h"
#include "filter/mediaspeedfilter.h"
#include <QDebug>

static enum AVPixelFormat hwPixFmt_;
static enum AVPixelFormat get_hw_format(AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts)
{
    const enum AVPixelFormat* p;
    for (p = pix_fmts; *p != -1; p++) {
        if (*p == hwPixFmt_)
            return *p;
    }

    fprintf(stderr, "Failed to get HW surface format.\n");
    return AV_PIX_FMT_NONE;
}

VDecoder::VDecoder(VideoSource* source):videoSource_(source),m_stop(false) {}

VDecoder::~VDecoder()
{
    close();
}

void VDecoder::init(AVStream *stream,FrameQueue* frameQueue)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    stream_ = stream;
    frameQueue_ = frameQueue;
    m_stop.store(false,std::memory_order_release);

    const AVCodec* decoder = nullptr;
    if(stream_->codecpar == nullptr)
    {
        return;
    }

    if(hwFlag){
        // 根据流的实际编码ID选择解码器
        switch (stream_->codecpar->codec_id) {
        case AV_CODEC_ID_H264:
            decoder = avcodec_find_decoder_by_name("h264_cuvid");
            break;
        case AV_CODEC_ID_MJPEG:
            decoder = avcodec_find_decoder_by_name("mjpeg_cuvid"); // NVIDIA MJPEG硬件解码
            break;
        case AV_CODEC_ID_HEVC:
            decoder = avcodec_find_decoder_by_name("hevc_cuvid");
            break;
        default:
            qDebug() << "Unsupported codec for HW accel:"
                     << avcodec_get_name(stream_->codecpar->codec_id);
            hwFlag = false; // 自动回退到软件解码
            decoder = avcodec_find_decoder(stream_->codecpar->codec_id);
        }

        if (!decoder) {
            qDebug() << "HW decoder not found, fallback to software";
            decoder = avcodec_find_decoder(stream_->codecpar->codec_id);
            hwFlag = false;
        }

        for (int i = 0;; i++) {
            const AVCodecHWConfig* config = avcodec_get_hw_config(decoder, i);
            if (!config) {
                qDebug() << "Decoder does not support CUDA.";
                break;
            }
            if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) &&
                config->device_type == hwType_) {
                hwPixFmt_ = config->pix_fmt;
                hw_pix_fmt_ = hwPixFmt_;
                break;
            }
        }
    }
    else
    {
        decoder = avcodec_find_decoder(stream->codecpar->codec_id);
    }

    if(!decoder)
    {
        qDebug() << "could not find decoder!";
        return;
    }

    codecCtx_ = avcodec_alloc_context3(decoder);
    if(!codecCtx_)
    {
        qDebug() << "fail to alloc codecCtx_!";
        avcodec_free_context(&codecCtx_);
        return;
    }

    int ret = avcodec_parameters_to_context(codecCtx_,stream->codecpar);
    if(ret < 0)
    {
        printError(ret);
        avcodec_free_context(&codecCtx_);
        return;
    }

    AVDictionary* codec_opts = nullptr;
    if(!hwFlag)
    {
        codecCtx_->thread_count = av_cpu_count();
        av_dict_set(&codec_opts, "fast", "1", 0);            // 启用快速解码模式
    }
    else
    {
        av_dict_set(&codec_opts, "low_latency", "1", 0);
        // 创建硬件设备上下文
        ret = av_hwdevice_ctx_create(&hwDeviceCtx_, hwType_, nullptr, nullptr, 0);
        if (ret < 0) {
            printError(ret);
            return;
        }
        codecCtx_->hw_device_ctx = av_buffer_ref(hwDeviceCtx_);
        codecCtx_->get_format = get_hw_format;                  // 注册格式选择函数
    }

    ret = avcodec_open2(codecCtx_,decoder,&codec_opts);
    if(ret < 0)
    {
        printError(ret);
        avcodec_free_context(&codecCtx_);
        return;
    }

    if(videoSource_->type() == VideoSourceType::Media)
    {
        LocalVideoSource* mediaSource = dynamic_cast<LocalVideoSource*>(videoSource_);
        hasAudio_ = mediaSource && (mediaSource->getAssociatedAudioSource() != nullptr);
    }

    qDebug() << "vDecoder init sucess!";
    av_dict_free(&codec_opts);


}

void VDecoder::initRescaler()
{
    rescale->init(vInParams_,vOutParams_);
}

void VDecoder::initVideoParams(AVFrame *frame)
{
    vInParams_->timeBase = stream_->time_base;
    if(codecCtx_->framerate.den == 0 || codecCtx_->framerate.num == 0){
        vInParams_->frameRate = stream_->avg_frame_rate;
        codecCtx_->framerate = stream_->avg_frame_rate; //调整frameRate
    }
    if(hwFlag)
        vInParams_->pixFmt = AV_PIX_FMT_NV12;
    else
        vInParams_->pixFmt = codecCtx_->pix_fmt;

    vInParams_->width = frame->width;
    vInParams_->height = frame->height;

    memcpy(vOutParams_,vInParams_,sizeof(VideoParams));
    vOutParams_->pixFmt = AV_PIX_FMT_YUV420P;
}

void VDecoder::printError(int ret)
{
    char errorBuffer[AV_ERROR_MAX_STRING_SIZE];
    int res = av_strerror(ret,errorBuffer,sizeof errorBuffer);
    if(res < 0){
        qDebug() << "Unknow Error!";
    }
    else{
        qDebug() << "Error:" << errorBuffer;
    }
}


void VDecoder::decode(AVPacket *packet)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(codecCtx_ == nullptr){
        return;
    }

    // 初始化帧序列
    if(seekSync_)
    {
        localSerial_ = seekSync_->serial.load(std::memory_order_acquire);
    }else
    {
        localSerial_ = 0;
    }

    // 暂停等待
    while(m_paused && !m_stop)
    {
        std::unique_lock<std::mutex> pauseLock(pauseMutex_);
        pauseCond_.wait(pauseLock,[this]{ return !m_paused || m_stop; });
        pauseLock.unlock();
    }

    if(m_stop.load()) return;

    int ret = avcodec_send_packet(codecCtx_,packet);
    if(ret < 0 && ret != AVERROR(EAGAIN))
    {
        printError(ret);
        avcodec_free_context(&codecCtx_);
        return;
    }

    AVFrame* frame = GlobalPool::getFramePool().get();

    while(ret >= 0)
    {
        if(m_stop.load(std::memory_order_acquire)){
            break;
        }

        while (m_paused && !m_stop) {
            std::unique_lock<std::mutex> pauseLock(pauseMutex_);
            pauseCond_.wait(pauseLock, [this] { return !m_paused || m_stop; });
        }

        ret = avcodec_receive_frame(codecCtx_,frame);

        if(ret < 0){
            if(ret == AVERROR_EOF){
                break;
            }
            else if(ret == AVERROR(EAGAIN)){
                continue;
            }
            else{
                printError(ret);
                GlobalPool::getFramePool().recycle(frame);
                avcodec_free_context(&codecCtx_);
                return;
            }
        }
        else
        {
            // 首帧同步（匹配解码开启前后时间差）
            if (isFirstFrame) {
                isFirstFrame = false;
                if (syncClock_) {
                    syncClock_->setVideoStarted(true);
                    qDebug() << "视频第一帧解码完成，通知音频线程";

                }
            }

            // 初始化视频参数
            if(vInParams_ == nullptr)
            {
                vInParams_ = new VideoParams();
                vOutParams_ = new VideoParams();
                initVideoParams(frame);
                if(vInParams_->pixFmt != vOutParams_->pixFmt && frame->format != AV_PIX_FMT_CUDA)
                {
                    rescale = new VRescaler();
                    initRescaler();
                }
            }

            // 处理倍速
            // if(!hasAudio_ && videoSource_->type() == VideoSourceType::Media)
            // {
            //     AVFrame* filteredFrame = GlobalPool::getFramePool().get();
            //     int ret =  ->getVideoFrame(frame, filteredFrame);
            //     if (ret != 0) {
            //         GlobalPool::getFramePool().recycle(filteredFrame);
            //         av_frame_unref(frame);
            //         continue;
            //     }

            //     av_frame_unref(frame);
            //     frame = filteredFrame;
            // }


            // 当前帧PTS
            int64_t framePtsUs = AV_NOPTS_VALUE;
            if (frame->pts != AV_NOPTS_VALUE) {
                double ptsSec = frame->pts * av_q2d(stream_->time_base);
                framePtsUs = static_cast<int64_t>(ptsSec * 1000000);
            } else {
                int64_t masterUs = syncClock_ ? syncClock_->getMaterClockPts() : 0;
                framePtsUs = (masterUs > 0) ? masterUs : GlobalClock::getInstance().getCurrentUs();
            }

            // 判断帧序列是否过期（过期丢弃）
            if (seekSync_) {
                int currGen = seekSync_->serial.load(std::memory_order_acquire);
                if (currGen != localSerial_) {
                    av_frame_unref(frame);
                    continue;
                }
            }

            // 等待音频PTS赶上targetUs
            if (seekSync_ && seekSync_->seeking.load(std::memory_order_acquire)) {
                int64_t targetUs = seekSync_->targetUs.load(std::memory_order_acquire);
                const int64_t TOLERANCE_US = 10000; // 10ms
                if (framePtsUs + TOLERANCE_US < targetUs) {
                    // 丢弃此帧
                    av_frame_unref(frame);
                    continue;
                } else {
                    // 到达或超过 target，标记 video ready
                    seekSync_->vReady.store(true, std::memory_order_release);
                    if (!hasAudio_) {
                        seekSync_->seeking.store(false, std::memory_order_release);
                        qDebug() << "纯视频seek完成，无需等待音频";
                    } else {
                        if (seekSync_->aReady.load(std::memory_order_acquire)) {
                            seekSync_->seeking.store(false, std::memory_order_release);
                            qDebug() << "音视频seek完成";
                        }

                    }
                }
            }
            if(syncClock_ && videoSource_->type() == VideoSourceType::Media)
            {
                int64_t master = syncClock_->getMaterClockPts();
                int64_t diff = framePtsUs - master;
                // qDebug() << "mster : " << master << "framePts" << framePtsUs << "diff" << diff;
                const int64_t MAX_VIDEO_SLEEP_US = 50000; // 50ms
                const int64_t REANCHOR_VIDEO_US = 200000; // 200ms

                bool seekingNow = seekSync_ && seekSync_->seeking.load(std::memory_order_acquire);
                if (!seekingNow) {

                    if (diff > REANCHOR_VIDEO_US && !hasAudio_) {
                        syncClock_->seekTo(framePtsUs);
                        // qDebug() << "[VDecoder] 纯视频模式，重锚定时钟到" << framePtsUs;
                    }

                    else if (diff > 0) {
                        int64_t sleepUs = std::min(diff, MAX_VIDEO_SLEEP_US);

                        if (diff > REANCHOR_VIDEO_US) {
                            sleepUs = std::min(sleepUs * 2, diff);
                        }
                        std::this_thread::sleep_for(std::chrono::microseconds(sleepUs));
                        // qDebug() << "[VDecoder] 视频领先" << diff << "us，sleep" << sleepUs << "us减速";
                    }
                }

                if (!hasAudio_) {
                    syncClock_->setMediaVPts(framePtsUs); // 纯视频场景才更新视频PTS
                }
            }

            if (frame->format == AV_PIX_FMT_CUDA) {
                AVHWFramesContext* framesCtx = (AVHWFramesContext*)frame->hw_frames_ctx->data;
                AVPixelFormat swFormat = framesCtx->sw_format;
                if (swFormat != AV_PIX_FMT_NV12) {
                    qWarning() << "Unsupported internal format inside AV_PIX_FMT_CUDA: " << swFormat;
                    continue;
                }

                CudaFrameInfo info;
                info.timestamp = GlobalClock::getInstance().getCurrentUs();
                info.type = VideoSourceType::Camera;
                info.format = FrameFormat::NV12;
                info.yPlane = (CUdeviceptr)frame->data[0];
                info.uvPlane = (CUdeviceptr)frame->data[1];
                info.pitchY = frame->linesize[0];
                info.pitchUV = frame->linesize[1];
                info.width = frame->width;
                info.height = frame->height;
                info.sourceId = videoSource_->sourceId();
                info.sceneId = videoSource_->sceneId();
                info.priority = videoSource_->priority();

                if(!m_stop)
                {
                    emit frameReady(info);
                }
                av_frame_unref(frame);
            }
            else
            {
                if(rescale)
                {
                    AVFrame* swsFrame = nullptr;
                    rescale->rescale(frame,&swsFrame);
                    av_frame_unref(frame);

                    if(m_stop.load(std::memory_order_acquire)){
                        GlobalPool::getFramePool().recycle(swsFrame);
                        m_stop.store(false,std::memory_order_release);
                        break;
                    }
                    else{
                        AVFrame* decoderFrame = GlobalPool::getFramePool().get();
                        av_frame_move_ref(decoderFrame, swsFrame);

                        // // 再次检查 gen & seeking
                        // if (seekSync_) {
                        //     int currGen3 = seekSync_->serial.load(std::memory_order_acquire);
                        //     if (currGen3 != localSerial_) {
                        //         GlobalPool::getFramePool().recycle(swsFrame);
                        //         GlobalPool::getFramePool().recycle(decoderFrame);
                        //         continue;
                        //     }
                        //     if (seekSync_->seeking.load(std::memory_order_acquire)) {
                        //         int64_t targetUs = seekSync_->targetUs.load(std::memory_order_acquire);
                        //         double ptsSec = decoderFrame->pts * av_q2d(stream_->time_base);
                        //         int64_t ptsUs = static_cast<int64_t>(ptsSec * 1000000);
                        //         if (ptsUs + 10000 < targetUs) {
                        //             GlobalPool::getFramePool().recycle(swsFrame);
                        //             GlobalPool::getFramePool().recycle(decoderFrame);
                        //             continue;
                        //         } else {
                        //             seekSync_->vReady.store(true, std::memory_order_release);
                        //             if (seekSync_->aReady.load(std::memory_order_acquire)) {
                        //                 seekSync_->seeking.store(false, std::memory_order_release);
                        //             }
                        //         }
                        //     }
                        // }

                        if(frameQueue_){
                            decoderFrame->opaque = (void*)GlobalClock::getInstance().getCurrentUs();
                            frameQueue_->push(decoderFrame);
                        }
                        GlobalPool::getFramePool().recycle(decoderFrame);
                        GlobalPool::getFramePool().recycle(swsFrame);
                    }

                }
                else
                {
                    if(m_stop.load(std::memory_order_acquire)){
                        av_frame_unref(frame);
                        break;
                    }
                    else
                    {
                        // 再次检查 gen/seeking before pushing
                        // if (seekSync_) {
                        //     int currGen4 = seekSync_->serial.load(std::memory_order_acquire);
                        //     if (currGen4 != localSerial_) {
                        //         av_frame_unref(frame);
                        //         continue;
                        //     }
                        //     if (seekSync_->seeking.load(std::memory_order_acquire)) {
                        //         int64_t targetUs = seekSync_->targetUs.load(std::memory_order_acquire);
                        //         double ptsSec = frame->pts * av_q2d(stream_->time_base);
                        //         int64_t ptsUs = static_cast<int64_t>(ptsSec * 1000000);
                        //         if (ptsUs + 10000 < targetUs) {
                        //             av_frame_unref(frame);
                        //             continue;
                        //         } else {
                        //             seekSync_->vReady.store(true, std::memory_order_release);
                        //             if (seekSync_->aReady.load(std::memory_order_acquire)) {
                        //                 seekSync_->seeking.store(false, std::memory_order_release);
                        //             }
                        //         }
                        //     }
                        // }

                        if(frameQueue_){
                            AVFrame* decoderFrame = GlobalPool::getFramePool().get();
                            if (!decoderFrame) {
                                qWarning() << "Failed to get frame from pool";
                                av_frame_unref(frame);
                                continue;
                            }
                            decoderFrame->format = frame->format;
                            decoderFrame->width = frame->width;
                            decoderFrame->height = frame->height;
                            decoderFrame->ch_layout = frame->ch_layout;
                            av_frame_get_buffer(decoderFrame, 0); // 分配缓冲区
                            av_frame_copy(decoderFrame, frame);   // 复制数据
                            av_frame_copy_props(decoderFrame, frame); // 复制属性（PTS等）
                            frameQueue_->push(decoderFrame);
                            GlobalPool::getFramePool().recycle(decoderFrame);
                        }

                        av_frame_unref(frame);
                    }
                }
            }


        }
    }
    GlobalPool::getFramePool().recycle(frame);
}


void VDecoder::adjustVideoRender(int64_t diffUs, CudaFrameInfo* info) {


    const int64_t MAX_WAIT_US = 50000;   // 最大等待50ms
    const int64_t MAX_DROP_US = 100000;  // 超过100ms则丢帧

    if (diffUs < -MAX_DROP_US) {
        info->skip = true; // 标记跳过渲染
    } else if (diffUs > MAX_WAIT_US) {

        int64_t waitUs = diffUs - MAX_WAIT_US;
        std::this_thread::sleep_for(std::chrono::microseconds(waitUs));
    }
}
void VDecoder::flushDecoder() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (codecCtx_) {
        avcodec_flush_buffers(codecCtx_); // 关键：清空解码器内部缓冲区
    }
    isFirstFrame = true; // 重置第一帧标志
}

void VDecoder::setClock(MediaClock *clock)
{
    syncClock_ = clock;
}

void VDecoder::setSpeedFilter(MediaSpeedFilter *speedFilter)
{
    speedFilter_ = speedFilter;
}

int VDecoder::getDuration()
{
    return static_cast<int>(stream_->duration * av_q2d(stream_->time_base));
}

VideoParams *VDecoder::getVideoPars()
{
    return vOutParams_;
}

AVCodecContext *VDecoder::getCodecCtx()
{
    return codecCtx_;
}

AVStream *VDecoder::getStream()
{
    return stream_;
}

void VDecoder::stop()
{
    m_stop.store(true,std::memory_order_release);
    pauseCond_.notify_all();
}

void VDecoder::pause()
{
    std::lock_guard<std::mutex> lock(pauseMutex_);
    m_paused.store(true);
    pauseCond_.notify_all();
}


void VDecoder::resume()
{
    std::lock_guard<std::mutex> lock(pauseMutex_);
    m_paused.store(false);
    pauseCond_.notify_all();
}

void VDecoder::flushQueue()
{
    frameQueue_->clear();
}
void VDecoder::close() {
    decode(nullptr);
    stop();

    std::lock_guard<std::mutex> lock(m_mutex);

    if(codecCtx_) {
        av_buffer_unref(&codecCtx_->hw_device_ctx);
        avcodec_free_context(&codecCtx_);
        codecCtx_ = nullptr;
    }

    if(hwDeviceCtx_) {
        av_buffer_unref(&hwDeviceCtx_);
        hwDeviceCtx_ = nullptr;
    }

    if(vInParams_) {
        delete vInParams_;
        vInParams_ = nullptr;
    }
    if(vOutParams_) {
        delete vOutParams_;
        vOutParams_ = nullptr;
    }

    if(rescale) {
        delete rescale;
        rescale = nullptr;
    }

    isFirstFrame = true;
    m_stop.store(false, std::memory_order_release);
    m_paused.store(false);
    hwFlag = false;
    qDebug() << "VDecoder closed!";
}



