#ifndef AUDIOMIXPROCESSOR_H
#define AUDIOMIXPROCESSOR_H
#include "queue/framequeue.h"
#include "mixer/audiomixer.h"
#include "sync/avsyncclock.h"
#include <QObject>
#include <QVector>

struct FrameTimeRange {
    int64_t startUs = 0;   // 帧的全局开始时间（opaque）
    int64_t endUs = 0;     // 帧的全局结束时间（start + duration）
    int64_t durationUs = 0;
    AVFrame* frame = nullptr; // 原始帧
};
class AudioMixProcessor : public QObject {
    Q_OBJECT
public:
    explicit AudioMixProcessor(AVSyncClock* syncClock,QObject *parent = nullptr);
    ~AudioMixProcessor();

    bool init(QVector<std::shared_ptr<FrameQueue>> frameQueues);
    void stop();
    void pauseCleanup();
    AVFrame* getMixedFrame();
    std::vector<AVFrame*> sliceFrame(AVFrame* originalFrame, int sliceSize);
    AVFrame* createSilenceFrame(int sampleRate = 48000, int channels = 2, AVSampleFormat format = AV_SAMPLE_FMT_FLTP);
    AVFrame* alignFrameSamples(AVFrame* frame, int targetSamples);
    void setFlushing(bool flushing);
    QVector<std::shared_ptr<FrameQueue>> frameQueues();
    void setLastMixedPtsUs(int64_t pts);
    void setCurrentActiveSource(std::set<int> activeSources);

signals:
    void errorOccurred(const QString& error);

private:
    QVector<std::shared_ptr<FrameQueue>> frameQueues_;
    std::unique_ptr<AudioMixer> mixer_;
    AVSyncClock* syncClock_;

    bool isRunning_ = false;
    std::vector<int> emptyQueueCount_;
    std::atomic<bool> isFlushing_{false};

    std::vector<FrameTimeRange> inputFrameRanges_;
    std::unordered_map<int, AVFrame*> silenceFrameCache;
    std::set<int> currentActiveSources;


    int64_t lastOutputUs = 0; // 速率控制用
    int64_t lastMixedPtsUs = 0; // 上一帧PTS
    int64_t nextOutputUs = 0; // 静态变量，记录下一次应输出的时间
    int64_t targetNextPts_ = 0;

    std::vector<std::deque<AVFrame*>> frameCaches_;
    const int CACHE_SIZE = 5; // 每个缓存最多存5帧（可调整）
    const int HIGH_LOAD_THRESHOLD = 50;
    const float REDUCE_WAIT_RATIO = 0.5f;
};


#endif // AUDIOMIXPROCESSOR_H

