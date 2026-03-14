#ifndef VIDEOFRAME_H
#define VIDEOFRAME_H

#include <QImage>
#include <QSize>

// 视频帧数据结构
struct VideoFrame
{
    QImage image;       // 图像数据
    int priority = 0;   // 显示优先级
    qreal opacity = 1.0;// 不透明度 (0.0-1.0)
    QSize size;         // 图像尺寸

    // 构造函数
    VideoFrame(const QImage& img = QImage(),
               int prio = 0,
               qreal op = 1.0)
        : image(img),
        priority(prio),
        opacity(op),
        size(img.size()) {}

    // 检查帧是否有效
    bool isValid() const { return !image.isNull(); }

    // 设置尺寸（自动缩放图像）
    void setSize(const QSize& newSize) {
        if (size != newSize && !image.isNull()) {
            image = image.scaled(newSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            size = newSize;
        }
    }
};

#endif // VIDEOFRAME_H
