#ifndef VIDEOSOURCE_H
#define VIDEOSOURCE_H

#include <QObject>
#include "infotools.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
}

class VideoSource : public QObject {
    Q_OBJECT
public:
    explicit VideoSource(int sourceId, int sceneId,QObject* parent = nullptr)
        : QObject(parent), m_sourceId(sourceId),m_sceneId(sceneId) {}

    virtual ~VideoSource() {}

    virtual int open() = 0;
    virtual void close() = 0;
    virtual AVFormatContext* getFormatContext() = 0;
    virtual QString name() const = 0;
    virtual VideoSourceType type() const = 0;

    int sourceId() const { return m_sourceId; }
    int sceneId() const { return m_sceneId; }

    virtual void setPriority(int priority) {
        if(m_priority != priority)
        {
            m_priority = priority;
            if (this->type() == VideoSourceType::Text) {
                emit priorityUpdated(m_sourceId); // 触发VSourceTaskThread生成帧
            }
        }

    }
    virtual int priority() const { return m_priority; }

    virtual void setOpacity(float opacity) { m_opacity = opacity; }
    virtual float opacity() const { return m_opacity; }

    virtual void setActive(bool active)
    {
        if (m_active != active) {
            m_active = active;
            emit activeChanged(m_sourceId);
        }
    }

    virtual bool isActive() const { return m_active; }

    virtual void setVisible(bool visible) {
        if (m_visible != visible) {
            m_visible = visible;
            emit visibilityChanged(m_sourceId,m_visible);
        }
    }

    virtual void setLocked(bool locked) {
        if (m_locked != locked) {
            m_locked = locked;
            emit lockStateChanged(m_sourceId,m_locked);
        }
    }

    virtual bool isVisible() const { return m_visible; }
    virtual bool isLocked() const { return m_locked; }



signals:
    void visibilityChanged(int sourceId,bool visible);
    void lockStateChanged(int sourceId,bool locked);
    void activeChanged(int sourceId);
    void priorityUpdated(int sourceId);

protected:
    int m_sourceId = -1;
    int m_sceneId = -1;
    bool m_visible = true;
    bool m_locked = false;
    int m_priority = 0;
    float m_opacity = 1.0f;
    bool m_active = true;
};

#endif // VIDEOSOURCE_H
