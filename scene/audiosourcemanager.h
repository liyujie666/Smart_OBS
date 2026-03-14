#ifndef AUDIOSOURCEMANAGER_H
#define AUDIOSOURCEMANAGER_H

#include <QObject>
#include <QMap>
#include <QVector>
#include "source/audio/audiosource.h"

class AudioSourceManager : public QObject {
    Q_OBJECT
public:
    explicit AudioSourceManager(QObject* parent = nullptr);
    ~AudioSourceManager();
    // 添加音源
    void addGlobalSource(AudioSource* source);                 // 麦克风/桌面音频
    void addSceneSource(int sceneId, AudioSource* source);     // 媒体源

    void removeSceneSource(int sceneId,int sourceId);

    // 获取音源
    QVector<AudioSource*> globalSources() const;
    QVector<AudioSource*> sceneSources(int sceneId) const;

    // 当前激活场景 ID
    void setCurrentScene(int sceneId);
    QVector<AudioSource*> currentActiveSources() const;

    int allocteSourceId();
    void release();

signals:
    void sourcesChanged();

private:
    QVector<AudioSource*> m_globalSources;
    QMap<int, QVector<AudioSource*>> m_sceneSources;

    int m_currentSceneId = -1;
    int m_nextSourceId = 0;
};

#endif // AUDIOSOURCEMANAGER_H
