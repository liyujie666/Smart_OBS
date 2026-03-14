#include <QObject>
#include <QMutex>
#include <QThread>
#include <QDebug>
#include <map>

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
}

struct AudioInfo{
    int sampleRate;
    int channels;
    AVSampleFormat format;
    AVFilterContext* filterCtx;
};

class AudioMixer : public QObject
{
    Q_OBJECT

public:
    AudioMixer(QObject* parent = nullptr);
    ~AudioMixer();

    bool init(int sampleRate,int channels,AVSampleFormat format);
    bool addInput(int index, int sampleRate = 48000, int channels = 2, AVSampleFormat format = AV_SAMPLE_FMT_FLTP);
    bool addFrame(int index, AVFrame* frame);
    AVFrame* getMixedFrame();
    std::map<int,AudioInfo> inputInfos() const;
    void release();

private:
    bool initFilterGraph();
    void setErrorString(int ret);

    AVFilterGraph* filterGraph_ = nullptr;
    AVFilterContext* mixFilterCtx_ = nullptr;
    AVFilterContext* sinkFilterCtx_ = nullptr;
    std::map<int, AudioInfo> inputInfos_;
    AudioInfo outputInfo_;
    bool isInitialized_ = false;
    QString errorString_;
    std::mutex mutex_;

};


