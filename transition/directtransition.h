#ifndef DIRECTTRANSITION_H
#define DIRECTTRANSITION_H

#include "transitionbase.h"

class DirectTransition : public TransitionBase
{
public:
    explicit DirectTransition(QObject *parent = nullptr);
    void render(CudaRenderWidget* renderWidget, const QSize& widgetSize, RenderTarget target) override;
    void updateProgress(float progress) override;
};

#endif // DIRECTTRANSITION_H
