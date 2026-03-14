#include "asourcetaskthread.h"
#include "source/audio/audiosource.h"
#include "QTimer"

ASourceTaskThread::ASourceTaskThread(AudioSource *source,QObject* parent)
    : QObject(parent),audioSource_(source),isRunning_(false)
{

    aDemuxer_ = std::make_unique<Demuxer>(audioSource_->getFormatContext());
    aDecoder_ = std::make_unique<ADecoder>(audioSource_);
    aPktQueue_ = std::make_shared<PacketQueue>();
    aFrameQueue_ = std::make_shared<FrameQueue>();

    volumeUpdateTimer_ = std::make_unique<QTimer>(this);
    connect(volumeUpdateTimer_.get(), &QTimer::timeout, this, [&]() {
        float dB = aDecoder_->getDb(); // 后台线程更新这个值
        float smoothed = smoothDb(dB);
        emit volumeLevelChanged(smoothed);
    });
    volumeUpdateTimer_->start(30);

}

ASourceTaskThread::~ASourceTaskThread()
{
    stop();

}

void ASourceTaskThread::start()
{
    aDemuxer_->init(aPktQueue_.get(),nullptr);
    AVStream* stream = aDemuxer_->getAStream();
    aDecoder_->init(stream,aFrameQueue_.get());
    isRunning_.store(true);

    aDemuxThread_ = std::thread(&ASourceTaskThread::runDemux,this);
    aDecodeThread_ = std::thread(&ASourceTaskThread::runDecode,this);

}

void ASourceTaskThread::stop()
{
    if(!isRunning_.load()) return;
    isRunning_.store(false);

    if(aDemuxThread_.joinable()){
        aDemuxThread_.join();
        qDebug() << "Demux thread exited";
    }
    if(aDecodeThread_.joinable()) {
        aDecodeThread_.join();
        qDebug() << "Decode thread exited";
    }

    aFrameQueue_->clear();
    aPktQueue_->clear();
    aDecoder_->close();
    aDemuxer_->close();
    qDebug() << "ASourceTaskThread stopped";
}


bool ASourceTaskThread::isRunning() const
{
    return isRunning_.load();
}

std::shared_ptr<FrameQueue> ASourceTaskThread::frameQueue() const
{
    return aFrameQueue_;
}

int ASourceTaskThread::audioSourceId() const
{
    if(!audioSource_) return -1;
    return audioSource_->sourceId();
}

float ASourceTaskThread::calculateRMSdB(AVFrame *frame)
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

void ASourceTaskThread::runDemux()
{
    while(isRunning_)
    {
        int ret = aDemuxer_->demux();
        if(ret < 0)
        {
            break;
        }
    }
}

void ASourceTaskThread::runDecode()
{
    while(isRunning_)
    {
        AVPacket* pkt = aPktQueue_->pop();
        if(!pkt) continue;

        aDecoder_->decode(pkt);
        GlobalPool::getPacketPool().recycle(pkt);
    }
}

float ASourceTaskThread::smoothDb(float currentDb)
{
    const float alpha = 0.1f; // 平滑系数
    prevDb_ = alpha * currentDb + (1 - alpha) * prevDb_;
    return prevDb_;
}

void ASourceTaskThread::setVolumeGain(int gain)
{
    aDecoder_->setVolumeGain(gain);
}

void ASourceTaskThread::startAddingFrame()
{
    aDecoder_->setRecording(true);
}

void ASourceTaskThread::stopAddingFrame()
{
    aDecoder_->setRecording(false);
}
