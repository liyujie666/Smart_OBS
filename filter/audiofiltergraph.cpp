#include "audiofiltergraph.h"
#include "pool/gloabalpool.h"
#include <QDebug>
AudioFilterGraph::AudioFilterGraph() {}

AudioFilterGraph::~AudioFilterGraph()
{
    reset();
}

bool AudioFilterGraph::init(int sourceNums, int sampleRate, AVSampleFormat sampleFmt, int nbChannels)
{
    reset();
    std::lock_guard<std::mutex> lock(mutex_);

    sourceNums_ = sourceNums;
    sampleRate_ = sampleRate;
    sampleFmt_ = sampleFmt;
    nbChannels_ = nbChannels;

    filterGraph_ = avfilter_graph_alloc();
    if(!filterGraph_) return false;

    char args[512];
    char name[32];

    nextPtsPerInput_.resize(sourceNums, AV_NOPTS_VALUE);

    for(int i=0; i < sourceNums; ++i)
    {
        snprintf(name, sizeof(name), "in%d", i);
        const AVFilter* abuffer = avfilter_get_by_name("abuffer");

        AVChannelLayout ch_layout = {};
        uint64_t channel_mask = 0;
        av_channel_layout_default(&ch_layout,nbChannels);
        if (ch_layout.order == AV_CHANNEL_ORDER_NATIVE) {
            channel_mask = ch_layout.u.mask;
        }

        snprintf(args, sizeof(args),
                 "sample_fmt=%s:sample_rate=%d:time_base=1/%d:channel_layout=0x%" PRIx64,
                 av_get_sample_fmt_name(sampleFmt), sampleRate, sampleRate, channel_mask);

        AVFilterContext* ctx = nullptr;
        if (avfilter_graph_create_filter(&ctx, abuffer, name, args, nullptr, filterGraph_) < 0) {
            qDebug() << "Failed to create abuffer " << i;
            reset();
            return false;
        }

        abufferCtxs_.push_back(ctx);
    }

    const AVFilter* amix = avfilter_get_by_name("amix");
    snprintf(args, sizeof(args), "inputs=%d:duration=longest", sourceNums);
    if (avfilter_graph_create_filter(&amixCtx_, amix, "amix", args, nullptr, filterGraph_) < 0) {
        qDebug() << "Failed to create amix filter";
        reset();
        return false;
    }

    // 创建 abuffersink
    const AVFilter* abuffersink = avfilter_get_by_name("abuffersink");
    if (avfilter_graph_create_filter(&abuffersinkCtx_, abuffersink, "out", nullptr, nullptr, filterGraph_) < 0) {
        qDebug() << "Failed to create abuffersink";
        reset();
        return false;
    }

    // 链接各个输入
    for (int i = 0; i < sourceNums; ++i) {
        if (avfilter_link(abufferCtxs_[i], 0, amixCtx_, i) < 0) {
            qDebug() << "Failed to link abuffer to amix";
            reset();
            return false;
        }
    }

    if (avfilter_link(amixCtx_, 0, abuffersinkCtx_, 0) < 0) {
        qDebug() << "Failed to link amix to abuffersink";
        reset();
        return false;
    }

    if (avfilter_graph_config(filterGraph_, nullptr) < 0) {
        qDebug() << "Failed to config filter graph";
        reset();
        return false;
    }

    initialized_ = true;
    return true;
}

bool AudioFilterGraph::addFrame(int sourceIndex, AVFrame *frame)
{
    std::lock_guard<std::mutex> locker(mutex_);
    if (!initialized_ || sourceIndex >= abufferCtxs_.size() || !frame) {
        qDebug() << "[addFrame] invalid input. initialized:" << initialized_
                 << " sourceIndex:" << sourceIndex << " frame:" << (void*)frame;
        return false;
    }

    int64_t& nextPts = nextPtsPerInput_[sourceIndex];
    if (nextPts == AV_NOPTS_VALUE) {
        nextPts = frame->pts;
    } else {
        if (frame->pts > nextPts) {
            int64_t diff = frame->pts - nextPts;
            int nb_samples = static_cast<int>(diff);  // 假设时间基是 1/sample_rate

            AVFrame* silenceFrame = GlobalPool::getFramePool().get();
            silenceFrame->nb_samples = nb_samples;
            silenceFrame->format = sampleFmt_;
            silenceFrame->sample_rate = sampleRate_;
            av_channel_layout_default(&silenceFrame->ch_layout, nbChannels_);

            int ret = av_frame_get_buffer(silenceFrame, 0);
            if (ret < 0) {
                qDebug() << "[addFrame] av_frame_get_buffer failed!";
                GlobalPool::getFramePool().recycle(silenceFrame);
                return false;
            }

            av_samples_set_silence(silenceFrame->data, 0, nb_samples, nbChannels_, sampleFmt_);
            silenceFrame->pts = nextPts;

            ret = av_buffersrc_add_frame_flags(abufferCtxs_[sourceIndex], silenceFrame, AV_BUFFERSRC_FLAG_PUSH);
            if (ret < 0) {
                char errbuf[256];
                av_strerror(ret, errbuf, sizeof(errbuf));
                qDebug() << "[addFrame] silent frame failed! err:" << errbuf;
                GlobalPool::getFramePool().recycle(silenceFrame);
                return false;
            }

            GlobalPool::getFramePool().recycle(silenceFrame);
            nextPts += nb_samples;
        }

        if (frame->pts < nextPts) {
            frame->pts = nextPts;  // 可选策略
        }
    }

    int ret = av_buffersrc_add_frame_flags(abufferCtxs_[sourceIndex], frame, AV_BUFFERSRC_FLAG_PUSH);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        qDebug() << "[addFrame] av_buffersrc_add_frame failed! err:" << errbuf;
        return false;
    }

    nextPts += frame->nb_samples;
    return true;
}

AVFrame* AudioFilterGraph::getMixedFrame()
{
    std::lock_guard<std::mutex> locker(mutex_);
    if (!initialized_) return nullptr;

    AVFrame* frame = GlobalPool::getFramePool().get();
    int ret = av_buffersink_get_frame(abuffersinkCtx_, frame);
    if (ret >= 0) {
        qDebug() << "[getMixedFrame] mixed frame obtained, pts:" << frame->pts;
        return frame;
    }

    if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        qDebug() << "[getMixedFrame] failed to get frame:" << errbuf;
    }

    GlobalPool::getFramePool().recycle(frame);
    return nullptr;
}

void AudioFilterGraph::flush()
{
    std::lock_guard<std::mutex> locker(mutex_);
    for (auto& src : abufferCtxs_) {
        av_buffersrc_add_frame_flags(src, nullptr, AV_BUFFERSRC_FLAG_PUSH);
    }
}

void AudioFilterGraph::reset()
{
    std::lock_guard<std::mutex> locker(mutex_);
    if(filterGraph_)
    {
        avfilter_graph_free(&filterGraph_);
    }

    abufferCtxs_.clear();
    amixCtx_ = nullptr;
    abuffersinkCtx_ = nullptr;
    initialized_ = false;
}
