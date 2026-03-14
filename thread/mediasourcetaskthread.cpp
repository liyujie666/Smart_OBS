#include "mediasourcetaskthread.h"
#include "source/audio/mediaaudiosource.h"
#include "source/video/localvideosource.h"
#include "sync/mediaclock.h"

MediaSourceTaskThread::MediaSourceTaskThread(LocalVideoSource *videoSource, MediaAudioSource *audioSource, QObject *parent)
    : QObject(parent),videoSource_(videoSource),audioSource_(audioSource)
{
    demuxer_ = std::make_unique<Demuxer>();

    if(videoSource)
    {
        vDecoder_ = std::make_unique<VDecoder>(videoSource_);
        vPktQueue_ = std::make_unique<PacketQueue>();
        vFrameQueue_  = std::make_shared<FrameQueue>();
        currentSceneId_ = videoSource_->sceneId();
    }
    if(audioSource)
    {
        aDecoder_ = std::make_unique<ADecoder>(audioSource_);
        aPktQueue_ = std::make_unique<PacketQueue>();
        aFrameQueue_ = std::make_shared<FrameQueue>();
        volumeUpdateTimer_ = std::make_unique<QTimer>(this);
        connect(volumeUpdateTimer_.get(), &QTimer::timeout, this, [&]() {
            float dB = aDecoder_->getDb();
            float smoothed = smoothDb(dB);
            emit volumeLevelChanged(smoothed);
        });
        volumeUpdateTimer_->start(30);
        currentSceneId_ = videoSource_->sceneId();
    }
}

MediaSourceTaskThread::~MediaSourceTaskThread()
{
    stop();

    qDebug() << "~MediaSourceTaskThread";
}

void MediaSourceTaskThread::init()
{
    demuxer_->init(aPktQueue_.get(), vPktQueue_.get());
    // 初始化解码器
    AVStream* aStream = nullptr,*vStream = nullptr;
    if(videoSource_)
        vStream = demuxer_->getVStream();
    if(audioSource_)
        aStream = demuxer_->getAStream();

    if(!aStream && !vStream)
    {
        qDebug() << "流未初始化！";
        return;
    }

    // bool res = initSpeedFilter();
    // if(!res)
    // {
    //     qDebug() << "倍速滤镜初始化失败！";
    //     return;
    // }

    if(videoSource_)
    {
        vDecoder_->init(vStream, vFrameQueue_.get());
        // vDecoder_->setSpeedFilter(speedFilter_);
        qRegisterMetaType<CudaFrameInfo>("CudaFrameInfo");
        connect(vDecoder_.get(), &VDecoder::frameReady, this, [=](const CudaFrameInfo& info) {
            // if (currentSceneId_ == videoSource_->sceneId()) {
            //     openglWidget_->addOrUpdateLayer(info);
            // }
            openglWidget_->addOrUpdateLayer(info);
        }, Qt::QueuedConnection);
    }
    if(audioSource_)
    {
        aDecoder_->init(aStream, aFrameQueue_.get());
        // aDecoder_->setSpeedFilter(speedFilter_);
    }

}

void MediaSourceTaskThread::start()
{
    if(isRunning_.load()) return;

    syncClock_->resetMedia();
    isRunning_.store(true);

    // 开启线程
    demuxThread_ = std::thread(&MediaSourceTaskThread::runDemux,this);
    if(videoSource_)
        vDecodeThread_ = std::thread(&MediaSourceTaskThread::runVDecode,this);
    if(audioSource_)
        aDecodeThread_ = std::thread(&MediaSourceTaskThread::runADecode,this);

}

void MediaSourceTaskThread::stop() {
    if (!isRunning_.load()) return;
    isRunning_.store(false);

    // 停止音量定时器
    if (volumeUpdateTimer_) {
        volumeUpdateTimer_->stop();
        volumeUpdateTimer_->disconnect();
    }

    // 中断并清空队列
    if (aFrameQueue_) {
        aFrameQueue_->clear();
        aFrameQueue_->interrupt();
        aFrameQueue_->close();
    }
    if (vFrameQueue_) {
        vFrameQueue_->clear();
        vFrameQueue_->interrupt();
        vFrameQueue_->close();
    }
    if (aPktQueue_) {
        aPktQueue_->clear();
        aPktQueue_->interrupt();
        aPktQueue_->close();
    }
    if (vPktQueue_) {
        vPktQueue_->clear();
        vPktQueue_->interrupt();
        vPktQueue_->close();
    }

    // 停止解码器
    if (aDecoder_) aDecoder_->stop();
    if (vDecoder_) vDecoder_->stop();
    demuxer_->stop();

    // 等待子线程终止
    if (aDecodeThread_.joinable()) {
        aDecodeThread_.join();
    }
    if (vDecodeThread_.joinable()) {
        vDecodeThread_.join();
    }
    if (demuxThread_.joinable()) {
        demuxThread_.join();
    }


    // 清空全局池
    GlobalPool::getFramePool().clear();
    GlobalPool::getPacketPool().clear();

    // 移除图层（仅视频存在时）
    if (videoSource_ && openglWidget_) {
        openglWidget_->removeLayerBySourceId(videoSource_->sourceId());
    }

    isDemuxFinished_.store(false);
    qDebug() << "MediaSourceTaskThread stopped!";
}


void MediaSourceTaskThread::resume()
{
    demuxer_->resume();
    if (aDecoder_) aDecoder_->resume();
    if (vDecoder_) vDecoder_->resume();
    if (syncClock_) {
        syncClock_->resume();
    }
}

void MediaSourceTaskThread::pause()
{
    demuxer_->pause();
    if (aDecoder_) aDecoder_->pause();
    if (vDecoder_) vDecoder_->pause();
    if (syncClock_) {
        syncClock_->pause();
    }
}

void MediaSourceTaskThread::flush()
{
    GlobalPool::getFramePool().clear();
    GlobalPool::getPacketPool().clear();

    if (aDecoder_) {
        aDecoder_->flushQueue();
        aDecoder_->flushDecoder();
    }
    if (vDecoder_) {
        vDecoder_->flushQueue();
        vDecoder_->flushDecoder();
    }

    qDebug() << "[MediaSourceTaskThread] flush() called at" << GlobalClock::getInstance().getCurrentUs();
}
bool MediaSourceTaskThread::seek(int64_t seekUs) {
    demuxer_->pause();

    if (!seekSync_) {
        seekSync_ = std::make_shared<SeekSync>();
        if (aDecoder_) aDecoder_->setSeekSync(seekSync_);
        if (vDecoder_) vDecoder_->setSeekSync(seekSync_);
    } else {
        seekSync_->seeking.store(true, std::memory_order_release);
        seekSync_->aReady.store(false, std::memory_order_release);
        seekSync_->vReady.store(false, std::memory_order_release);
        seekSync_->targetUs.store(seekUs, std::memory_order_release);
        seekSync_->serial.fetch_add(1, std::memory_order_acq_rel);
    }

    if (syncClock_) {
        syncClock_->pause();
        syncClock_->resetVideoFirstFrame();
    }
    demuxer_->seek(seekUs);
    flush();
    if (syncClock_) {
        syncClock_->seekTo(seekUs);
        syncClock_->resume();
    }
    demuxer_->resume();

    return true;
}

bool MediaSourceTaskThread::initSpeedFilter()
{
    bool hasVideo = false;
    bool hasAudio = false;
    if (videoSource_) {
        videoParams_ = videoSource_->getVideoFilterParams();
        hasVideo = true;
    }

    // 获取音频参数（从音频源中提取）
    if (audioSource_) {
        audioParams_ = audioSource_->getAudioFilterParams();
        hasAudio = true;
    }

    // 初始化滤镜
    speedFilter_ = std::make_unique<MediaSpeedFilter>();
    return speedFilter_->initialize(hasVideo, hasAudio, videoParams_, audioParams_,MediaSpeedFilter::Speed::S2_0);

}

void MediaSourceTaskThread::setSpeed(MediaSpeedFilter::Speed speed) {
    if (speedFilter_) {
        speedFilter_->changeSpeed(speed);

        // double speedValue = 1.0;
        // switch (speed) {
        // case MediaSpeedFilter::Speed::S0_5: speedValue = 0.5; break;
        // case MediaSpeedFilter::Speed::S1_5: speedValue = 1.5; break;
        // case MediaSpeedFilter::Speed::S2_0: speedValue = 2.0; break;
        // default: speedValue = 1.0;
        // }
        // if (syncClock_) {
        //     syncClock_->setSpeed(speedValue);
        // }
    }
}
void MediaSourceTaskThread::setClock(MediaClock *clock)
{
    syncClock_ = clock;
    if (aDecoder_) aDecoder_->setClock(clock); // 仅音频解码器存在时
    if (vDecoder_) vDecoder_->setClock(clock); // 仅视频解码器存在时
}


void MediaSourceTaskThread::setDemuxerFmtCtx(AVFormatContext* fmtCtx) {
    demuxer_->setFmtCtx(fmtCtx);
}
void MediaSourceTaskThread::setOpenGlWidget(CudaRenderWidget *openglWidget)
{
    openglWidget_ = openglWidget;
}

void MediaSourceTaskThread::setVolumeGain(int gain)
{
    if (aDecoder_) aDecoder_->setVolumeGain(gain);
}

void MediaSourceTaskThread::setRecording(bool record)
{
    if (aDecoder_) aDecoder_->setRecording(record);
}

float MediaSourceTaskThread::smoothDb(float currentDb)
{
    const float alpha = 0.1f; // 平滑系数
    prevDb_ = alpha * currentDb + (1 - alpha) * prevDb_;
    return prevDb_;
}

int MediaSourceTaskThread::getMediaThreadSourceId() const
{
    if (videoSource_) {
        return videoSource_->sourceId();
    } else if (audioSource_) {
        return audioSource_->sourceId();
    }
    return -1;
}
std::shared_ptr<FrameQueue> MediaSourceTaskThread::aFrameQueue() const
{
    return aFrameQueue_;
}

bool MediaSourceTaskThread::isRunning() const
{
    return isRunning_.load();
}

void MediaSourceTaskThread::onSceneChanged(int sceneId)
{
    currentSceneId_ = sceneId;
    // qDebug() << "currentSceneId_" << currentSceneId_;
}

void MediaSourceTaskThread::runDemux()
{
    while (isRunning_.load()) {
        int ret = demuxer_->demux();
        if (ret == 1) {
            qDebug() << "文件读取完毕，等待解码线程处理剩余包...";
            isDemuxFinished_.store(true);
            std::unique_lock<std::mutex> lock(queueCheckMutex_);
            queueCheckCond_.wait(lock, [this]() {
                bool audioEmpty = !aPktQueue_ || aPktQueue_->isEmpty();
                bool videoEmpty = !vPktQueue_ || vPktQueue_->isEmpty();
                return (audioEmpty && videoEmpty) || !isRunning_.load();
            });

            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            if (isRunning_.load()) {
                qDebug() << "所有包处理完毕，发送播放结束信号";
                int sourceId = getMediaThreadSourceId();
                if (sourceId != -1) {
                    emit playFinished(sourceId);
                }
            }
            break;
        } else if (ret < 0) {
            qDebug() << "Demux错误，停止读取";
            isRunning_.store(false);
            break;
        }
    }
}

void MediaSourceTaskThread::runADecode()
{

    while(isRunning_.load())
    {
        AVPacket* pkt = aPktQueue_->pop();
        if(!pkt) continue;

        aDecoder_->decode(pkt);
        GlobalPool::getPacketPool().recycle(pkt);

        if (isDemuxFinished_.load()) {
            queueCheckCond_.notify_one();
        }
    }
}

void MediaSourceTaskThread::runVDecode()
{

    while(isRunning_.load())
    {
        AVPacket* pkt = vPktQueue_->pop();
        if(!pkt) continue;

        vDecoder_->decode(pkt);
        GlobalPool::getPacketPool().recycle(pkt);

        if (isDemuxFinished_.load()) {
            queueCheckCond_.notify_one();
        }
    }
}
