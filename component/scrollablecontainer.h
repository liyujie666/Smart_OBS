#ifndef SCROLLABLECONTAINER_H
#define SCROLLABLECONTAINER_H

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>

class ScrollableContainer : public QWidget
{
    Q_OBJECT
public:
    // 构造函数：可指定滚动条策略和布局对齐方式
    explicit ScrollableContainer(
        Qt::ScrollBarPolicy vScrollPolicy = Qt::ScrollBarAsNeeded,
        Qt::ScrollBarPolicy hScrollPolicy = Qt::ScrollBarAlwaysOff,
        Qt::Alignment alignment = Qt::AlignTop,
        QWidget *parent = nullptr);

    // 添加子部件到滚动区域（默认插入到伸缩项前）
    void addWidget(QWidget *widget, int stretch = 0, Qt::Alignment alignment = Qt::Alignment());

    // 移除子部件
    void removeWidget(QWidget *widget);

    // 获取内部布局（便于高级定制）
    QVBoxLayout* contentLayout() const;

    QList<QWidget*> allWidgets() const { return m_widgets; }

    // 获取滚动区域（便于设置额外属性）
    QScrollArea* scrollArea() const;
    void applyOBStyle();

private:
    QScrollArea *m_scrollArea;      // 滚动区域
    QWidget *m_contentWidget;       // 内容容器
    QVBoxLayout *m_contentLayout;   // 内容布局
    QList<QWidget*> m_widgets;
};

#endif // SCROLLABLECONTAINER_H
