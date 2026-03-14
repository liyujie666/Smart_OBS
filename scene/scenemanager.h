// #ifndef SCENEMANAGER_H
// #define SCENEMANAGER_H
// #include <QObject>
// #include <QString>
// #include <map>
// #include <vector>
// #include <mutex>
// class Scene;
// class VideoSource;

// class SceneManager : public QObject
// {
//     Q_OBJECT

// public:
//     SceneManager(QObject* parent = nullptr);
//     ~SceneManager();

//     void addScene(Scene* scene);
//     void removeSceneById(int sceneId);

//     Scene* findSceneById(int sceneId);
//     void switchScene(int sceneId);
//     Scene* currentScene();
//     std::vector<Scene*> getAllScenes();


//     void moveSceneUp(int sceneId);
//     void moveSceneDown(int sceneId);
//     void release();

// signals:
//     void sceneSwitchBefore(Scene* oldScene,Scene* newScene);
//     void sceneSwitched(Scene* newScene);
//     void sceneIdUpdate(int sceneId);

// private:
//     void sortScenesByPriority();


// private:

//     std::vector<Scene*> m_scenes;
//     Scene* m_currentScene;
//     std::mutex m_mutex;
// };

// #endif // SCENEMANAGER_H
#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H
#include <QObject>
#include <QString>
#include <vector>
#include <mutex>
#include <memory>  // 引入智能指针头文件

class Scene;
class VideoSource;

class SceneManager : public QObject
{
    Q_OBJECT

public:
    SceneManager(QObject* parent = nullptr);
    ~SceneManager() override;

    // 添加场景（接收shared_ptr）
    void addScene(std::shared_ptr<Scene> scene);
    // 移除场景（按ID）
    void removeSceneById(int sceneId);

    // 查找场景（返回shared_ptr）
    std::shared_ptr<Scene> findSceneById(int sceneId);
    // 切换场景
    void switchScene(int sceneId);
    // 获取当前场景（返回shared_ptr）
    std::shared_ptr<Scene> currentScene();
    // 获取所有场景（返回shared_ptr的vector）
    std::vector<std::shared_ptr<Scene>> getAllScenes();

    void moveSceneUp(int sceneId);
    void moveSceneDown(int sceneId);
    void release();

signals:
    void sceneSwitchBefore(std::shared_ptr<Scene> oldScene, std::shared_ptr<Scene> newScene);
    void sceneSwitched(std::shared_ptr<Scene> newScene);
    void sceneIdUpdate(int sceneId);

private:
    void sortScenesByPriority();

private:
    // 存储场景的智能指针容器
    std::vector<std::shared_ptr<Scene>> m_scenes;
    // 当前场景使用weak_ptr，避免循环引用
    std::weak_ptr<Scene> m_currentScene;
    std::mutex m_mutex;
};

#endif // SCENEMANAGER_H
