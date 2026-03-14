#include "scene.h"
#include "source/video/videosource.h"
#include "source/audio/audiosource.h"
#include "framelayer.h"
Scene::Scene(int id, const QString &name):m_id(id),m_name(name)
{

}

Scene::~Scene()
{
    clear();
}

int Scene::id() const
{
    return m_id;
}

QString Scene::name()
{
    return m_name;
}

int Scene::priority() const
{
    return m_priority;
}

void Scene::addVideoSource(VideoSource *videoSource)
{
    if(!videoSource) return;
    std::lock_guard<std::mutex> locker(m_mutex);
    m_videoSources.push_back(videoSource);
    m_videoSourceIds.insert(videoSource->sourceId());
    sortSourcesByPriority();
}

void Scene::addAudioSource(AudioSource *audioSource)
{
    if(!audioSource) return;
    std::lock_guard<std::mutex> locker(m_mutex);
    m_audioSources.push_back(audioSource);
    m_audioSourceIds.insert(audioSource->sourceId());
}

VideoSource *Scene::findVideoSourceById(int sourceId)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    for(const auto &source : m_videoSources)
    {
        if(source->sourceId() == sourceId)
        {
            return source;
        }
    }
    return nullptr;
}

AudioSource *Scene::findAudioSourceById(int sourceId)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    for(const auto &source : m_audioSources)
    {
        if(source->sourceId() == sourceId)
        {
            return source;
        }
    }
    return nullptr;
}



void Scene::removeVideoSource(int sourceId)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    for (auto it = m_videoSources.begin(); it != m_videoSources.end(); ++it)
    {
        if (*it && (*it)->sourceId() == sourceId)
        {
            delete *it;
            m_videoSources.erase(it);
            m_videoSourceIds.erase(m_videoSourceIds.find(sourceId));
            break;
        }
    }

}

void Scene::removeAudioSource(int sourceId)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    for (auto it = m_audioSources.begin(); it != m_audioSources.end(); ++it)
    {
        if (*it && (*it)->sourceId() == sourceId)
        {
            delete *it;
            m_audioSources.erase(it);
            m_audioSourceIds.erase(m_audioSourceIds.find(sourceId));
            break;
        }
    }
}

void Scene::moveSourceUp(int sourceId)
{
    std::lock_guard<std::mutex> locker(m_mutex);

    auto& sources = m_videoSources;

    for (size_t i = 0; i < sources.size(); ++i) {
        if (sources[i] && sources[i]->sourceId() == sourceId) {
            if (i == 0) return;  // 顶部了，不能再上移

            // 交换优先级
            int pri1 = sources[i]->priority();
            int pri2 = sources[i - 1]->priority();
            sources[i]->setPriority(pri2);
            sources[i - 1]->setPriority(pri1);

            sortSourcesByPriority();
            break;
        }
    }
}

void Scene::moveSourceDown(int sourceId)
{
    std::lock_guard<std::mutex> locker(m_mutex);

    auto& sources = m_videoSources;

    for (size_t i = 0; i < sources.size(); ++i) {
        if (sources[i] && sources[i]->sourceId() == sourceId) {
            if (i >= sources.size() - 1) return;  // 底部了，不能再下移

            // 交换优先级
            int pri1 = sources[i]->priority();
            int pri2 = sources[i + 1]->priority();
            sources[i]->setPriority(pri2);
            sources[i + 1]->setPriority(pri1);

            sortSourcesByPriority();
            break;
        }
    }
}

std::vector<VideoSource *> Scene::videoSources()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    return m_videoSources;
}

std::vector<AudioSource *> Scene::audioSources()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    return m_audioSources;
}

std::set<int> Scene::videoSourceIds()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    return m_videoSourceIds;
}

std::set<int> Scene::audioSourceIds()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    return m_audioSourceIds;
}

void Scene::clearVideoSource()
{
    if(m_videoSources.empty()) return;

    for(VideoSource* src : m_videoSources)
    {
        if(src)
        {
            delete src;
        }
    }

    m_videoSources.clear();
    m_videoSourceIds.clear();
}

void Scene::clearAudioSource()
{
    if(m_audioSources.empty()) return;

    for(AudioSource* src : m_audioSources)
    {
        if(src)
        {
            delete src;
        }
    }

    m_audioSources.clear();
    m_audioSourceIds.clear();
}

void Scene::clear()
{
    clearVideoSource();
    clearAudioSource();
}


void Scene::setPriority(int prio)
{
    m_priority = prio;
}

void Scene::setActive(bool active)
{
    m_isActived.store(active);
}

bool Scene::isActive() const
{
    return m_isActived.load();
}

int Scene::allocteSourceId()
{
    return m_nextSourceId++;
}

void Scene::addLayer(FrameLayer *layer)
{
    if (!layer) return;
    std::lock_guard<std::mutex> locker(m_layerMutex);
    m_sceneLayers.push_back(layer);
    sortLayersByPriority();
}

FrameLayer *Scene::findLayerById(int sourceId)
{
    std::lock_guard<std::mutex> locker(m_layerMutex);
    for (auto layer : m_sceneLayers) {
        if (layer && layer->frameInfo.sourceId == sourceId) {
            return layer;
        }
    }
    return nullptr;
}

void Scene::removeLayer(int sourceId)
{
    std::lock_guard<std::mutex> locker(m_layerMutex);
    for (auto it = m_sceneLayers.begin(); it != m_sceneLayers.end(); ++it) {
        if (*it && (*it)->frameInfo.sourceId == sourceId) {
            (*it)->isActive = false;
            (*it)->isVisible = false;
            if ((*it)->interopHelper) {
                (*it)->interopHelper->waitForOperationsToComplete();
                (*it)->interopHelper->release(); // 释放内部资源
                delete (*it)->interopHelper;
                (*it)->interopHelper = nullptr;
            }
            delete *it;
            m_sceneLayers.erase(it);
            break;
        }
    }
}

std::vector<FrameLayer *> Scene::sceneLayers()
{
    std::lock_guard<std::mutex> locker(m_layerMutex);
    return m_sceneLayers;
}

void Scene::releaseLayers()
{
    std::lock_guard<std::mutex> locker(m_layerMutex);
    for (auto layer : m_sceneLayers) {
        if (layer) {
            if (layer->interopHelper) {
                layer->interopHelper->waitForOperationsToComplete();
                layer->interopHelper->release(); // 释放内部资源
                delete layer->interopHelper;
                layer->interopHelper = nullptr;
            }
            delete layer;
        }
    }
    m_sceneLayers.clear();
}

void Scene::sortSourcesByPriority()
{
    std::sort(m_videoSources.begin(), m_videoSources.end(),
              [](VideoSource* a, VideoSource* b) {
                  return a->priority() < b->priority();
              });
    sortLayersByPriority();
}

void Scene::sortLayersByPriority()
{
    std::sort(m_sceneLayers.begin(), m_sceneLayers.end(),
              [](const FrameLayer* a, const FrameLayer* b) {
                  return a->frameInfo.priority > b->frameInfo.priority;
              });
}
