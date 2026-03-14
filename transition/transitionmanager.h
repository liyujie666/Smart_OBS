#ifndef TRANSITIONMANAGER_H
#define TRANSITIONMANAGER_H

#include <QObject>
#include "transitionbase.h"

class TransitionManager : public QObject
{
    Q_OBJECT
public:
    static TransitionManager* getInstance();
    ~TransitionManager() = default;

    void registerTransition(int transitionId,const QString& transitionName,TransitionBase* transition);
    void unregisterAllTransitions();

    void setCurrentTransition(int transitionId);
    void setTransitionDuration(int durationMs);

    void start(std::shared_ptr<Scene> oldScene, std::shared_ptr<Scene> newScene);
    void stop();

    bool isTransitioning() const { return m_isTransitioning.load(); }
    TransitionBase* getCurrentTransition() const { return m_currentTransition; }
    QMap<int, QString> getTransitionList() const { return m_transitionNameMap; }
    std::shared_ptr<Scene> oldScene() const { return m_oldScene.lock(); }
    std::shared_ptr<Scene> newScene() const { return m_newScene.lock(); }

signals:
    void transitionProgressUpdated();  // 进度更新
    void transitionFinished();         // 转场结束

private slots:
    void onTimerTimeout();

private:
    TransitionManager(QObject *parent = nullptr);
    TransitionManager(const TransitionManager&) = delete;  // 禁止拷贝
    TransitionManager& operator=(const TransitionManager&) = delete;

private:
    QMap<int, TransitionBase*> m_transitionMap;    // 转场实例注册表（ID→实例）
    QMap<int, QString> m_transitionNameMap;        // 转场名称表（ID→名称，用于UI）
    int m_currentTransitionId = -1;                // 当前选中的转场ID
    TransitionBase* m_currentTransition = nullptr; // 当前转场实例
    int m_currentDurationMs = 500;                 // 当前转场时长（默认500ms）
    std::weak_ptr<Scene> m_oldScene;
    std::weak_ptr<Scene> m_newScene;

    QTimer* m_transitionTimer = nullptr;           // 转场计时器（驱动进度更新）
    qint64 m_startTime = 0;                        // 转场开始时间（ms）
    std::atomic<bool> m_isTransitioning = false;                // 是否正在转场
    mutable std::mutex m_mutex;                    // 线程安全锁
};

#endif // TRANSITIONMANAGER_H
