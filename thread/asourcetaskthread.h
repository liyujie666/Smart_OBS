#ifndef ASOURCETASKTHREAD_H
#define ASOURCETASKTHREAD_H
#include "queue/framequeue.h"
#include "queue/packetqueue.h"
#include "demux/demuxer.h"
#include "decoder/adecoder.h"
#include "infotools.h"
#include <QObject>
#include <QThread>
extern"C"
{
#include "libavformat/avformat.h"
}

class AudioSource;
class AVSyncClock;
class ASourceTaskThread :public QObject
{
    Q_OBJECT
public:
    ASourceTaskThread(AudioSource* source,QObject* parent=nullptr);
    ~ASourceTaskThread();

    void start();
    void stop();

    bool isRunning() const;
    std::shared_ptr<FrameQueue> frameQueue() const;
    int audioSourceId() const;
    float calculateRMSdB(AVFrame* frame);
    float smoothDb(float currentDb);
    void setVolumeGain(int gain);
    void startAddingFrame();
    void stopAddingFrame();
signals:
    void frameReady(AVFrame* frame);
    void volumeLevelChanged(float dB);

private:
    void runDemux();
    void runDecode();

private:
    AudioSource* audioSource_;

    std::unique_ptr<Demuxer> aDemuxer_;
    std::unique_ptr<ADecoder> aDecoder_;
    std::shared_ptr<PacketQueue> aPktQueue_;
    std::shared_ptr<FrameQueue> aFrameQueue_;
    std::unique_ptr<QTimer> volumeUpdateTimer_;
    // Demuxer* aDemuxer_;
    // ADecoder* aDecoder_;
    // PacketQueue* aPktQueue_;
    // FrameQueue* aFrameQueue_;

    std::atomic<bool> isRunning_;
    std::thread aDemuxThread_;
    std::thread aDecodeThread_;

    float lastCalculatedDb;
    float prevDb_ = -60.f;
};

#endif // ASOURCETASKTHREAD_H
