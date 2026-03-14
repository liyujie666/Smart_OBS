#ifndef ADECODER_H
#define ADECODER_H
#include "mutex"
#include "resample/aresampler.h"
#include "component/audiomixerdialog.h"
#include "sync/seeksync.h"
extern "C" {
#include<libavformat/avformat.h>
#include<libavutil/samplefmt.h>
#include<libavcodec/avcodec.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}
#include <QObject>
#include <queue>
class AudioSource;
class FrameQueue;
class MediaClock;
class MediaSpeedFilter;
class AudioMonitor;

struct AudioParams{
    int sampleRate;
    int nbChannels;
    AVRational timeBase;
    enum AVSampleFormat format;
    int sampleSize;
};

class ADecoder : public QObject
{
    Q_OBJECT
public:
    explicit ADecoder(AudioSource* audioSource_);
    ADecoder(const ADecoder&) = delete;
    ADecoder& operator=(const ADecoder&) = delete;
    ~ADecoder();

    void init(AVStream *stream,FrameQueue* frameQueue);
    void decode(AVPacket* packet);
    void flushDecoder();

    void applyVolumeGain_SIMD(AVFrame* frame, float gain);
    void applyVolumeGainAVX2(AVFrame* frame, float linearGain);
    void setVolumeGain(float gain);
    float getVolumeGain() const;
    float calculateRMSdB(AVFrame *frame);
    void extractFixedSizeFrames();
    bool makeFLTPFrameMute(AVFrame* fixedFrame);

    int getDuration();
    float getDb() const;
    AudioParams* getVideoPars();
    AVCodecContext *getCodecCtx();
    AVStream *getStream();
    void setClock(MediaClock* clock);
    void setSeekSync(const std::shared_ptr<SeekSync>& s) { seekSync_ = s; }
    void setRecording(bool record);
    void setActive(bool active);
    void setSpeedFilter(MediaSpeedFilter* speedFilter);
    bool isActived() const;
    void stop();
    void pause();
    void resume();
    void flushQueue();
    void close();
    void clearFifo();
public slots:
    void onListenModeChanged(int sourceId,MixerListenMode mode);
signals:
    void frameReady();
    void frameOutReady(const QByteArray& pcmData);
    // void frameOutReady(AVFrame* frame);

private:
    void initFifo();
    void initResampler();
    void initAudioParams(AVFrame* frame);
    AVFrame* convertFLTPToFLT(AVFrame* frame);
    void printError(int ret);
    void printFmt();

    AVCodecContext* codecCtx_ = nullptr;
    AVStream* stream_ = nullptr;
    AResampler* resampler_ = nullptr;
    FrameQueue* frameQueue_ = nullptr;
    AudioParams* aInParams_ = nullptr;
    AudioParams* aOutParams_ = nullptr;
    MediaClock* syncClock_ = nullptr;
    AudioSource* audioSource_ = nullptr;
    MediaSpeedFilter* speedFilter_ = nullptr;
    AudioMonitor* audioMonitor_ = nullptr;
    SwrContext* swrOutputCtx_ = nullptr;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_paused = false;
    std::atomic<bool> m_isRecording{false};
    std::atomic<bool> m_isActived{false};
    std::mutex mutex_;
    std::condition_variable pauseCond_;
    std::mutex pauseMutex_;

    std::atomic<float> m_dB = -60.0f;
    std::atomic<float> m_volumeGain;

    MixerListenMode m_listenMode = MixerListenMode::CLOSE;

    AVAudioFifo* audioFifo = nullptr; // FIFO 缓冲区
    const int TARGET_SAMPLES = 1024;  // 你需要的固定样本数
    std::queue<std::pair<int, int64_t>> ptsQueue_;  // 存储：<样本数, PTS>
    bool isFirstFrame = true;
    int64_t lastAudioPtsUs_ = 0;
    int64_t lastFramePts = 0;
    std::mutex ptsQueueMutex_;  // 保护队列线程安全
    bool hasVideo_ = false;
    int frameCount = 0;

    std::shared_ptr<SeekSync> seekSync_;
    int localSerial_ = 0;
    bool firstFrameAfterSeekA = false;



};

#endif // ADECODER_H
