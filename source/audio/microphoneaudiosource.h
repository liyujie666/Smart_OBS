#ifndef MICROPHONEAUDIOSOURCE_H
#define MICROPHONEAUDIOSOURCE_H

#include "audiosource.h"

class MicrophoneAudioSource : public AudioSource {
public:
    explicit MicrophoneAudioSource(int sourceId,int sceneId,const AudioDeviceParams& params, QObject* parent = nullptr);
    ~MicrophoneAudioSource();

    int open() override;
    AVFormatContext* getFormatContext() override;
    QString name() const override;
    AudioSourceType type() const override;
    void rename(const QString& newName) override;
    QString getPlatformDevicePrefix() const;

private:
    AVFormatContext* m_microFmtCtx = nullptr;
    AudioDeviceParams m_params;
};

#endif // MICROPHONEAUDIOSOURCE_H
