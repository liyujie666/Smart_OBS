#ifndef TRANSITIONBASE_H
#define TRANSITIONBASE_H

#include <QObject>
#include "scene/scene.h"
#include "render/cudarenderwidget.h"

enum RenderTarget {
    Window,
    FBO
};

class TransitionBase : public QObject
{
    Q_OBJECT
public:
    explicit TransitionBase(QObject *parent = nullptr): QObject(parent){};
    virtual ~TransitionBase() = default;

    virtual void init(std::shared_ptr<Scene> oldScene, std::shared_ptr<Scene> newScene, int durationMs){
        m_oldScene = oldScene;
        m_newScene = newScene;
        m_durationMs = durationMs;
        m_progress = 0.0;
        m_isRunning = true;
    }

    virtual void render(CudaRenderWidget* renderWidget,const QSize& widgetSize,RenderTarget target) = 0;

    virtual void updateProgress(float progress){
        if(progress < 0.0) m_progress = 0.0;
        if(progress > 1.0) m_progress = 1.0;
        m_progress = progress;

        if(m_progress >= 1.0)
        {
            m_isRunning = false;
            emit transitionFinished();
        }
    }

    virtual void finish(){
        m_isRunning = false;
        m_oldScene = nullptr;
        m_newScene = nullptr;
    }

    bool isRunning() const { return m_isRunning; }
    float progress() const { return m_progress; }
    int durationMs() const { return m_durationMs; }

signals:
    void transitionFinished();
protected:
    std::shared_ptr<Scene> m_oldScene;
    std::shared_ptr<Scene> m_newScene;
    int m_durationMs = 200;
    float m_progress = 0.0;
    bool m_isRunning = false;

signals:
};

#endif // TRANSITIONBASE_H
