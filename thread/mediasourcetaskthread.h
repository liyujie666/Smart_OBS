#ifndef MEDIASOURCETASKTHREAD_H
#define MEDIASOURCETASKTHREAD_H
#include "demux/demuxer.h"
#include "decoder/adecoder.h"
#include "decoder/vdecoder.h"
#include "queue/packetqueue.h"
#include "ASourceTaskThread.h"  // 复用音频解码相关逻辑
#include "VSourceTaskThread.h"  // 复用视频解码相关逻辑
#include "filter/mediaspeedfilter.h"
#include <QObject>
#include <QTimer>
class MediaClock;
struct SeekSync;
class LocalVideoSource;
class MediaAudioSource;

class MediaSourceTaskThread : public QObject
{
    Q_OBJECT
public:
    explicit MediaSourceTaskThread(LocalVideoSource* videoSource,MediaAudioSource* audioSource,QObject *parent = nullptr);
    ~MediaSourceTaskThread();

    void init();
    void start();
    void stop();
    void resume();
    void pause();
    void flush();
    bool seek(int64_t seekUs);

    bool initSpeedFilter();
    void setSpeed(MediaSpeedFilter::Speed speed);
    void setClock(MediaClock* clock);
    void setOpenGlWidget(CudaRenderWidget* openglWidget);
    void setDemuxerFmtCtx(AVFormatContext* fmtCtx);
    void setVolumeGain(int gain);
    void setRecording(bool record);
    float smoothDb(float currentDb);
    int getMediaThreadSourceId() const;
    std::shared_ptr<FrameQueue> aFrameQueue() const;
    double getDuration();
    bool isRunning() const;

public slots:
    void onSceneChanged(int sceneId);

signals:
    void volumeLevelChanged(float dB);
    void playFinished(int sourceId);
private:
    void runDemux();
    void runADecode();
    void runVDecode();


private:

    LocalVideoSource* videoSource_;
    MediaAudioSource* audioSource_;

    std::unique_ptr<Demuxer> demuxer_;
    std::unique_ptr<VDecoder> vDecoder_;
    std::unique_ptr<ADecoder> aDecoder_;
    std::unique_ptr<PacketQueue> aPktQueue_;
    std::unique_ptr<PacketQueue> vPktQueue_;
    std::shared_ptr<FrameQueue> aFrameQueue_;
    std::shared_ptr<FrameQueue> vFrameQueue_;
    std::unique_ptr<MediaSpeedFilter> speedFilter_;

    MediaClock* syncClock_ = nullptr;
    std::shared_ptr<SeekSync> seekSync_ = nullptr;
    CudaRenderWidget* openglWidget_ = nullptr;

    std::thread demuxThread_;
    std::thread vDecodeThread_;
    std::thread aDecodeThread_;
    std::atomic<bool> isRunning_ = false;
    int currentSceneId_ = 1;

    ASourceTaskThread* aTaskHelper_ = nullptr;
    VSourceTaskThread* vTaskHelper_ = nullptr;

    std::unique_ptr<QTimer> volumeUpdateTimer_;
    float prevDb_ = -60.f;

    std::atomic<bool> isDemuxFinished_{false};
    std::mutex queueCheckMutex_;
    std::condition_variable queueCheckCond_;

    std::tuple<int, int, AVPixelFormat, AVRational> videoParams_;
    std::tuple<int, AVSampleFormat, AVChannelLayout, AVRational> audioParams_;


};

#endif // MEDIASOURCETASKTHREAD_H
