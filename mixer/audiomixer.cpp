#include "audiomixer.h"
#include "pool/gloabalpool.h"
AudioMixer::AudioMixer(QObject *parent) : QObject(parent) {
}

AudioMixer::~AudioMixer() {
    release();
}

bool AudioMixer::init(int sampleRate, int channels, AVSampleFormat format)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (isInitialized_) {
        qDebug() << "AudioMixer already initialized";
        return false;
    }

    // 设置输出音频信息
    outputInfo_.sampleRate = sampleRate;
    outputInfo_.channels = channels;
    outputInfo_.format = format;

    if(!initFilterGraph())
    {
        qDebug() << "filterGraph init failed!";
        return false;
    }
    isInitialized_ = true;
    return true;
}

bool AudioMixer::addInput(int index, int sampleRate, int channels, AVSampleFormat format) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (isInitialized_ && filterGraph_ != nullptr) {
        qDebug() << "Cannot add input after filter graph initialized";
        return false;
    }

    // 检查输入格式是否支持
    if (!av_sample_fmt_is_planar(format) &&
        av_get_bytes_per_sample(format) <= 0) {
        qDebug() << "Unsupported sample format for input" << index;
        return false;
    }

    AudioInfo info;
    info.sampleRate = sampleRate;
    info.channels = channels;
    info.format = format;
    info.filterCtx = nullptr;

    inputInfos_[index] = info;
    qDebug() << "Added audio input" << index << "sample rate:" << sampleRate
             << "channels:" << channels << "format:" << av_get_sample_fmt_name(format);

    return true;
}

bool AudioMixer::initFilterGraph() {
    if (filterGraph_ != nullptr) {
        avfilter_graph_free(&filterGraph_);
    }

    // 创建过滤器图
    filterGraph_ = avfilter_graph_alloc();
    if (!filterGraph_) {
        qDebug() << "Failed to allocate filter graph";
        return false;
    }

    // 创建混音过滤器
    const AVFilter* amixFilter = avfilter_get_by_name("amix");
    if (!amixFilter) {
        qDebug() << "Failed to get amix filter";
        return false;
    }

    // 设置混音过滤器参数
    char args[512];
    int inputCount = inputInfos_.size();
    if (inputCount <= 0) {
        qDebug() << "No input sources available for amix";
        return false;
    }

    // 正确格式化参数：inputs=实际数量，避免超出范围
    snprintf(args, sizeof(args),
             "inputs=%d:duration=longest:dropout_transition=0.1",  // 减小transition值避免参数过大
             inputCount);

    mixFilterCtx_ = avfilter_graph_alloc_filter(filterGraph_, amixFilter, "amix");
    if (!mixFilterCtx_) {
        qDebug() << "Failed to allocate amix filter context";
        return false;
    }

    int ret = avfilter_init_str(mixFilterCtx_, args);
    if (ret < 0) {
        setErrorString(ret);
        qDebug() << "Failed to initialize amix filter:" << errorString_;
        return false;
    }

    // 创建输出格式过滤器
    const AVFilter* aformatFilter = avfilter_get_by_name("aformat");
    if (!aformatFilter) {
        qDebug() << "Failed to get aformat filter";
        return false;
    }

    AVFilterContext* formatFilterCtx = avfilter_graph_alloc_filter(
        filterGraph_, aformatFilter, "aformat");
    if (!formatFilterCtx) {
        qDebug() << "Failed to allocate aformat filter context";
        return false;
    }

    // 设置输出格式参数
    AVChannelLayout layout;
    av_channel_layout_default(&layout,outputInfo_.channels);
    snprintf(args, sizeof(args), "sample_fmts=%s:sample_rates=%d:channel_layouts=0x%lx",
             av_get_sample_fmt_name(outputInfo_.format),
             outputInfo_.sampleRate,
             (unsigned long)layout.u.mask);
    av_channel_layout_uninit(&layout);

    ret = avfilter_init_str(formatFilterCtx, args);
    if (ret < 0) {
        setErrorString(ret);
        qDebug() << "Failed to initialize aformat filter:" << errorString_;
        return false;
    }

    // 创建输出过滤器
    const AVFilter* abuffersinkFilter = avfilter_get_by_name("abuffersink");
    if (!abuffersinkFilter) {
        qDebug() << "Failed to get abuffersink filter";
        return false;
    }

    sinkFilterCtx_ = avfilter_graph_alloc_filter(
        filterGraph_, abuffersinkFilter, "output");
    if (!sinkFilterCtx_) {
        qDebug() << "Failed to allocate abuffersink filter context";
        return false;
    }

    ret = avfilter_init_str(sinkFilterCtx_, nullptr);
    if (ret < 0) {
        setErrorString(ret);
        qDebug() << "Failed to initialize abuffersink filter:" << errorString_;
        return false;
    }

    // 创建输入过滤器并链接
    qDebug() << "inputInfos_ size" << inputInfos_.size();
    for (auto& [index, info] : inputInfos_) {
        const AVFilter* abufferFilter = avfilter_get_by_name("abuffer");
        if (!abufferFilter) {
            qDebug() << "Failed to get abuffer filter for input" << index;
            return false;
        }

        // 设置输入缓冲区过滤器参数
        av_channel_layout_default(&layout,info.channels);
        snprintf(args, sizeof(args),
                 "sample_rate=%d:sample_fmt=%s:channel_layout=0x%lx",
                 info.sampleRate,
                 av_get_sample_fmt_name(info.format),
                  (unsigned long)layout.u.mask);

        av_channel_layout_uninit(&layout);

        info.filterCtx = avfilter_graph_alloc_filter(
            filterGraph_, abufferFilter, QString("input%1").arg(index).toUtf8().constData());
        if (!info.filterCtx) {
            qDebug() << "Failed to allocate abuffer filter for input" << index;
            return false;
        }

        ret = avfilter_init_str(info.filterCtx, args);
        if (ret < 0) {
            setErrorString(ret);
            qDebug() << "Failed to initialize abuffer filter for input" << index
                     << ":" << errorString_;
            return false;
        }

        // 链接输入过滤器到混音过滤器
        ret = avfilter_link(info.filterCtx, 0, mixFilterCtx_, index);
        if (ret < 0) {
            setErrorString(ret);
            qDebug() << "Failed to link input" << index << "to amix filter:" << errorString_;
            return false;
        }
    }

    // 链接混音过滤器到格式转换过滤器
    ret = avfilter_link(mixFilterCtx_, 0, formatFilterCtx, 0);
    if (ret < 0) {
        setErrorString(ret);
        qDebug() << "Failed to link amix to aformat filter:" << errorString_;
        return false;
    }

    // 链接格式转换过滤器到输出过滤器
    ret = avfilter_link(formatFilterCtx, 0, sinkFilterCtx_, 0);
    if (ret < 0) {
        setErrorString(ret);
        qDebug() << "Failed to link aformat to abuffersink filter:" << errorString_;
        return false;
    }

    // 配置过滤器图
    ret = avfilter_graph_config(filterGraph_, nullptr);
    if (ret < 0) {
        setErrorString(ret);
        qDebug() << "Failed to configure filter graph:" << errorString_;
        return false;
    }

    qDebug() << "Filter graph initialized successfully with" << inputInfos_.size() << "inputs";
    return true;
}

bool AudioMixer::addFrame(int index, AVFrame* frame) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!isInitialized_ || !filterGraph_) {
        qDebug() << "Audio mixer not initialized";
        return false;
    }

    // if (frame && frame->pts != AV_NOPTS_VALUE) {
    //     qDebug() << "Input" << index << "frame pts:" << frame->pts;
    // }
    // 查找输入信息
    auto it = inputInfos_.find(index);
    if (it == inputInfos_.end()) {
        qDebug() << "Input" << index << "not found";
        return false;
    }

    AudioInfo& info = it->second;
    if (!info.filterCtx) {
        qDebug() << "Filter context for input" << index << "not initialized";
        return false;
    }

    // 检查帧是否有效
    if (!frame) {
        // 发送空帧表示输入结束
        int ret = av_buffersrc_add_frame(info.filterCtx, nullptr);
        if (ret < 0) {
            setErrorString(ret);
            qDebug() << "Failed to send EOF to input" << index << ":" << errorString_;
            return false;
        }
        return true;
    }

    // 检查帧格式是否匹配
    if (frame->format != info.format ||
        frame->sample_rate != info.sampleRate ||
        frame->ch_layout.nb_channels != info.channels) {
        qDebug() << "Frame format mismatch for input" << index;
        return false;
    }

    // 向过滤器添加帧
    int ret = av_buffersrc_add_frame(info.filterCtx, frame);
    if (ret < 0) {
        setErrorString(ret);
        // qDebug() << "Failed to add frame to input" << index << ":" << errorString_;
        return false;
    }
    return true;
}

AVFrame* AudioMixer::getMixedFrame() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!isInitialized_ || !filterGraph_ || !sinkFilterCtx_) {
        qDebug() << "AudioMixer not initialized or sink missing";
        return nullptr;
    }

    AVFrame* out = GlobalPool::getFramePool().get();
    if (!out) {
        qDebug() << "Failed to allocate output AVFrame";
        return nullptr;
    }

    int ret = av_buffersink_get_frame(sinkFilterCtx_, out);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        // 暂无输出帧或已到 EOF
        GlobalPool::getFramePool().recycle(out);
        return nullptr;
    } else if (ret < 0) {
        // 真正的错误
        setErrorString(ret);
        qDebug() << "Failed to get frame from abuffersink:" << errorString_;
        GlobalPool::getFramePool().recycle(out);
        return nullptr;
    }

    return out;
}

std::map<int, AudioInfo> AudioMixer::inputInfos() const
{
    return inputInfos_;
}

void AudioMixer::release() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (filterGraph_) {
        avfilter_graph_free(&filterGraph_);
        filterGraph_ = nullptr;
    }

    mixFilterCtx_ = nullptr;
    sinkFilterCtx_ = nullptr;
    inputInfos_.clear();

    isInitialized_ = false;
    qDebug() << "Audio mixer resources released";
}

void AudioMixer::setErrorString(int ret) {
    char errbuf[1024];
    av_strerror(ret, errbuf, sizeof(errbuf));
    errorString_ = QString(errbuf);
}


