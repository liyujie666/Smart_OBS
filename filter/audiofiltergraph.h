#ifndef AUDIOFILTERGRAPH_H
#define AUDIOFILTERGRAPH_H

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
}

#include <QVector>
#include <mutex>

class AudioFilterGraph {
public:
    AudioFilterGraph();
    ~AudioFilterGraph();

    bool init(int sourceNums,int sampleRate,AVSampleFormat sampleFmt,int nbChannels);
    bool addFrame(int sourceIndex,AVFrame* frame);
    AVFrame* getMixedFrame();

    void flush();
    void reset();


private:
    int sampleRate_ = 0;
    AVSampleFormat sampleFmt_ = AV_SAMPLE_FMT_FLTP;
    int nbChannels_ = 2;
    int sourceNums_ = 3;

    AVFilterGraph* filterGraph_ = nullptr;
    QVector<AVFilterContext*> abufferCtxs_;
    AVFilterContext* amixCtx_ = nullptr;
    AVFilterContext* abuffersinkCtx_ = nullptr;

    QVector<int64_t> nextPtsPerInput_;
    std::mutex mutex_;
    bool initialized_ = false;
};
#endif // AUDIOFILTERGRAPH_H
