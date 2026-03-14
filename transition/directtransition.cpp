#include "directtransition.h"

DirectTransition::DirectTransition(QObject *parent)
    : TransitionBase{parent}
{}

void DirectTransition::render(CudaRenderWidget *renderWidget, const QSize &widgetSize, RenderTarget target)
{
    if(!m_newScene || !renderWidget) return;
    renderWidget->renderSceneLayers(m_newScene.get(), widgetSize);

}

void DirectTransition::updateProgress(float progress)
{
    m_progress = 1.0f;
    m_isRunning = false;
    emit transitionFinished();
}
