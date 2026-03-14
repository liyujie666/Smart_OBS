#ifndef DESKTOPAUDIOSOURCE_H
#define DESKTOPAUDIOSOURCE_H

#include "audiosource.h"

class DesktopAudioSource : public AudioSource
{
public:
    explicit DesktopAudioSource(int sourceId,int sceneId,const AudioDeviceParams& params,QObject* parent = nullptr);
    ~DesktopAudioSource();

    int open() override;
    AVFormatContext* getFormatContext() override;
    QString name() const override;
    AudioSourceType type() const override;
    void rename(const QString& newName) override;

    QString getPlatformDevicePrefix() const;

private:
    AVFormatContext* m_desktopFmtCtx = nullptr;
    AudioDeviceParams m_params;
};

#endif // DESKTOPAUDIOSOURCE_H
