#include "mediaspeedfilter.h"
#include <QDebug>
MediaSpeedFilter::MediaSpeedFilter() {
    m_currentSpeed = Speed::S1_0;
    m_speedValue = 1.0;
}

MediaSpeedFilter::~MediaSpeedFilter()
{
    releaseFilters();
}

bool MediaSpeedFilter::initialize(bool hasVideo, bool hasAudio,
                                  const std::tuple<int, int, AVPixelFormat, AVRational> &videoParams,
                                  const std::tuple<int, AVSampleFormat, AVChannelLayout, AVRational> &audioParams,
                                  Speed initialSpeed)
{
    releaseFilters();

    m_hasAudio = hasAudio;
    m_hasVideo = hasVideo;
    m_videoParams = videoParams;
    m_audioParams = audioParams;

    m_currentSpeed = initialSpeed;
    switch (initialSpeed) {
        case Speed::S0_5: m_speedValue = 0.5; break;
        case Speed::S1_0: m_speedValue = 1.0; break;
        case Speed::S1_5: m_speedValue = 1.5; break;
        case Speed::S2_0: m_speedValue = 2.0; break;
    }

    bool res = initFilterGraph();
    m_initialized = res;

    return res;


}

bool MediaSpeedFilter::initFilterGraph()
{
    if(m_filterGraph)
    {
        avfilter_graph_free(&m_filterGraph);
    }

    m_filterGraph = avfilter_graph_alloc();
    if(!m_filterGraph)
    {
        qDebug() << "滤镜初始化失败";
        return false;
    }

    if(m_hasVideo && !m_hasAudio)
    {
        int width = std::get<0>(m_videoParams);
        int height = std::get<1>(m_videoParams);
        AVPixelFormat pixFmt = std::get<2>(m_videoParams);
        AVRational timeBase = std::get<3>(m_videoParams);

        if(!initVideoFilters(width,height,pixFmt,timeBase))
        {
            qDebug() << "视频倍速滤镜初始化失败";
            avfilter_graph_free(&m_filterGraph);
            m_filterGraph = nullptr;
            return false;
        }
    }else
    {
        int sampleRate = std::get<0>(m_audioParams);
        AVSampleFormat sampleFmt = std::get<1>(m_audioParams);
        AVChannelLayout ch_layout = std::get<2>(m_audioParams);
        AVRational timeBase = std::get<3>(m_audioParams);

        if (!initAudioFilters(sampleRate, sampleFmt, ch_layout, timeBase)) {
            qDebug() << "音频倍速滤镜初始化失败";
            avfilter_graph_free(&m_filterGraph);
            m_filterGraph = nullptr;
            return false;
        }
    }

    if (avfilter_graph_config(m_filterGraph, nullptr) < 0) {
        qDebug() << "无法配置滤镜图";
        avfilter_graph_free(&m_filterGraph);
        m_filterGraph = nullptr;
        return false;
    }

    if (m_audioSinkCtx->outputs && m_audioSinkCtx->outputs[0]) {
        m_audioOutTimeBase = m_audioSinkCtx->outputs[0]->time_base;
        qDebug() << "滤镜输出time_base: " << m_audioOutTimeBase.num << "/" << m_audioOutTimeBase.den;
    } else {
        qWarning() << "无法获取滤镜输出链路，使用默认time_base";
        m_audioOutTimeBase = av_make_q(1, 1000000); // 默认为微秒
    }

    return true;
}

bool MediaSpeedFilter::changeSpeed(Speed newSpeed) {
    if (!m_initialized) {
        qDebug() << "滤镜未初始化，无法改变倍速";
        return false;
    }

    // 如果倍速未改变，直接返回
    if (newSpeed == m_currentSpeed) {
        return true;
    }

    // 保存当前EOF状态
    bool wasVideoEOF = m_videoEOF;
    bool wasAudioEOF = m_audioEOF;

    // 更新倍速值
    m_currentSpeed = newSpeed;
    switch (newSpeed) {
    case Speed::S0_5: m_speedValue = 0.5; break;
    case Speed::S1_0: m_speedValue = 1.0; break;
    case Speed::S1_5: m_speedValue = 1.5; break;
    case Speed::S2_0: m_speedValue = 2.0; break;
    }

    // 重建滤镜链
    bool result = initFilterGraph();

    // 恢复EOF状态
    m_videoEOF = wasVideoEOF;
    m_audioEOF = wasAudioEOF;

    return result;
}

int MediaSpeedFilter::getVideoFrame(AVFrame* srcVFrame, AVFrame* dstVFrame) {

    if (!m_initialized || !m_hasVideo || !srcVFrame || !dstVFrame) {
        return -1;
    }

    // 清除目标帧数据
    av_frame_unref(dstVFrame);

    // 推送源帧到滤镜
    int ret = av_buffersrc_add_frame(m_videoSrcCtx, srcVFrame);
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
        qDebug() << "推送视频帧到滤镜失败: " << ret;
        return ret;
    }

    // 尝试从滤镜获取处理后的帧
    ret = av_buffersink_get_frame(m_videoSinkCtx, dstVFrame);
    return ret;
}
int MediaSpeedFilter::getAudioFrame(AVFrame* srcAFrame, AVFrame* dstAFrame) {
    if (!m_initialized || !m_hasAudio || !srcAFrame || !dstAFrame) {
        return -1;
    }

    // 清除目标帧数据

    // 推送源帧到滤镜
    int ret = av_buffersrc_add_frame(m_audioSrcCtx, srcAFrame);
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
        qDebug()<< "推送音频帧到滤镜失败: " << ret;
        return ret;
    }

    // 尝试从滤镜获取处理后的帧
    ret = av_buffersink_get_frame(m_audioSinkCtx, dstAFrame);
    return ret;
}

std::tuple<int, AVSampleFormat, AVChannelLayout, AVRational> MediaSpeedFilter::getAudioOutputParams() const
{
    return m_audioParams;
}

bool MediaSpeedFilter::isInitialized()
{
    return m_initialized;
}

// void MediaSpeedFilter::sendEOF() {

//     if (m_hasVideo && m_videoSrcCtx && !m_videoEOF) {
//         // 使用AV_BUFFERSRC_FLAG_END_OF_STREAM替代
//         av_buffersrc_add_frame_flags(m_videoSrcCtx, nullptr, AV_BUFFERSRC_FLAG_END_OF_STREAM);
//         m_videoEOF = true;
//     }

//     if (m_hasAudio && m_audioSrcCtx && !m_audioEOF) {
//         av_buffersrc_add_frame_flags(m_audioSrcCtx, nullptr, AV_BUFFERSRC_FLAG_END_OF_STREAM);
//         m_audioEOF = true;
//     }
// }

void MediaSpeedFilter::reset() {
    m_videoEOF = false;
    m_audioEOF = false;

    // 重置滤镜链但保持当前倍速
    if (m_initialized) {
        initFilterGraph();
    }
}

void MediaSpeedFilter::releaseFilters() {
    if (m_filterGraph) {
        avfilter_graph_free(&m_filterGraph);
        m_filterGraph = nullptr;
    }

    m_videoSrcCtx = nullptr;
    m_videoSinkCtx = nullptr;
    m_audioSrcCtx = nullptr;
    m_audioSinkCtx = nullptr;

    m_initialized = false;
    m_videoEOF = false;
    m_audioEOF = false;
}
bool MediaSpeedFilter::initVideoFilters(int width, int height, AVPixelFormat format, AVRational timebase)
{
    const AVFilter* bufferSrc = avfilter_get_by_name("buffer");
    const AVFilter* bufferSink = avfilter_get_by_name("buffersink");
    AVFilterInOut* outputs = avfilter_inout_alloc();
    AVFilterInOut* inputs = avfilter_inout_alloc();
    std::string args;
    std::string  filterDesc;
    int ret = 0;

    if (!outputs || !inputs || !bufferSrc || !bufferSink) {
        qDebug() << "无法分配滤镜输入输出对象";
        return false;
    }

    // 配置buffer源滤镜参数
    args = "video_size=" + std::to_string(width) + "x" + std::to_string(height) +
           ":pix_fmt=" + std::to_string(format) +
           ":time_base=" + std::to_string(timebase.num) + "/" + std::to_string(timebase.den) +
           ":pixel_aspect=1/1";

    // 创建视频源滤镜上下文
    ret = avfilter_graph_create_filter(&m_videoSrcCtx,bufferSrc,"video_src",args.c_str(),nullptr,m_filterGraph);
    if(ret < 0)
    {
        qDebug() << "无法创建视频源滤镜";
        goto fail;
    }

    // 创建视频输出滤镜上下文
    ret = avfilter_graph_create_filter(&m_videoSinkCtx,bufferSink,"video_sink",nullptr,nullptr ,m_filterGraph);
    if(ret < 0)
    {
        qDebug() << "无法创建视频输出滤镜";
        goto fail;
    }

    // 链接滤镜
    outputs->name = av_strdup("in");
    outputs->filter_ctx = m_videoSrcCtx;
    outputs->pad_idx = 0;
    outputs->next = nullptr;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = m_videoSinkCtx;
    inputs->pad_idx = 0;
    inputs->next = nullptr;

    // 创建视频倍速滤镜
    filterDesc = "setpts=" + std::to_string(1.0 / m_speedValue) + "*PTS";

    ret = avfilter_graph_parse_ptr(m_filterGraph,filterDesc.c_str(),&inputs,&outputs,nullptr);
    if(ret < 0)
    {
        qDebug() << "无法解析视频滤镜描述: " << filterDesc;
        goto fail;
    }

    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    return true;

fail:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    return false;
}

bool MediaSpeedFilter::initAudioFilters(int sampleRate, AVSampleFormat format, AVChannelLayout ch_layout, AVRational timebase)
{
    const AVFilter* abuffersrc = avfilter_get_by_name("abuffer");
    const AVFilter* abuffersink = avfilter_get_by_name("abuffersink");
    AVFilterInOut* outputs = avfilter_inout_alloc();
    AVFilterInOut* inputs = avfilter_inout_alloc();
    // std::string args;
    std::string filterDesc;
    int ret = 0;

    if (!outputs || !inputs || !abuffersrc || !abuffersink) {
        qDebug() << "无法分配音频滤镜输入输出对象";
        return false;
    }

    // args = "time_base=" + std::to_string(timebase.num) + "/" + std::to_string(timebase.den) +
    //        ":sample_rate=" + std::to_string(sampleRate) +
    //        ":sample_fmt=" + av_get_sample_fmt_name(format) +
    //        ":channel_layout=" + std::to_string(ch_layout.u.mask);

    char args[512];
    snprintf(args, sizeof(args),
             "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%" PRIx64,
             timebase.num, timebase.den,
             sampleRate,
             av_get_sample_fmt_name(format),
             ch_layout.u.mask);

    // 创建音频源滤镜上下文
    ret = avfilter_graph_create_filter(&m_audioSrcCtx, abuffersrc, "audio_src",
                                       args, nullptr, m_filterGraph);
    if (ret < 0) {
        qDebug() << "无法创建音频源滤镜";
        goto fail;
    }

    // 创建音频sink滤镜上下文
    ret = avfilter_graph_create_filter(&m_audioSinkCtx, abuffersink, "audio_sink",
                                       nullptr, nullptr, m_filterGraph);
    if (ret < 0) {
        qDebug() << "无法创建音频输出滤镜";
        goto fail;
    }

    // 设置滤镜链连接
    outputs->name = av_strdup("in");
    outputs->filter_ctx = m_audioSrcCtx;
    outputs->pad_idx = 0;
    outputs->next = nullptr;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = m_audioSinkCtx;
    inputs->pad_idx = 0;
    inputs->next = nullptr;

    // 音频滤镜描述：使用atempo改变速度
    filterDesc = "atempo=" + std::to_string(m_speedValue)+
                ",aformat=sample_fmts=fltp:channel_layouts=" + std::to_string(ch_layout.u.mask);;

    // 解析滤镜描述
    if ((ret = avfilter_graph_parse_ptr(m_filterGraph, filterDesc.c_str(),
                                        &inputs, &outputs, nullptr)) < 0) {
        qDebug() << "无法解析音频滤镜描述: " << filterDesc;
        goto fail;
    }

    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    return true;

fail:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    return false;
}
