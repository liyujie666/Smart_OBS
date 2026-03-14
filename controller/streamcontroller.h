#ifndef STREAMCONTROLLER_H
#define STREAMCONTROLLER_H
#include <encoder/aencoder.h>
#include <encoder/vencoder.h>
#include <sync/avsyncclock.h>
#include <queue/packetqueue.h>
#include <muxer/muxermanager.h>
#include <thread/threadpool.h>
#include <mixer/audiomixprocessor.h>
#include <statusbar/statusbarmanager.h>
#include <monitor/networkmonitor.h>
#include <controller/dynamicbitratecontroller.h>
#include <QObject>
#include <QWaitCondition>
struct StreamConfig {
    bool enableRecord = false;
    bool enableStream = false;
    QString filePath;
    QString streamUrl;
    videoEncodeConfig vEnConfig;
    AudioEncodeConfig aEnConfig;
};
using FlushCallback = std::function<void()>;
class StreamController : public QObject
{
    Q_OBJECT
public:
    StreamController(ThreadPool* threadPools,CudaRenderWidget* openglWidget,NetworkMonitor* networkMonitor,QObject* parent = nullptr);
    ~StreamController();

    bool start();
    void stop();
    void pause();
    void resume();
    void init(StreamConfig config);
    void flushAudioMixer();
    void stopPushing(); // 新增：停止推流（服务器断开时调用）
    void resumePushing(); // 新增：恢复推流（重连成功时调用）
signals:
    void streamPushingStopped();
    void streamPushingReconnected();
public slots:
    void videoEncodeLoop();
    void onNewFrameAvailable(int64_t framePts);
private:

    void audioEncodeLoop();
    void videoMuxLoop();
    void audioMuxLoop();
    void updateExternalClockLoop();
    void clearFramePtsQueue();
    void flushAudioMixerAsync(FlushCallback callback);
    void updateEncoderBitrate(int targetBitrate);
    void cleanup();


    std::unique_ptr<PacketQueue> vPktQueue_;
    std::unique_ptr<PacketQueue> aPktQueue_;
    std::unique_ptr<MuxerManager> muxerManager_;
    std::unique_ptr<VEncoder> vEncoder_;
    std::unique_ptr<AEncoder> aEncoder_;
    std::unique_ptr<AudioMixProcessor> mixProcessor_;
    std::unique_ptr<FPSCounter> fpsCounter_;
    std::unique_ptr<AVSyncClock> syncClock_;
    std::unique_ptr<DynamicBitrateController> dynamicBitrateCtrl_;

    StreamConfig config_;
    NetworkMonitor* networkMonitor_;
    NetworkMonitor* tcpMonitor_;
    ThreadPool* threadPools_;
    std::thread videoThread_;
    std::thread videoMuxThread_;
    std::thread audioThread_;
    std::thread audioMuxThread_;
    QThread* networkThread_;

    std::atomic<bool> isRunning_{false};
    CudaRenderWidget* openglWidget_;
    std::mutex encodeMutex_;
    std::condition_variable encodeCond_;
    QVector<std::shared_ptr<FrameQueue>> aFrameQueues_;
    int64_t lastAudioFramePts_ = 0;
    std::queue<int64_t> cudaFramePtsQueue_;
    int currentBitrate_ = 0;

    // 暂停相关
    std::atomic<bool> isPaused_{false};
    std::mutex pauseMutex_;

    std::mutex pushMutex_; // 新增：推流状态锁（线程安全）
    bool isPushingStopped_ = false; // 新增：推流是否已停止（避免重复停止）



};

#endif // STREAMCONTROLLER_H
