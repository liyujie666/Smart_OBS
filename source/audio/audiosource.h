#ifndef AUDIOSOURCE_H
#define AUDIOSOURCE_H

#include <QObject>
#include "infotools.h"
extern"C"{
#include <libavformat/avformat.h>
}

struct AudioDeviceParams {
    QString deviceName;
    QString nickName;
    int sampleRate = 48000;
    int channels = 2;
};

class AudioSource : public QObject
{
    Q_OBJECT
public:
    AudioSource(int sourceId,int sceneId,QObject* parent = nullptr):QObject(parent),m_sourceId(sourceId),m_sceneId(sceneId){};
    virtual ~AudioSource(){};

    virtual int open() = 0;
    virtual AVFormatContext* getFormatContext() = 0;
    virtual QString name() const = 0;
    virtual AudioSourceType type() const = 0;
    virtual void rename(const QString& newName) = 0;

    virtual int sourceId() const { return m_sourceId; }
    virtual int sceneId() const { return m_sceneId; }


    virtual void setVolume(float volume)
    {
        m_volume = volume;
    }

    virtual float volume() const { return m_volume; }

    virtual void mute() {
        if (!m_muted) {
            m_muted = true;
        }
    }

    virtual void unmute() {
        if (m_muted) {
            m_muted = false;
        }
    }


    virtual bool isMute() const { return m_muted; }

    virtual void setActive(bool active) {
        if (m_actived != active) {
            m_actived = active;
        }
    }

    virtual bool isActive() const { return m_actived; }

signals:
    void activeChanged(int sourceId, bool active);

private:
    int m_sourceId = -1;
    int m_sceneId = -1;
    float m_volume = 1.0f;
    bool m_muted = false;
    bool m_actived = true;
};
#endif // AUDIOSOURCE_H
