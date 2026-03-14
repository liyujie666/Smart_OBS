#ifndef SCENE_H
#define SCENE_H
#include <QObject>
#include <mutex>
#include <vector>
#include <set>
class FrameLayer;
class VideoSource;
class AudioSource;
class Scene
{
public:
    Scene(int id,const QString& name);
    ~Scene();

    int id() const;
    QString name();
    int priority() const;

    void addVideoSource(VideoSource* videoSource);
    void addAudioSource(AudioSource* audioSource);

    VideoSource* findVideoSourceById(int sourceId);
    AudioSource* findAudioSourceById(int sourceId);
    void removeVideoSource(int sourceId);
    void removeAudioSource(int sourceId);

    void moveSourceUp(int sourceId);
    void moveSourceDown(int sourceId);
    std::vector<VideoSource*> videoSources();
    std::vector<AudioSource*> audioSources();
    std::set<int> videoSourceIds();
    std::set<int> audioSourceIds();

    void clearVideoSource();
    void clearAudioSource();
    void clear();
    void setPriority(int prio);
    void setActive(bool active);
    bool isActive() const;
    int allocteSourceId();

    void addLayer(FrameLayer* layer);
    FrameLayer* findLayerById(int sourceId);
    void removeLayer(int sourceId);
    std::vector<FrameLayer*> sceneLayers();
    void releaseLayers();
    void sortLayersByPriority();
    std::mutex m_layerMutex;
private:
    void sortSourcesByPriority();


private:
    int m_id;
    int m_priority;
    int m_nextSourceId = 0;
    QString m_name;
    std::atomic<bool> m_isActived{true};

    std::vector<VideoSource*> m_videoSources;
    std::vector<AudioSource*> m_audioSources;
    std::vector<FrameLayer*> m_sceneLayers;
    std::set<int> m_videoSourceIds;
    std::set<int> m_audioSourceIds;
    std::mutex m_mutex;

};

#endif // SCENE_H
