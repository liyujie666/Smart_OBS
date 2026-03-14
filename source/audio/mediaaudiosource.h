#ifndef MEDIAAUDIOSOURCE_H
#define MEDIAAUDIOSOURCE_H

#include "audiosource.h"
class LocalVideoSource;
class MediaAudioSource : public AudioSource
{
public:
    explicit MediaAudioSource(int sourceId,int sceneId,const AudioDeviceParams& params, QObject* parent = nullptr);
    ~MediaAudioSource();

    int open() override;
    AVFormatContext* getFormatContext() override;
    QString name() const override;
    AudioSourceType type() const override;
    void rename(const QString& newName) override;
    void close();

    QString getPlatformDevicePrefix() const;
    void setFormatContext(AVFormatContext* fmtCtx);
    void setAudioParams(AudioDeviceParams params);
    void setAudioSourceId(int sourceId);
    AudioDeviceParams getAudioParams() const;
    double duration() const;
    void seek(int64_t us);
    void setVieoSource(LocalVideoSource* videoSouce);
    LocalVideoSource* getAssociatedVideoSource();
    std::tuple<int, AVSampleFormat, AVChannelLayout, AVRational> getAudioFilterParams() const;
    int AudioSourceId() const;
private:
    AVFormatContext* m_mediaFmtCtx = nullptr;
    AudioDeviceParams m_params;
    LocalVideoSource* videoSource_ = nullptr;
    int m_sourceId = -1;
};

#endif // MEDIAAUDIOSOURCE_H
