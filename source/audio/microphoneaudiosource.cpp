#include "microphoneaudiosource.h"

MicrophoneAudioSource::MicrophoneAudioSource(int sourceId,int sceneId,const AudioDeviceParams& params, QObject *parent)
    :AudioSource(sourceId, sceneId,parent), m_params(params)
{

}

MicrophoneAudioSource::~MicrophoneAudioSource()
{
    // if (m_microFmtCtx) {
    //     avformat_close_input(&m_microFmtCtx);
    // }
}

int MicrophoneAudioSource::open()
{
    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "sample_rate", QString::number(m_params.sampleRate).toUtf8().constData(), 0);
    av_dict_set(&opts, "channels", QString::number(m_params.channels).toUtf8().constData(), 0);
    av_dict_set(&opts, "buffer_size", "1024", 0);
    QString format = getPlatformDevicePrefix();

    const AVInputFormat* inputFmt = av_find_input_format(format.toUtf8().constData());

    if (!inputFmt) {
        qWarning() << "Unable to find input format:" << format;
        return -1;
    }
    QString deviceName = "audio=" +  m_params.deviceName;
    int ret = avformat_open_input(&m_microFmtCtx, deviceName.toUtf8().constData(), inputFmt, &opts);

    av_dict_free(&opts);

    if (ret < 0) {
        qWarning() << "Failed to open microphone:" << m_params.deviceName;
        return ret;
    }

    ret = avformat_find_stream_info(m_microFmtCtx,nullptr);
    if(ret < 0){
        avformat_close_input(&m_microFmtCtx);
        return ret;
    }

    return 0;
}

AVFormatContext *MicrophoneAudioSource::getFormatContext()
{
    return m_microFmtCtx;
}

QString MicrophoneAudioSource::name() const
{
    return m_params.nickName;
}

AudioSourceType MicrophoneAudioSource::type() const
{
    return AudioSourceType::Microphone;
}

void MicrophoneAudioSource::rename(const QString &newName)
{
    m_params.nickName = newName;
}

QString MicrophoneAudioSource::getPlatformDevicePrefix() const
{
#ifdef Q_OS_WIN
    return "dshow";
#elif defined(Q_OS_MACOS)
    return "avfoundation";
#else
    return "alsa";
#endif
}

