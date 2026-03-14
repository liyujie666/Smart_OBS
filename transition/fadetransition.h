#ifndef FADETRANSITION_H
#define FADETRANSITION_H

#include "transitionbase.h"

class FadeTransition : public TransitionBase
{
public:
    explicit FadeTransition(QObject *parent = nullptr);
    void render(CudaRenderWidget* renderWidget,const QSize& size,RenderTarget target) override;
};

#endif // FADETRANSITION_H
