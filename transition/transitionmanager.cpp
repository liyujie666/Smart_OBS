#include "transitionmanager.h"
#include "transition/fadetransition.h"
#include "transition/slidetransition.h"
#include "transition/directtransition.h"
#include <QDateTime>
TransitionManager::TransitionManager(QObject *parent)
    : QObject{parent}
{
    m_transitionTimer = new QTimer(this);
    m_transitionTimer->setInterval(16);
    m_transitionTimer->setSingleShot(false);
    connect(m_transitionTimer, &QTimer::timeout, this, &TransitionManager::onTimerTimeout);

    // 注册默认转场（
    registerTransition(0, "淡入淡出", new FadeTransition(this));
    registerTransition(1, "左右滑动", new SlideTransition(this));
    registerTransition(2, "直接切换", new DirectTransition(this));
}

TransitionManager *TransitionManager::getInstance()
{
    static TransitionManager instance;
    return &instance;
}

void TransitionManager::registerTransition(int transitionId, const QString &transitionName, TransitionBase *transition)
{
    if(!transition || m_transitionMap.contains(transitionId)) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_transitionMap[transitionId] = transition;
    m_transitionNameMap[transitionId] = transitionName;
}

void TransitionManager::unregisterAllTransitions()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    qDeleteAll(m_transitionMap.values());
    m_transitionMap.clear();
    m_transitionNameMap.clear();
}

void TransitionManager::setCurrentTransition(int transitionId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_transitionMap.contains(transitionId)) {
        m_currentTransitionId = transitionId;
        m_currentTransition = m_transitionMap[transitionId];
    }
}

void TransitionManager::setTransitionDuration(int durationMs)
{
    if (durationMs < 100) durationMs = 100;
    m_currentDurationMs = durationMs;
}

void TransitionManager::start(std::shared_ptr<Scene> oldScene, std::shared_ptr<Scene> newScene)
{
    if(!oldScene || !newScene || !m_currentTransition || m_isTransitioning.load()) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    m_oldScene = oldScene;
    m_newScene = newScene;

    m_currentTransition->init(oldScene,newScene,m_currentDurationMs);

    connect(m_currentTransition,&TransitionBase::transitionFinished,this,&TransitionManager::stop);

    m_startTime = QDateTime::currentMSecsSinceEpoch();
    m_transitionTimer->start();
    m_isTransitioning.store(true);

    // 4. 激活新场景图层（按你的需求：转场开始即激活新场景）
    // for (int sourceId : newScene->videoSourceIds()) {
    //     FrameLayer* layer = newScene->findLayerById(sourceId);
    //     if (layer) layer->setActive(true);
    // }

    emit transitionProgressUpdated();  // 触发首次渲染
}

void TransitionManager::stop()
{
    if (!m_isTransitioning.load() || !m_currentTransition) return;
    std::lock_guard<std::mutex> lock(m_mutex);

    // 1. 停止计时器，断开信号
    m_transitionTimer->stop();
    disconnect(m_currentTransition, &TransitionBase::transitionFinished, this, &TransitionManager::stop);
    // 2. 结束当前转场，清理资源
    m_currentTransition->finish();
    m_isTransitioning.store(false);

    m_oldScene.reset();
    m_newScene.reset();

    emit transitionFinished();  // 通知SceneManager清理旧场景
}

void TransitionManager::onTimerTimeout()
{
    if (!m_currentTransition || !m_isTransitioning.load()) return;

    // 1. 计算当前进度（已耗时 / 总时长）
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    float elapsedMs = currentTime - m_startTime;
    float progress = elapsedMs / m_currentDurationMs;
    // 2. 更新转场进度
    m_currentTransition->updateProgress(progress);
    // 3. 触发渲染刷新
    emit transitionProgressUpdated();
}
