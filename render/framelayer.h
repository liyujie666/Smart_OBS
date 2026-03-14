#ifndef FRAMELAYER_H
#define FRAMELAYER_H
#include "infotools.h"
#include <QOpenGLWidget>
#include "cudainterophelper.h"
#include <QMouseEvent>
#include <QVector>

// 帧图层类，管理每个输入源的属性
class FrameLayer {
public:
    CudaFrameInfo frameInfo;
    CudaInteropHelper* interopHelper;
    QRectF rect;           // 显示区域
    QRectF rectRatio;
    bool isSelected;       // 是否被选中
    bool isDragging;       // 是否正在拖拽
    bool isResizing;       // 是否正在缩放
    QPointF dragOffset;    // 拖拽偏移量
    int resizeHandle;      // 缩放手柄位置

    bool isLocked;         // 锁定：不能选中/拖动/缩放
    bool isVisible;        // 隐藏：不渲染/不可命中
    bool isActive;         // 关闭渲染
    float logicalAspectRatio;
    float selfAspectRatio = 16.0f / 9.0f; // 自身宽高比（默认横屏）
    int selfRotateType = 0; // 自身旋转类型（0=正常，1=90度，2=270度）
    QRectF savedRect;
    bool hasSavedRect;
    int64_t timestamp;

    FrameLayer()
        : interopHelper(nullptr), isSelected(false), isDragging(false),
        isResizing(false), resizeHandle(0), isLocked(false), isVisible(true),
        isActive(true), logicalAspectRatio(16.0f / 9.0f), timestamp(0),
        savedRect(QRectF()), hasSavedRect(false)
    {}

    ~FrameLayer() {

    }

    void setVisible(bool visible) { isVisible = visible; }

    void setLocked(bool locked) { isLocked = locked; }

    void setActive(bool active) { isActive = active; }

    // 检查点是否在图层内
    bool contains(const QPointF& point) const {
        return rect.contains(point);
    }

    // 根据 widget 尺寸更新 rect
    void updateAbsoluteRect(int widgetWidth, int widgetHeight) {
        // 计算原始比例对应的像素尺寸
        float x = rectRatio.x() * widgetWidth;
        float y = rectRatio.y() * widgetHeight;
        float w = rectRatio.width() * widgetWidth;
        float h = w / selfAspectRatio;

        // 强制宽高比为逻辑尺寸比例（关键修复）

        rect = QRectF(x, y, w, h);
    }

    // 拖动或缩放后更新比例坐标
    void updateRatioFromAbsolute(int widgetWidth, int widgetHeight) {
        if (widgetWidth == 0 || widgetHeight == 0) return;

        rectRatio = QRectF(
            rect.x() / widgetWidth,
            rect.y() / widgetHeight,
            rect.width() / widgetWidth,
            (rect.width() / logicalAspectRatio) / widgetHeight
            );
    }

    // 检查点是否在缩放手柄上
    int hitTestResizeHandle(const QPointF& point) const {
        const int HANDLE_SIZE = 10;
        QRectF handles[8] = {
            QRectF(rect.left() - HANDLE_SIZE/2, rect.top() - HANDLE_SIZE/2, HANDLE_SIZE, HANDLE_SIZE),
            QRectF(rect.center().x() - HANDLE_SIZE/2, rect.top() - HANDLE_SIZE/2, HANDLE_SIZE, HANDLE_SIZE),
            QRectF(rect.right() - HANDLE_SIZE/2, rect.top() - HANDLE_SIZE/2, HANDLE_SIZE, HANDLE_SIZE),
            QRectF(rect.right() - HANDLE_SIZE/2, rect.center().y() - HANDLE_SIZE/2, HANDLE_SIZE, HANDLE_SIZE),
            QRectF(rect.right() - HANDLE_SIZE/2, rect.bottom() - HANDLE_SIZE/2, HANDLE_SIZE, HANDLE_SIZE),
            QRectF(rect.center().x() - HANDLE_SIZE/2, rect.bottom() - HANDLE_SIZE/2, HANDLE_SIZE, HANDLE_SIZE),
            QRectF(rect.left() - HANDLE_SIZE/2, rect.bottom() - HANDLE_SIZE/2, HANDLE_SIZE, HANDLE_SIZE),
            QRectF(rect.left() - HANDLE_SIZE/2, rect.center().y() - HANDLE_SIZE/2, HANDLE_SIZE, HANDLE_SIZE)
        };

        for (int i = 0; i < 8; i++) {
            if (handles[i].contains(point))
                return i + 1;
        }

        return 0;
    }

    // 根据缩放手柄调整大小
    void resizeByHandle(int handle, const QPointF& delta) {
        QRectF newRect = rect;

        // 原始缩放逻辑（保持不变）
        switch(handle) {
        case 1: newRect.setLeft(newRect.left() + delta.x()); newRect.setTop(newRect.top() + delta.y()); break;
        case 2: newRect.setTop(newRect.top() + delta.y()); break;
        case 3: newRect.setRight(newRect.right() + delta.x()); newRect.setTop(newRect.top() + delta.y()); break;
        case 4: newRect.setRight(newRect.right() + delta.x()); break;
        case 5: newRect.setRight(newRect.right() + delta.x()); newRect.setBottom(newRect.bottom() + delta.y()); break;
        case 6: newRect.setBottom(newRect.bottom() + delta.y()); break;
        case 7: newRect.setLeft(newRect.left() + delta.x()); newRect.setBottom(newRect.bottom() + delta.y()); break;
        case 8: newRect.setLeft(newRect.left() + delta.x()); break;
        }

        // 强制按逻辑比例修正（关键）
        float targetWidth = newRect.width();
        float targetHeight = targetWidth / selfAspectRatio;

        // 根据缩放手柄调整位置，避免缩放时画面偏移
        if (handle == 1 || handle == 2 || handle == 3) {
            // 顶部手柄缩放：保持底部位置不变
            newRect.setTop(newRect.bottom() - targetHeight);
        }
        if (handle == 1 || handle == 7 || handle == 8) {
            // 左侧手柄缩放：保持右侧位置不变
            newRect.setLeft(newRect.right() - targetWidth);
        }

        // 应用修正后的尺寸
        newRect.setWidth(targetWidth);
        newRect.setHeight(targetHeight);

        // 确保最小尺寸
        if (newRect.width() >= 50 && newRect.height() >= 50)
            rect = newRect;
    }


};
#endif // FRAMELAYER_H
