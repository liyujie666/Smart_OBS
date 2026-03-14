#include "audiosourcemanager.h"

AudioSourceManager::AudioSourceManager(QObject* parent)
    : QObject(parent) {}

AudioSourceManager::~AudioSourceManager()
{
    release();
}

void AudioSourceManager::addGlobalSource(AudioSource* source) {
    m_globalSources.append(source);
    emit sourcesChanged();
}

void AudioSourceManager::addSceneSource(int sceneId, AudioSource* source) {
    m_sceneSources[sceneId].append(source);
    emit sourcesChanged();
}

void AudioSourceManager::removeSceneSource(int sceneId,int sourceId)
{
    for (auto it = m_sceneSources[sceneId].begin(); it != m_sceneSources[sceneId].end(); ++it)
    {
        if (*it && (*it)->sourceId() == sourceId)
        {
            delete *it;
            m_sceneSources[sceneId].erase(it);
            break;
        }
    }
}

QVector<AudioSource*> AudioSourceManager::globalSources() const {
    return m_globalSources;
}

QVector<AudioSource*> AudioSourceManager::sceneSources(int sceneId) const {
    return m_sceneSources.value(sceneId);
}

void AudioSourceManager::setCurrentScene(int sceneId) {
    if (m_currentSceneId != sceneId) {
        m_currentSceneId = sceneId;
        emit sourcesChanged();
    }
}

QVector<AudioSource*> AudioSourceManager::currentActiveSources() const {
    QVector<AudioSource*> result = m_globalSources;
    result += m_sceneSources.value(m_currentSceneId);
    return result;
}

int AudioSourceManager::allocteSourceId()
{
    return m_nextSourceId++;
}

void AudioSourceManager::release()
{
    // 释放全局音频源
    qDeleteAll(m_globalSources);
    m_globalSources.clear();

    // 释放所有场景的音频源
    for (auto& sceneSources : m_sceneSources) {
        qDeleteAll(sceneSources);
        sceneSources.clear();
    }
    m_sceneSources.clear();

    // 重置状态变量
    m_currentSceneId = -1;
    m_nextSourceId = 0;
}
