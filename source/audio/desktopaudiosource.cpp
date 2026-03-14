#include "desktopaudiosource.h"


DesktopAudioSource::DesktopAudioSource(int sourceId,int sceneId,const AudioDeviceParams& params,QObject *parent)
    :AudioSource(sourceId,sceneId,parent), m_params(params)
{

}

DesktopAudioSource::~DesktopAudioSource()
{
    // if (m_desktopFmtCtx) {
    //     avformat_close_input(&m_desktopFmtCtx);
    // }
}

int DesktopAudioSource::open()
{
    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "sample_rate", QString::number(m_params.sampleRate).toUtf8().constData(), 0);
    av_dict_set(&opts, "channels", QString::number(m_params.channels).toUtf8().constData(), 0);

    QString format = getPlatformDevicePrefix();

    const AVInputFormat* inputFmt = av_find_input_format(format.toUtf8().constData());

    if (!inputFmt) {
        qWarning() << "Unable to find input format:" << format;
        return -1;
    }
    QString deviceName = "audio=" +  m_params.deviceName;
    int ret = avformat_open_input(&m_desktopFmtCtx, deviceName.toUtf8().constData(), inputFmt, &opts);

    av_dict_free(&opts);

    if (ret < 0) {
        qWarning() << "Failed to desktop audio:" << m_params.deviceName;
        return ret;
    }


    ret = avformat_find_stream_info(m_desktopFmtCtx,nullptr);
    if(ret < 0){
        avformat_close_input(&m_desktopFmtCtx);
        return ret;
    }
    return 0;
}

AVFormatContext *DesktopAudioSource::getFormatContext()
{
    return m_desktopFmtCtx;
}

QString DesktopAudioSource::name() const
{
    return m_params.nickName;
}

AudioSourceType DesktopAudioSource::type() const
{
    return AudioSourceType::Desktop;
}

void DesktopAudioSource::rename(const QString &newName)
{
    m_params.nickName = newName;
}

QString DesktopAudioSource::getPlatformDevicePrefix() const
{
#ifdef Q_OS_WIN
    return "dshow";
#elif defined(Q_OS_MACOS)
    return "avfoundation";
#else
    return "alsa";
#endif
}


