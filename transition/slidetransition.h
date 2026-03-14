#ifndef SLIDETRANSITION_H
#define SLIDETRANSITION_H

#include "transitionbase.h"

class SlideTransition : public TransitionBase
{
    Q_OBJECT
public:
    enum Direction {
        Left,   // 向左滑动（新场景从右侧进入）
        Right,  // 向右滑动（新场景从左侧进入）
        Up,     // 向上滑动（新场景从下方进入）
        Down    // 向下滑动（新场景从上方进入）
    };

    explicit SlideTransition(QObject *parent = nullptr);
    void setDirection(Direction direction);
    void render(CudaRenderWidget *renderWidget, const QSize &widgetSize,RenderTarget target) override;

private:
    Direction m_direction = Left;  // 默认向左滑动
};
#endif // SLIDETRANSITION_H
