// #include "scenemanager.h"
// #include "scene/scene.h"
// #include "transition/transitionmanager.h"
// #include <QDebug>
// SceneManager::SceneManager(QObject* parent)
//     : QObject(parent), m_currentScene(nullptr)
// {
// }

// SceneManager::~SceneManager()
// {
//     release();
// }

// void SceneManager::addScene(Scene *scene)
// {
//     if(!scene) return;
//     std::lock_guard<std::mutex> locker(m_mutex);
//     m_scenes.push_back(scene);
// }

// void SceneManager::removeSceneById(int sceneId)
// {
//     std::lock_guard<std::mutex> locker(m_mutex);
//     for (auto it = m_scenes.begin(); it != m_scenes.end(); ++it)
//     {
//         if (*it && (*it)->id() == sceneId)
//         {
//             delete *it;
//             m_scenes.erase(it);
//             break;
//         }
//     }
// }

// Scene *SceneManager::findSceneById(int sceneId)
// {
//     for(const auto& scene : m_scenes)
//     {
//         if(scene->id() == sceneId)
//         {
//             return scene;
//         }
//     }

//     return nullptr;
// }

// // void SceneManager::switchScene(int sceneId)
// // {
// //     std::lock_guard<std::mutex> locker(m_mutex);

// //     auto newScene = findSceneById(sceneId);
// //     if(!newScene) return;

// //     // // 停止源渲染
// //     Scene* oldScene = m_currentScene;
// //     emit sceneSwitchBefore(oldScene,newScene);

// //     m_currentScene = newScene;
// //     emit sceneIdUpdate(sceneId);
// //     // 新场景渲染
// //     emit sceneSwitched(newScene);

// // }

// void SceneManager::switchScene(int sceneId)
// {
//     std::lock_guard<std::mutex> locker(m_mutex);
//     auto newScene = findSceneById(sceneId);
//     if (!newScene || newScene == m_currentScene) return;

//     // 1. 获取转场管理器实例
//     TransitionManager* transitionMgr = TransitionManager::getInstance();
//     if (transitionMgr->isTransitioning()) {
//         transitionMgr->stop();  // 中断当前转场（如果有）
//     }

//     Scene* oldScene = m_currentScene;
//     if (!oldScene) {
//         // 无旧场景，直接切换（不触发转场）
//         m_currentScene = newScene;
//         emit sceneIdUpdate(sceneId);
//         emit sceneSwitched(newScene);
//         return;
//     }

//     // 3. 连接转场结束信号：转场结束后更新当前场景，清理旧场景
//     connect(transitionMgr, &TransitionManager::transitionFinished, this, [=]() {
//         std::lock_guard<std::mutex> locker(m_mutex);
//         m_currentScene = newScene;
//         emit sceneIdUpdate(sceneId);
//         emit sceneSwitchBefore(oldScene,newScene);
//         emit sceneSwitched(newScene);
//         disconnect(transitionMgr, &TransitionManager::transitionFinished, this, nullptr);
//     });

//     // 4. 开始转场
//     transitionMgr->start(oldScene, newScene);
// }

// Scene *SceneManager::currentScene()
// {
//     return m_currentScene;
// }

// std::vector<Scene *> SceneManager::getAllScenes()
// {
//     return m_scenes;
// }

// void SceneManager::moveSceneUp(int sceneId)
// {
//     std::lock_guard<std::mutex> locker(m_mutex);
//     auto scene = findSceneById(sceneId);
//     if(scene)
//     {
//         scene->setPriority(scene->priority() - 1);
//     }

// }

// void SceneManager::moveSceneDown(int sceneId)
// {
//     std::lock_guard<std::mutex> locker(m_mutex);
//     auto scene = findSceneById(sceneId);
//     if(scene)
//     {
//         scene->setPriority(scene->priority() + 1);
//     }
// }

// void SceneManager::release()
// {
//     std::lock_guard<std::mutex> locker(m_mutex);
//     for (auto& scene : m_scenes) {
//         if (scene) {
//             delete scene;
//         }
//     }
//     m_scenes.clear();
//     m_currentScene = nullptr;
// }


// void SceneManager::sortScenesByPriority()
// {
//     std::sort(m_scenes.begin(),m_scenes.end(),[](const Scene* a,const Scene* b){
//         return a->priority() < b->priority();
//     });
// }
#include "scenemanager.h"
#include "scene/scene.h"
#include "transition/transitionmanager.h"
#include <QDebug>
#include <algorithm>

SceneManager::SceneManager(QObject* parent)
    : QObject(parent)
{
}

SceneManager::~SceneManager()
{
    release();
}

// 添加场景（接收shared_ptr，自动管理生命周期）
void SceneManager::addScene(std::shared_ptr<Scene> scene)
{
    if (!scene) return;
    std::lock_guard<std::mutex> locker(m_mutex);
    m_scenes.push_back(scene);
}

// 移除场景（只需从容器中删除，shared_ptr自动释放内存）
void SceneManager::removeSceneById(int sceneId)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    // 查找并移除对应的场景
    auto it = std::remove_if(m_scenes.begin(), m_scenes.end(),
                             [sceneId](const std::shared_ptr<Scene>& scene) {
                                 return scene && scene->id() == sceneId;
                             });

    if (it != m_scenes.end()) {
        m_scenes.erase(it, m_scenes.end());
        qDebug() << "场景" << sceneId << "已移除";
    }
}

// 查找场景（返回shared_ptr，确保访问时对象有效）
std::shared_ptr<Scene> SceneManager::findSceneById(int sceneId)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    for (const auto& scene : m_scenes) {
        if (scene && scene->id() == sceneId) {
            return scene;
        }
    }
    return nullptr;
}

void SceneManager::switchScene(int sceneId)
{
    std::shared_ptr<Scene> newScene = findSceneById(sceneId);
    if (!newScene || (m_currentScene.lock() == newScene)) return;

    std::lock_guard<std::mutex> locker(m_mutex);
    // 获取转场管理器实例
    TransitionManager* transitionMgr = TransitionManager::getInstance();
    if (transitionMgr->isTransitioning()) {
        transitionMgr->stop();  // 中断当前转场
    }

    std::shared_ptr<Scene> oldScene = m_currentScene.lock();  // 从weak_ptr获取shared_ptr
    if (!oldScene) {
        // 无旧场景，直接切换
        m_currentScene = newScene;  // weak_ptr自动接收shared_ptr
        emit sceneIdUpdate(sceneId);
        emit sceneSwitched(newScene);
        return;
    }

    // 连接转场结束信号：使用shared_ptr的拷贝确保转场期间对象有效
    connect(transitionMgr, &TransitionManager::transitionFinished, this, [=, newSceneCopy = newScene, oldSceneCopy = oldScene]() {
        std::lock_guard<std::mutex> locker(m_mutex);
        m_currentScene = newSceneCopy;
        emit sceneIdUpdate(sceneId);
        emit sceneSwitchBefore(oldSceneCopy, newSceneCopy);
        emit sceneSwitched(newSceneCopy);
        disconnect(transitionMgr, &TransitionManager::transitionFinished, this, nullptr);
    });

    // 开始转场（传递shared_ptr的拷贝）
    transitionMgr->start(oldScene, newScene);
}

// 获取当前场景（返回shared_ptr，确保访问时对象有效）
std::shared_ptr<Scene> SceneManager::currentScene()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    return m_currentScene.lock();  // 若对象已销毁，返回nullptr
}

// 获取所有场景
std::vector<std::shared_ptr<Scene>> SceneManager::getAllScenes()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    return m_scenes;
}

void SceneManager::moveSceneUp(int sceneId)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    auto scene = findSceneById(sceneId);
    if (scene) {
        scene->setPriority(scene->priority() - 1);
        sortScenesByPriority();
    }
}

void SceneManager::moveSceneDown(int sceneId)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    auto scene = findSceneById(sceneId);
    if (scene) {
        scene->setPriority(scene->priority() + 1);
        sortScenesByPriority();
    }
}

// 释放所有场景（容器清空后，shared_ptr自动释放内存）
void SceneManager::release()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    m_scenes.clear();
    m_currentScene.reset();  // 重置weak_ptr
}

void SceneManager::sortScenesByPriority()
{
    std::sort(m_scenes.begin(), m_scenes.end(),
              [](const std::shared_ptr<Scene>& a, const std::shared_ptr<Scene>& b) {
                  return a->priority() < b->priority();
              });
}
