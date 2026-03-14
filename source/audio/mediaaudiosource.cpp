#include "mediaaudiosource.h"


MediaAudioSource::MediaAudioSource(int sourceId,int sceneId,const AudioDeviceParams& params, QObject *parent)
    :AudioSource(sourceId,sceneId,parent), m_params(params)
{

}

MediaAudioSource::~MediaAudioSource()
{

}

int MediaAudioSource::open()
{
    if(m_mediaFmtCtx) return -1;
    int ret = avformat_open_input(&m_mediaFmtCtx,m_params.deviceName.toUtf8().constData(),nullptr, nullptr);

    if (ret < 0) {
        qWarning() << "Failed to desktop audio:" << m_params.deviceName;
        return ret;
    }

    ret = avformat_find_stream_info(m_mediaFmtCtx,nullptr);
    if(ret < 0)
    {
        qWarning() << "Failed to find stream Info!";
        return -1;
    }


    return 0;
}

AVFormatContext *MediaAudioSource::getFormatContext()
{
    return m_mediaFmtCtx;
}

QString MediaAudioSource::name() const
{
    return m_params.nickName + "音频";
}

AudioSourceType MediaAudioSource::type() const
{
    return AudioSourceType::Media;
}

void MediaAudioSource::rename(const QString &newName)
{
    m_params.nickName = newName;
}

void MediaAudioSource::close()
{
    if (m_mediaFmtCtx) {
        avformat_close_input(&m_mediaFmtCtx);
    }
    videoSource_ = nullptr;
}

QString MediaAudioSource::getPlatformDevicePrefix() const
{
#ifdef Q_OS_WIN
    return "dshow";
#elif defined(Q_OS_MACOS)
    return "avfoundation";
#else
    return "alsa";
#endif
}

void MediaAudioSource::setFormatContext(AVFormatContext *fmtCtx)
{
    m_mediaFmtCtx = fmtCtx;
}

void MediaAudioSource::setAudioParams(AudioDeviceParams params)
{
    m_params = params;
}

void MediaAudioSource::setAudioSourceId(int sourceId)
{
    m_sourceId = sourceId;
}

AudioDeviceParams MediaAudioSource::getAudioParams() const
{
    return m_params;
}

double MediaAudioSource::duration() const
{
    if(!m_mediaFmtCtx) return 0;
    return m_mediaFmtCtx ->duration / (double) AV_TIME_BASE;
}

void MediaAudioSource::seek(int64_t us) {
    if (m_mediaFmtCtx) {
        av_seek_frame(m_mediaFmtCtx, -1, us, AVSEEK_FLAG_BACKWARD);
        avformat_flush(m_mediaFmtCtx);
    }
}

void MediaAudioSource::setVieoSource(LocalVideoSource *videoSource)
{
    videoSource_ = videoSource;
}

LocalVideoSource *MediaAudioSource::getAssociatedVideoSource()
{
    return videoSource_;
}


std::tuple<int, AVSampleFormat, AVChannelLayout, AVRational> MediaAudioSource::getAudioFilterParams() const {
    // 初始化默认无效参数
    AVChannelLayout invalidLayout;  // 无效声道布局
    std::tuple<int, AVSampleFormat, AVChannelLayout, AVRational> audioParams =
        std::make_tuple(0, AV_SAMPLE_FMT_NONE, invalidLayout, AVRational{0, 1});

    // 媒体上下文无效时直接返回
    if (!m_mediaFmtCtx) {
        return audioParams;
    }

    // 遍历流寻找音频流
    for (int i = 0; i < m_mediaFmtCtx->nb_streams; ++i) {
        AVStream* stream = m_mediaFmtCtx->streams[i];
        AVCodecParameters* codecPar = stream->codecpar;

        if (codecPar->codec_type == AVMEDIA_TYPE_AUDIO) {

            // 组装音频参数元组
            audioParams = std::make_tuple(
                codecPar->sample_rate,                 // 采样率
                static_cast<AVSampleFormat>(codecPar->format),  // 采样格式
                codecPar->ch_layout,                              // 声道布局（AVChannelLayout类型）
                stream->time_base                      // 时间基
                );
            break;  // 找到第一个音频流后退出
        }
    }

    return audioParams;
}

int MediaAudioSource::AudioSourceId() const
{
    return m_sourceId;
}
