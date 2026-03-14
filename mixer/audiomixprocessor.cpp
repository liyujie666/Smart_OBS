#include "audiomixprocessor.h"
#include "pool/gloabalpool.h"
#include "sync/offsetmanager.h"
#include "sync/globalclock.h"
#include "QTimer"
AudioMixProcessor::AudioMixProcessor(AVSyncClock* syncClock,QObject *parent) : QObject(parent),syncClock_(syncClock) {
    mixer_ = std::make_unique<AudioMixer>();
    currentActiveSources.insert(0);
    currentActiveSources.insert(1);
}

AudioMixProcessor::~AudioMixProcessor() {
    stop();

}

bool AudioMixProcessor::init(QVector<std::shared_ptr<FrameQueue>> frameQueues)
{
    frameQueues_ = frameQueues;
    frameCaches_.resize(frameQueues_.size());
    // 添加输入源
    for(int i = 0;i < frameQueues.size();i++)
    {
        if(!mixer_->addInput(i))
        {
            emit errorOccurred("Failed to add audio inputs");
            mixer_->release();
            return false;
        }
    }

    // 初始化混音器
    if (!mixer_->init(48000, 2, AV_SAMPLE_FMT_FLTP)) {
        emit errorOccurred("Failed to initialize audio mixer");
        return false;
    }


    return true;
}

AVFrame* AudioMixProcessor::getMixedFrame() {
    if (!mixer_ || frameQueues_.isEmpty()) {
        qDebug() << "AudioMixProcessor not initialized";
        return nullptr;
    }

    // 初始化变量
    inputFrameRanges_.clear();
    bool hasValidFrame = false;
    auto offsetConfig = OffsetManager::getInstance().getConfig();
    int totalQueueSize = 0;

    for (size_t i = 0; i < frameQueues_.size(); ++i) {
        std::shared_ptr<FrameQueue> queue = frameQueues_[i];
        if (!queue || queue ->isClosed()){
            mixer_->addFrame(static_cast<int>(i), nullptr);
            continue;
        }
        // qDebug() << "queue" << i << "size" << queue->size();
        totalQueueSize += queue->size();

        while (queue && frameCaches_[i].size() < CACHE_SIZE && !queue->isEmpty()) {
            AVFrame* frame = queue->pop(); // 从队列取1帧
            if (frame) frameCaches_[i].push_back(frame);
        }
        // bool isActive = currentActiveSources.contains(static_cast<int>(i));
        // while (frameCaches_[i].size() < CACHE_SIZE && !queue->isEmpty()) {
        //     if(isActive){
        //         AVFrame* frame = queue->pop(); // 从队列取1帧
        //         if (frame) frameCaches_[i].push_back(frame);
        //     }else
        //     {
        //         if (silenceFrameCache.contains(static_cast<int>(i))) {
        //             frameCaches_[i].push_back(silenceFrameCache[i]);
        //         }
        //     }

        // }
    }

    // 从本地缓存取帧
    for (size_t i = 0; i < frameQueues_.size(); ++i) {
        FrameTimeRange range;
        range.frame = nullptr;

        // 本地缓存有帧则取1帧
        if (!frameCaches_[i].empty()) {
            range.frame = frameCaches_[i].front();
            frameCaches_[i].pop_front();

            // 获取偏移后的帧
            OffsetManager::getInstance().processAudioOffset(range.frame, i, offsetConfig);

            // 3. 基于偏移后的PTS计算startUs和endUs
            range.startUs = (int64_t)range.frame->opaque; // 偏移后的时间戳（替代原range.startUs = (int64_t)range.frame->opaque）
            range.durationUs = (int64_t)((double)range.frame->nb_samples / range.frame->sample_rate * 1e6);
            range.endUs = range.startUs + range.durationUs;

            hasValidFrame = true;
        }
        inputFrameRanges_.push_back(range);
    }

    // 向混音器添加帧
    for (size_t i = 0; i < inputFrameRanges_.size(); ++i) {

        auto& range = inputFrameRanges_[i];

        if (!range.frame) {
            if (isFlushing_) mixer_->addFrame(static_cast<int>(i), nullptr); // 冲刷时补空帧
            continue;
        }
        range.frame->pts = range.startUs;
        if (!mixer_->addFrame(static_cast<int>(i), range.frame)) {
            qDebug() << "混音器添加混音帧失败！";
        }
        GlobalPool::getFramePool().recycle(range.frame);
        // 临时PTS供混音器
    }

    // 获取混音后帧
    AVFrame* mixedFrame = mixer_->getMixedFrame();
    if (!mixedFrame) {

        if (isFlushing_) {
            qDebug() << "Audio mix flushing completed";
            isFlushing_ = false;
        }
        return nullptr;
    }

    // 计算帧时长
    int64_t frameDurationUs = (int64_t)((double)mixedFrame->nb_samples / mixedFrame->sample_rate * 1e6);
    if (frameDurationUs <= 0) {
        frameDurationUs = (1024 * 1000000LL) / 48000; // 默认21333微秒
        qWarning() << "Invalid frame duration, using default:" << frameDurationUs;
    }


    // 速率控制
    int64_t currentUs = GlobalClock::getInstance().getCurrentUs();

    if (nextOutputUs == 0) {

        nextOutputUs = currentUs;
    } else {
        // 计算理论上的下一次输出时间
        nextOutputUs += frameDurationUs;

        // 检查是否需要调整等待时间
        bool isHighLoad = (totalQueueSize > HIGH_LOAD_THRESHOLD);
        int64_t sleepUs = nextOutputUs - currentUs;

        if (currentUs > nextOutputUs) {
            nextOutputUs = currentUs;
        } else {
            // 根据负载堆积情况调整等待时间
            if (isHighLoad && sleepUs > 0) {
                sleepUs = (int64_t)(sleepUs * REDUCE_WAIT_RATIO);
                // qDebug() << "队列堆积（" << totalQueueSize << "帧），减少等待至" << sleepUs << "微秒";
            }

            if (sleepUs > 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(sleepUs));
                currentUs = GlobalClock::getInstance().getCurrentUs();
            }

            nextOutputUs = currentUs + (frameDurationUs - sleepUs);
        }
    }

    int64_t audioPts = 0;
    if (isFlushing_) {
        audioPts = syncClock_->getAPts();
    } else {

        if (hasValidFrame) {
            int64_t maxStartUs = 0;
            // for (const auto& range : inputFrameRanges_) {
            //     if (range.frame) maxStartUs = std::max(maxStartUs, range.startUs);
            // }
            maxStartUs = GlobalClock::getInstance().getCurrentUs();
            int64_t elapsedUs = maxStartUs - syncClock_->getStartUs();
            syncClock_->calibrateAudio(elapsedUs); // 校准
        }
        audioPts = syncClock_->getAPts();
    }

    // 7. 设置最终PTS（确保非负）
    mixedFrame->pts = audioPts;
    mixedFrame->opaque = (void*)audioPts;

    // 简化日志：仅打印异常间隔
    // static int64_t lastDebugPts = 0;
    // if (lastDebugPts != 0) {
    //     int64_t actualInterval = mixedFrame->pts - lastDebugPts;
    //     if (abs(actualInterval - frameDurationUs) > 5000) { // 偏差超5ms报警
    //         qWarning() << "[PTS间隔异常] 实际:" << actualInterval << "目标:" << frameDurationUs;
    //     }
    // }
    // lastDebugPts = mixedFrame->pts;
    return mixedFrame;
}



void AudioMixProcessor::stop() {
    isRunning_ = false;

    for(auto& queue : frameQueues_)
    {
        queue->clear();
    }

    for (auto& cache : frameCaches_) {
        for (AVFrame* frame : cache) {
            if (frame) GlobalPool::getFramePool().recycle(frame);
        }
        cache.clear();
    }
    frameCaches_.clear();
    // 释放混音器资源
    mixer_->release();
    qDebug() << "Audio mix processor stopped";
}

void AudioMixProcessor::pauseCleanup() {

    for (size_t i = 0; i < frameQueues_.size(); ++i) {
        auto queue = frameQueues_[i];
        if (!queue || queue->isClosed()) continue;

        // 仅保留最新1帧，其余清空
        while (queue->size() > 1) {
            AVFrame* oldFrame = queue->pop();
            GlobalPool::getFramePool().recycle(oldFrame); // 回收旧帧
        }

        // 清理缓存（保留最新1帧）
        while (frameCaches_[i].size() > 1) {
            AVFrame* oldCache = frameCaches_[i].front();
            frameCaches_[i].pop_front();
            GlobalPool::getFramePool().recycle(oldCache);
        }
    }
}

std::vector<AVFrame*> AudioMixProcessor::sliceFrame(AVFrame* originalFrame, int sliceSize) {
    std::vector<AVFrame*> slices;
    if (!originalFrame || sliceSize <= 0 || originalFrame->nb_samples <= 0) {
        qDebug() << "无效的帧或切片大小，无法切片";
        return slices;
    }

    int totalSamples = originalFrame->nb_samples;
    int offset = 0; // 当前切片在原帧中的起始样本位置
    int bytesPerSample = av_get_bytes_per_sample((AVSampleFormat)originalFrame->format);
    if (bytesPerSample <= 0) {
        qDebug() << "无法获取样本字节数，切片失败";
        return slices;
    }

    try {
        while (offset < totalSamples) {
            // 计算当前切片的样本数（最后一帧可能不足1024）
            int currentSliceSamples = std::min(sliceSize, totalSamples - offset);

            // 从帧池获取切片帧
            AVFrame* slice = GlobalPool::getFramePool().get();
            if (!slice) {
                qDebug() << "帧池获取切片帧失败";
                throw std::runtime_error("Get slice frame failed");
                break;
            }

            // 复制原帧的基础属性
            slice->format = originalFrame->format;
            slice->sample_rate = originalFrame->sample_rate;
            av_channel_layout_copy(&slice->ch_layout, &originalFrame->ch_layout);
            slice->nb_samples = currentSliceSamples;
            slice->pts = originalFrame->pts + offset; // 切片的pts基于原帧pts偏移


            // 为切片帧分配缓冲区
            if (av_frame_get_buffer(slice, 0) < 0) {
                qDebug() << "切片帧缓冲区分配失败";
                GlobalPool::getFramePool().recycle(slice);
                break;
            }

            // 复制原帧数据到切片（按通道处理）
            for (int ch = 0; ch < slice->ch_layout.nb_channels; ++ch) {
                const uint8_t* src = originalFrame->data[ch] + offset * bytesPerSample;
                uint8_t* dst = slice->data[ch];
                memcpy(dst, src, currentSliceSamples * bytesPerSample);
            }

            slices.push_back(slice);
            offset += currentSliceSamples;
        }
    } catch (...) {
        for (AVFrame* slice : slices) {
            if (slice) GlobalPool::getFramePool().recycle(slice);
        }
        slices.clear(); // 清空向量，避免返回泄漏的帧
    }

    return slices;
}

AVFrame *AudioMixProcessor::createSilenceFrame(int sampleRate, int channels, AVSampleFormat format)
{
    AVFrame* frame = GlobalPool::getFramePool().get();

    frame->nb_samples = 1024;
    frame->sample_rate = sampleRate;
    frame->format = format;
    frame->ch_layout.nb_channels = channels;
    av_channel_layout_default(&frame->ch_layout, channels);

    if (av_frame_get_buffer(frame, 0) < 0) {
        qDebug() << "deque_: failed to allocate frame buffer";
        GlobalPool::getFramePool().recycle(frame);
        return nullptr;
    }

    if (av_samples_set_silence(frame->extended_data, 0, frame->nb_samples, channels, format) < 0) {
        qDebug() << "createSilenceFrame: failed to set silence";
        GlobalPool::getFramePool().recycle(frame);
        return nullptr;
    }

    return frame;
}


AVFrame* AudioMixProcessor::alignFrameSamples(AVFrame* frame, int targetSamples) {
    if (!frame) return nullptr;
    if (frame->nb_samples == targetSamples) return frame;

    AVFrame* alignedFrame = GlobalPool::getFramePool().get();
    alignedFrame->format = frame->format;
    alignedFrame->sample_rate = frame->sample_rate;
    av_channel_layout_copy(&alignedFrame->ch_layout, &frame->ch_layout);
    alignedFrame->nb_samples = targetSamples;
    alignedFrame->pts = frame->pts;

    // 分配缓冲区（注意：确保与样本格式匹配的对齐要求）
    if (av_frame_get_buffer(alignedFrame, 0) < 0) {
        qDebug() << "校准帧分配缓冲区失败";
        GlobalPool::getFramePool().recycle(alignedFrame);
        return nullptr;
    }

    int copySamples = std::min(frame->nb_samples, targetSamples);
    int bytesPerSample = av_get_bytes_per_sample((AVSampleFormat)frame->format);
    if (bytesPerSample <= 0) {
        qDebug() << "无效的样本格式，无法计算字节数";
        GlobalPool::getFramePool().recycle(alignedFrame);
        GlobalPool::getFramePool().recycle(frame);
        return nullptr;
    }

    for (int ch = 0; ch < frame->ch_layout.nb_channels; ++ch) {
        // 1. 复制有效样本数据
        memcpy(
            alignedFrame->data[ch],
            frame->data[ch],
            copySamples * bytesPerSample
            );

        // 2. 不足部分补静音（关键修正：参数类型和起始索引）
        if (copySamples < targetSamples) {
            int silenceSamples = targetSamples - copySamples;

            // 调用av_samples_set_silence填充静音
            // 第一个参数：指向当前通道数据指针的指针（uint8_t**）
            // 第二个参数：从第几个样本开始填充（这里是copySamples之后）
            int ret = av_samples_set_silence(
                &alignedFrame->data[ch],  // 修正：传递指针的指针
                copySamples,              // 修正：起始样本索引（不是0）
                silenceSamples,
                1,
                (AVSampleFormat)frame->format
                );

            if (ret < 0) {
                qDebug() << "通道" << ch << "填充静音失败，错误码：" << ret;
                GlobalPool::getFramePool().recycle(alignedFrame);
                GlobalPool::getFramePool().recycle(frame);
                return nullptr;
            }
        }
    }

    GlobalPool::getFramePool().recycle(frame);
    return alignedFrame;
}

void AudioMixProcessor::setFlushing(bool flushing) {
    isFlushing_ = flushing;
}

QVector<std::shared_ptr<FrameQueue>> AudioMixProcessor::frameQueues()
{
    return frameQueues_;
}


void AudioMixProcessor::setLastMixedPtsUs(int64_t pts) {
    lastMixedPtsUs = pts;
    qDebug() << "冲刷起始PTS已对齐到:" << lastMixedPtsUs << "微秒";
}

void AudioMixProcessor::setCurrentActiveSource(std::set<int> activeSources)
{
    currentActiveSources.insert(activeSources.begin(),activeSources.end());
    for(int index : currentActiveSources)
    {
        if(!silenceFrameCache.contains(index))
        {
            AVFrame* silenceFrame = createSilenceFrame();
            silenceFrameCache[index] = silenceFrame;
        }
    }
}
