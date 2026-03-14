#include "adecoder.h"
#include <immintrin.h>
#include "queue/framequeue.h"
#include "pool/gloabalpool.h"
#include "sync/globalclock.h"
#include "sync/mediaclock.h"
#include "source/audio/audiosource.h"
#include "source/audio/mediaaudiosource.h"
#include "filter/mediaspeedfilter.h"
#include "monitor/audiomonitor.h"
#include <QDebug>

ADecoder::ADecoder(AudioSource* audioSource_):audioSource_(audioSource_)
{

}

ADecoder::~ADecoder()
{
    close();
}

void ADecoder::init(AVStream *stream, FrameQueue *frameQueue)
{
    std::lock_guard<std::mutex> lock(mutex_);
    m_stop = false;
    stream_ = stream;
    frameQueue_ = frameQueue;

    if(!stream_->codecpar) return;

    const AVCodec* decoder = avcodec_find_decoder(stream_->codecpar->codec_id);
    if(!decoder)
    {
        qDebug() << "Could not find Audio Decoder!";
        return;
    }

    codecCtx_ = avcodec_alloc_context3(decoder);
    if(!codecCtx_)
    {
        qDebug() << "Audio Codec Context alloc failed!";
        avcodec_free_context(&codecCtx_);
        return;
    }

    int ret = avcodec_parameters_to_context(codecCtx_,stream_->codecpar);
    if(ret < 0)
    {
        printError(ret);
        avcodec_free_context(&codecCtx_);
        return;
    }

    ret = avcodec_open2(codecCtx_,decoder,nullptr);
    if(ret < 0)
    {
        printError(ret);
        avcodec_free_context(&codecCtx_);
        return;
    }

    if(audioSource_->type() == AudioSourceType::Media)
    {
        MediaAudioSource* mediaSource = dynamic_cast<MediaAudioSource*>(audioSource_);
        hasVideo_ = mediaSource && (mediaSource->getAssociatedVideoSource() != nullptr);
    }

    audioMonitor_ = new AudioMonitor(this);
    connect(this,&ADecoder::frameOutReady,audioMonitor_,&AudioMonitor::playAudio);
    connect(AudioMixerDialog::getInstance(),&AudioMixerDialog::listenModeChanged,this,&ADecoder::onListenModeChanged);
    connect(AudioMixerDialog::getInstance(),&AudioMixerDialog::listenModeChanged,audioMonitor_,&AudioMonitor::setListenMode);
}

void ADecoder::decode(AVPacket *packet)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if(codecCtx_ == nullptr || m_stop.load()){
        GlobalPool::getPacketPool().recycle(packet);
        return;
    }

    // 设置帧序列
    if (seekSync_) {
        localSerial_ = seekSync_->serial.load(std::memory_order_acquire);
    } else {
        localSerial_ = 0;
    }

    // 暂停
    while(m_paused && !m_stop)
    {
        std::unique_lock<std::mutex> pauseLock(pauseMutex_);
        pauseCond_.wait(pauseLock,[this]{ return !m_paused || m_stop; });
        pauseLock.unlock();
    }

    int ret = avcodec_send_packet(codecCtx_,packet);
    if(ret < 0 && ret != AVERROR(EAGAIN)){
        printError(ret);
        GlobalPool::getPacketPool().recycle(packet);
        avcodec_free_context(&codecCtx_);
        return;
    }

    AVFrame* frame = GlobalPool::getFramePool().get();
    while(ret >= 0)
    {
        while (m_paused && !m_stop) {
            std::unique_lock<std::mutex> pauseLock(pauseMutex_);
            pauseCond_.wait(pauseLock, [this] { return !m_paused || m_stop; });
        }

        ret = avcodec_receive_frame(codecCtx_,frame);
        if(ret < 0)
        {
            if(ret == AVERROR_EOF)
            {
                break;
            }else if(ret == AVERROR(EAGAIN))
            {
                continue;
            }else
            {
                printError(ret);
                GlobalPool::getFramePool().recycle(frame);
                avcodec_free_context(&codecCtx_);
                return;
            }
        }


        int64_t framePts = static_cast<int64_t>(frame->pts * av_q2d(stream_->time_base) * 1000000);

        // 音频比视频解码快，等待视频同步
        if (isFirstFrame) {
            isFirstFrame = false;
            if (syncClock_) {

                if(hasVideo_)
                {
                    qDebug() << "音频第一帧解码完成，检查视频是否已启动... PTS:" << framePts;
                    // 等待视频启动（最多等1秒）
                    bool videoStarted = syncClock_->waitForVideoStart(3000000);
                    if (videoStarted) {
                        qDebug() << "视频已启动，音频开始播放";
                    } else {
                        qWarning() << "视频启动超时（1秒），音频继续播放";
                    }
                }
            }

        }

        // 初始化帧参数
        if(!aInParams_)
        {
            aInParams_ = new AudioParams();
            aOutParams_ = new AudioParams();
            initAudioParams(frame);
            // if (speedFilter_) {
            //     auto [filterSampleRate, filterFmt, filterChLayout,filtertimebase] = speedFilter_->getAudioOutputParams();
            //     aInParams_->sampleRate = filterSampleRate;
            //     aInParams_->format = filterFmt;
            //     aInParams_->nbChannels = filterChLayout.nb_channels;
            // }
            initFifo();
            if(aInParams_->format != aOutParams_->format){
                resampler_ = new AResampler();
                initResampler();
                // printFmt();
            }
        }

        // if(audioSource_->type() == AudioSourceType::Media)
        // {
        //     AVFrame* filteredFrame = GlobalPool::getFramePool().get();
        //     int ret = speedFilter_->getAudioFrame(frame, filteredFrame);
        //     if (ret != 0) {
        //         GlobalPool::getFramePool().recycle(filteredFrame);
        //         av_frame_unref(frame);
        //         continue;
        //     }

        //     av_frame_unref(frame);
        //     frame = filteredFrame;
        // }
        // 重采样（sampleRate：48000，channels：2，format：AV_SAMPLE_FORMAT_FLTP）
        if(resampler_)
        {
            AVFrame* swrFrame = nullptr;
            resampler_->resample(frame,&swrFrame);
            {
                std::lock_guard<std::mutex> lock(ptsQueueMutex_);
                ptsQueue_.push({swrFrame->nb_samples, swrFrame->pts});  // 样本数+当前帧PTS
            }
            av_audio_fifo_write(audioFifo, (void**)swrFrame->data, swrFrame->nb_samples);
            av_frame_unref(swrFrame);
            GlobalPool::getFramePool().recycle(swrFrame);
        } else {
            {
                std::lock_guard<std::mutex> lock(ptsQueueMutex_);
                ptsQueue_.push({frame->nb_samples, frame->pts});  // 样本数+当前帧PTS
            }

            if (!frame || !frame->data[0]) {
                qWarning() << "无效的音频帧，跳过写入FIFO";
                GlobalPool::getFramePool().recycle(frame);
                continue;
            }
            av_audio_fifo_write(audioFifo, (void**)frame->data, frame->nb_samples);
        }

        extractFixedSizeFrames();
    }
    GlobalPool::getFramePool().recycle(frame);
}

void ADecoder::extractFixedSizeFrames() {

    while (av_audio_fifo_size(audioFifo) >= TARGET_SAMPLES  && !m_stop.load(std::memory_order_acquire)) {
        // 申请一个新的 AVFrame 用于存放固定大小的样本
        AVFrame* fixedFrame = GlobalPool::getFramePool().get();
        fixedFrame->format = aOutParams_->format;
        fixedFrame->ch_layout.nb_channels = aOutParams_->nbChannels;
        av_channel_layout_default(&fixedFrame->ch_layout,aOutParams_->nbChannels);
        fixedFrame->sample_rate = aOutParams_->sampleRate;
        fixedFrame->nb_samples = TARGET_SAMPLES;

        // 为帧分配数据缓冲区
        int ret = av_frame_get_buffer(fixedFrame, 0);
        if (ret < 0) {
            GlobalPool::getFramePool().recycle(fixedFrame);
            break;
        }

        ret = av_audio_fifo_read(audioFifo,
                                 (void**)fixedFrame->data,
                                 TARGET_SAMPLES);
        if (ret != TARGET_SAMPLES) {
            GlobalPool::getFramePool().recycle(fixedFrame);
            break;
        }

        int64_t currentPts = AV_NOPTS_VALUE;
        int accumulatedSamples = 0;                 // 累加已读取的样本数
        {
            std::lock_guard<std::mutex> lock(ptsQueueMutex_);

            while (!ptsQueue_.empty() && accumulatedSamples < TARGET_SAMPLES) {
                auto [frameSamples, framePts] = ptsQueue_.front();
                accumulatedSamples += frameSamples;

                currentPts = framePts;
                // 累加样本数超过目标，剩余样本需重新放回队列
                if (accumulatedSamples > TARGET_SAMPLES) {
                    int remainingSamples = accumulatedSamples - TARGET_SAMPLES;
                    ptsQueue_.pop();
                    ptsQueue_.push({remainingSamples, framePts});
                    break;
                } else {
                    ptsQueue_.pop();
                }
            }
        }

        if (currentPts != AV_NOPTS_VALUE) {
            fixedFrame->pts = currentPts;
        } else {
            fixedFrame->pts = av_rescale_q(syncClock_->getMaterClockPts() / 1000000.0,
                                           AV_TIME_BASE_Q,
                                           stream_->time_base);
            qWarning() << "未找到匹配的音频PTS，估算为：" << fixedFrame->pts;
        }

        // 判断当前帧序列是否已过期
        if (seekSync_) {
            int currGen = seekSync_->serial.load(std::memory_order_acquire);
            if (currGen != localSerial_) {
                GlobalPool::getFramePool().recycle(fixedFrame);
                continue;
            }
        }

        // 调节音量增益
        applyVolumeGainAVX2(fixedFrame, getVolumeGain());
        m_dB = calculateRMSdB(fixedFrame);



        // 暂停判断
        if (m_stop.load(std::memory_order_acquire)) {
            GlobalPool::getFramePool().recycle(fixedFrame);
            m_stop.store(false, std::memory_order_release);
            break;
        }

        //
        if(syncClock_ && audioSource_->type() == AudioSourceType::Media) {
            int64_t framePts = fixedFrame->pts;
            double ptsSec = framePts * av_q2d(stream_->time_base);
            int64_t ptsUs = static_cast<int64_t>(ptsSec * 1000000);

            // qDebug() << "audio pts " << ptsUs;

            // 屏蔽期，等待视频和音频pts同步
            if (seekSync_ && seekSync_->seeking.load(std::memory_order_acquire)) {
                int64_t targetUs = seekSync_->targetUs.load(std::memory_order_acquire);
                const int64_t TOLERANCE_US = 10000; // 10ms
                qDebug() << "seeking time frame pts : " << ptsUs;
                if (ptsUs + TOLERANCE_US < targetUs) {
                    GlobalPool::getFramePool().recycle(fixedFrame);
                    continue;
                } else {

                    if (!hasVideo_) {
                        seekSync_->seeking.store(false, std::memory_order_release);
                        qDebug() << "纯音频seek完成，无需等待视频";
                    } else {

                        syncClock_->waitForVideoFirstFrame(5000); // 1秒超时
                        if (seekSync_->vReady.load(std::memory_order_acquire)) {
                            seekSync_->seeking.store(false, std::memory_order_release);
                            qDebug() << "音视频seek完成";
                        }
                    }
                }
            }

            int64_t diffUs  = syncClock_->setMediaAPts(ptsUs);
            // qDebug() << "audio diffUs " << diffUs;
            // 只有非 seeking 时才sleep
            if(!(seekSync_ && seekSync_->seeking.load(std::memory_order_acquire)) && diffUs > 0)
            {
                std::this_thread::sleep_for(std::chrono::microseconds(diffUs));
            }
        }

        // if(audioSource_->type() == AudioSourceType::Media)
        // {

        // }

        switch (m_listenMode) {
        case MixerListenMode::CLOSE:
            break;
        case MixerListenMode::LISTEN_MUTE:
        {
            AVFrame* swrFrame = convertFLTPToFLT(fixedFrame);
            if (swrFrame) {
                int dataSize = av_samples_get_buffer_size(
                    nullptr,
                    swrFrame->ch_layout.nb_channels,
                    swrFrame->nb_samples,
                    AV_SAMPLE_FMT_FLT,
                    1
                    );
                QByteArray pcmData((char*)swrFrame->data[0], dataSize);
                emit frameOutReady(pcmData);
                GlobalPool::getFramePool().recycle(swrFrame);
            }
            makeFLTPFrameMute(fixedFrame);
            break;
        }
        case MixerListenMode::LISTEN_OUTPUT:
        {
            AVFrame* swrFrame = convertFLTPToFLT(fixedFrame);
            if (swrFrame) {
                int dataSize = av_samples_get_buffer_size(
                    nullptr,
                    swrFrame->ch_layout.nb_channels,
                    swrFrame->nb_samples,
                    AV_SAMPLE_FMT_FLT,
                    1
                    );
                QByteArray pcmData((char*)swrFrame->data[0], dataSize);
                emit frameOutReady(pcmData);
                GlobalPool::getFramePool().recycle(swrFrame);
            }
            break;
        }
        default:
            break;
        }

        // if(!m_isActived.load() && audioSource_->type() == AudioSourceType::Media)
        // {
        //     GlobalPool::getFramePool().recycle(fixedFrame);
        //     break;
        // }

        if (m_isRecording) {
            fixedFrame->opaque = (void*)GlobalClock::getInstance().getCurrentUs();
            frameQueue_->push(fixedFrame);
        }

        GlobalPool::getFramePool().recycle(fixedFrame);
    }
}

bool ADecoder::makeFLTPFrameMute(AVFrame *fixedFrame)
{
    if(!fixedFrame || fixedFrame->format != AV_SAMPLE_FMT_FLTP)
    {
        qDebug() << "fixedFrame is not supported!";
        return false;
    }

    for (int ch = 0; ch < fixedFrame->ch_layout.nb_channels; ++ch) {
        float* samples = (float*)fixedFrame->data[ch];
        if (!samples) {
            qDebug() << "makeFLTPFrameMute: sample data for channel" << ch << "is null!";
            continue;
        }
        for (int i = 0; i < fixedFrame->nb_samples; ++i) {
            samples[i] = 0.0f;
        }
    }
    return true;
}

void ADecoder::flushDecoder() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (codecCtx_) {
        avcodec_flush_buffers(codecCtx_); // 关键：清空解码器内部缓冲区
    }
    clearFifo(); // 清空音频FIFO
    {
        std::lock_guard<std::mutex> lock(ptsQueueMutex_);
        while (!ptsQueue_.empty()) {
            ptsQueue_.pop(); // 清空PTS队列
        }
    }
    isFirstFrame = true; // 重置第一帧标志
}


void ADecoder::applyVolumeGain_SIMD(AVFrame *frame, float gain)
{
    if (!frame || gain == 1.0f)
        return;

    int nb_channels = frame->ch_layout.nb_channels;
    int nb_samples = frame->nb_samples;
    int aligned_samples = nb_samples & ~3;  // 4 的倍数（128 bit SSE）

    __m128 gain_vec = _mm_set1_ps(gain);  // 构造 [gain, gain, gain, gain]

    for (int ch = 0; ch < nb_channels; ++ch) {
        float* data = (float*)frame->extended_data[ch];

        // 向量化部分
        for (int i = 0; i < aligned_samples; i += 4) {
            __m128 src = _mm_loadu_ps(&data[i]);      // 加载 4 个 float
            __m128 result = _mm_mul_ps(src, gain_vec); // 乘以增益
            _mm_storeu_ps(&data[i], result);           // 存回内存
        }

        // 处理剩余的尾部样本
        for (int i = aligned_samples; i < nb_samples; ++i) {
            data[i] *= gain;
        }
    }
}

void ADecoder::applyVolumeGainAVX2(AVFrame *frame, float linearGain)
{
    if (!frame || linearGain == 1.0f) return;
    if (frame->format != AV_SAMPLE_FMT_FLTP) return; // 只处理 float planar 格式

    int nb_channels = frame->ch_layout.nb_channels;
    int nb_samples = frame->nb_samples;

    __m256 gainVec = _mm256_set1_ps(linearGain); // AVX2 8 float 通道

    for (int ch = 0; ch < nb_channels; ++ch) {
        float* data = (float*)frame->extended_data[ch];

        int i = 0;
        // 每次处理 8 个样本
        for (; i + 7 < nb_samples; i += 8) {
            __m256 src = _mm256_loadu_ps(&data[i]);
            __m256 scaled = _mm256_mul_ps(src, gainVec);
            _mm256_storeu_ps(&data[i], scaled);
        }

        // 处理剩下的不满 8 个的样本
        for (; i < nb_samples; ++i) {
            data[i] *= linearGain;
        }
    }
}

void ADecoder::setVolumeGain(float gain)
{
    m_volumeGain = gain;
}

float ADecoder::getVolumeGain() const
{

    return powf(10.0f, m_volumeGain / 20.0f);
}

int ADecoder::getDuration()
{
    return static_cast<int>(0.5 + stream_->duration * av_q2d(stream_->time_base));
}
float ADecoder::calculateRMSdB(AVFrame *frame)
{
    if (!frame || !frame->data[0])
        return -60.0f;

    int nb_samples = frame->nb_samples;
    int channels = frame->ch_layout.nb_channels;

    if (frame->format != AV_SAMPLE_FMT_FLTP) {
        qWarning() << "Unexpected sample format:" << frame->format;
        return -60.0f;
    }

    double sumSquares = 0.0;
    for (int ch = 0; ch < channels; ++ch) {
        float* chData = reinterpret_cast<float*>(frame->data[ch]);
        if (!chData)
            continue;

        for (int i = 0; i < nb_samples; ++i) {
            float sample = chData[i];
            sumSquares += sample * sample;
        }
    }

    double rms = std::sqrt(sumSquares / (nb_samples * channels));
    if (rms <= 1e-10)
        return -60.0f;

    float dB = 20.0f * std::log10(rms);
    return qBound(-60.0f, dB, 0.0f);
}


float ADecoder::getDb() const
{
    // std::unique_lock<std::mutex> locker(mutex_);

    return m_dB.load();
}

AudioParams *ADecoder::getVideoPars()
{
    return aOutParams_;
}

AVCodecContext *ADecoder::getCodecCtx()
{
    return codecCtx_;
}

AVStream *ADecoder::getStream()
{
    return stream_;
}

void ADecoder::setClock(MediaClock *clock)
{
    syncClock_ = clock;
}

void ADecoder::setRecording(bool record)
{
    if(m_isRecording != record)
    {
        m_isRecording.store(record);
    }
}

void ADecoder::setActive(bool active)
{
    m_isActived.store(active);
}

bool ADecoder::isActived() const
{
    return m_isActived.load();
}
void ADecoder::setSpeedFilter(MediaSpeedFilter *speedFilter)
{
    speedFilter_ = speedFilter;
}

void ADecoder::stop()
{
    m_stop.store(true);
    pauseCond_.notify_all();

    if (audioFifo) {
        av_audio_fifo_reset(audioFifo); // 重置FIFO（清空所有样本）
    }
}

void ADecoder::pause()
{
    std::lock_guard<std::mutex> lock(pauseMutex_);
    m_paused.store(true);
    pauseCond_.notify_all();
}


void ADecoder::resume()
{
    std::lock_guard<std::mutex> lock(pauseMutex_);
    m_paused.store(false);
    pauseCond_.notify_all();
}

void ADecoder::flushQueue()
{
    frameQueue_->clear();
}
void ADecoder::close() {
    decode(nullptr);
    stop();

    std::lock_guard<std::mutex> lock(mutex_);
    if(codecCtx_) {
        avcodec_free_context(&codecCtx_);
        codecCtx_ = nullptr;
    }

    if(aInParams_) {
        delete aInParams_;
        aInParams_ = nullptr;
    }
    if(aOutParams_) {
        delete aOutParams_;
        aOutParams_ = nullptr;
    }

    if(resampler_) {
        delete resampler_;
        resampler_ = nullptr;
    }

    if (audioFifo) {
        av_audio_fifo_free(audioFifo);
        audioFifo = nullptr;
    }

    if (swrOutputCtx_) {
        swr_free(&swrOutputCtx_);
        swrOutputCtx_ = nullptr;
    }

    if(audioMonitor_)
    {
        delete audioMonitor_;
        audioMonitor_ = nullptr;
    }
    while (!ptsQueue_.empty()) {
        ptsQueue_.pop(); // 清空PTS队列
    }
    // 重置状态标志
    isFirstFrame = true;
    m_dB.store(-60.0f);
    m_stop.store(false);
    m_paused.store(false);
}

void ADecoder::clearFifo()
{
    if (audioFifo) {
        av_audio_fifo_reset(audioFifo);
    }
}

void ADecoder::onListenModeChanged(int sourceId, MixerListenMode mode)
{
    if(audioSource_->type() == AudioSourceType::Media)
    {
        MediaAudioSource* mediaSource = dynamic_cast<MediaAudioSource*>(audioSource_);
        if(sourceId != mediaSource->AudioSourceId()) return;
    }else
    {
        if(sourceId != audioSource_->sourceId()) return;
    }

    m_listenMode = mode;

    if (mode == MixerListenMode::CLOSE) {
        if(swrOutputCtx_)
        {
            swr_free(&swrOutputCtx_);
            swrOutputCtx_ = nullptr;
        }
    }
}

void ADecoder::initFifo()
{
    if (audioFifo) {
        av_audio_fifo_free(audioFifo);
    }

    if (!aOutParams_) {
        qDebug() << "initFifo failed: aOutParams_ is nullptr!";
        return;
    }
    qDebug() << "initFifo参数："
             << "format=" << av_get_sample_fmt_name((AVSampleFormat)aOutParams_->format)
             << "nbChannels=" << aOutParams_->nbChannels
             << "sampleRate=" << aOutParams_->sampleRate;
    audioFifo = av_audio_fifo_alloc(aOutParams_->format, aOutParams_->nbChannels, TARGET_SAMPLES);
    if (!audioFifo) {
        qDebug() << "initFifo failed!";
    }
}

void ADecoder::initResampler()
{
    resampler_->init(aInParams_,aOutParams_);
}

void ADecoder::initAudioParams(AVFrame *frame)
{
    if (frame->ch_layout.nb_channels <= 0) {
        qWarning() << "检测到无效声道数" << frame->ch_layout.nb_channels << "，强制设为2";
        frame->ch_layout.nb_channels = 2;
        // 同时修正声道布局（可选，确保兼容性）
        av_channel_layout_default(&frame->ch_layout, 2);
    }
    aInParams_->timeBase = stream_->time_base;
    aInParams_->nbChannels = frame->ch_layout.nb_channels;
    aInParams_->format = codecCtx_->sample_fmt;
    aInParams_->sampleRate = frame->sample_rate;
    aInParams_->sampleSize = av_get_bytes_per_sample(codecCtx_->sample_fmt);

    memcpy(aOutParams_,aInParams_,sizeof(AudioParams));
    aOutParams_->format = AV_SAMPLE_FMT_FLTP;
    aOutParams_->sampleRate = 48000;
    aOutParams_->sampleSize = av_get_bytes_per_sample(aOutParams_->format);
}


AVFrame *ADecoder::convertFLTPToFLT(AVFrame *fixedFrame)
{
    // 校验输入参数
    if (!fixedFrame) {
        qDebug() << "convertFLTPToFLT: 输入帧为空";
        return nullptr;
    }
    if (fixedFrame->format != AV_SAMPLE_FMT_FLTP) {
        qDebug() << "convertFLTPToFLT: 输入帧格式不是 FLTP";
        return nullptr;
    }
    if (fixedFrame->ch_layout.nb_channels <= 0 || fixedFrame->nb_samples <= 0) {
        qDebug() << "convertFLTPToFLT: 无效的声道数或采样数";
        return nullptr;
    }

    // 初始化转换器（仅首次调用时初始化）
    if (!swrOutputCtx_) {
        int ret = swr_alloc_set_opts2(
            &swrOutputCtx_,
            &fixedFrame->ch_layout,  // 目标声道布局
            AV_SAMPLE_FMT_FLT,       // 目标格式（交错浮点）
            fixedFrame->sample_rate, // 目标采样率
            &fixedFrame->ch_layout,  // 源声道布局
            AV_SAMPLE_FMT_FLTP,      // 源格式（平面浮点）
            fixedFrame->sample_rate, // 源采样率
            0, nullptr
            );
        if(ret < 0)
        {
            qDebug() << "swr_alloc_set_opts2 failed!";
            return nullptr;
        }
        if (!swrOutputCtx_ || swr_init(swrOutputCtx_) < 0) {
            qDebug() << "convertFLTPToFLT: 初始化转换器失败";
            swr_free(&swrOutputCtx_);
            return nullptr;
        }
    }

    AVFrame* swrFrame = GlobalPool::getFramePool().get();
    swrFrame->format = AV_SAMPLE_FMT_FLT;
    swrFrame->ch_layout = fixedFrame->ch_layout;
    swrFrame->nb_samples = fixedFrame->nb_samples;
    swrFrame->sample_rate = fixedFrame->sample_rate;

    int ret = av_frame_get_buffer(swrFrame, 0);
    if (ret < 0) {
        qDebug() << "convertFLTPToFLT: 分配输出帧缓冲区失败，错误码:" << ret;
        GlobalPool::getFramePool().recycle(swrFrame);
        return nullptr;
    }

    // 执行格式转换（平面 -> 交错）
    ret = swr_convert(
        swrOutputCtx_,
        swrFrame->data, swrFrame->nb_samples,
        (const uint8_t**)fixedFrame->data, fixedFrame->nb_samples
        );
    if (ret < 0) {
        qDebug() << "convertFLTPToFLT: 格式转换失败，错误码:" << ret;
        GlobalPool::getFramePool().recycle(swrFrame);
        return nullptr;
    }

    return swrFrame;
}




void ADecoder::printError(int ret)
{
    char errorBuffer[AV_ERROR_MAX_STRING_SIZE];
    int res = av_strerror(ret,errorBuffer,sizeof errorBuffer);
    if(res < 0){
        qDebug() << "Unknow Error!";
    }
    else{
        qDebug() <<"Error:"<<errorBuffer;
    }
}

void ADecoder::printFmt()
{
    qDebug() << "audio format : "<< av_get_sample_fmt_name(aInParams_->format);
    qDebug() << "sample_rate : "<< aInParams_->sampleRate;
    qDebug() << "channels : "<< aInParams_->nbChannels;
    qDebug() << "time_base : "<< aInParams_->timeBase.num << " / " << aInParams_->timeBase.den;

    qDebug() << "=================";
    qDebug() << "audio format : "<< av_get_sample_fmt_name(aOutParams_->format);
    qDebug() << "sample_rate : "<< aOutParams_->sampleRate;
    qDebug() << "channels : "<< aOutParams_->nbChannels;
    qDebug() << "time_base : "<< aOutParams_->timeBase.num << " / " << aOutParams_->timeBase.den;
}
