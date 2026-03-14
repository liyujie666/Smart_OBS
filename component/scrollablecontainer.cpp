#include "scrollablecontainer.h"
#include <QDebug>

ScrollableContainer::ScrollableContainer(
    Qt::ScrollBarPolicy vScrollPolicy,
    Qt::ScrollBarPolicy hScrollPolicy,
    Qt::Alignment alignment,
    QWidget *parent)
    : QWidget(parent)
{
    // 1. 创建主布局（填充整个容器）
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // 2. 创建滚动区域
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true); // 内容自适应滚动区域大小
    m_scrollArea->setVerticalScrollBarPolicy(vScrollPolicy);
    m_scrollArea->setHorizontalScrollBarPolicy(hScrollPolicy);
    mainLayout->addWidget(m_scrollArea);

    // 3. 创建内容容器（滚动区域内部的部件）
    m_contentWidget = new QWidget(m_scrollArea);
    m_scrollArea->setWidget(m_contentWidget); // 绑定到滚动区域

    // 4. 创建内容布局（垂直布局，用于添加子部件）
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setAlignment(alignment); // 默认顶部对齐
    m_contentLayout->setContentsMargins(5, 5, 5, 5); // 轻微内边距，避免内容贴边
    m_contentLayout->setSpacing(5); // 子部件间距

    // 添加伸缩项（确保子部件顶部对齐，不会被拉伸）
    m_contentLayout->addStretch();
    applyOBStyle();
}

// 添加子部件到布局（默认插入到伸缩项前）
void ScrollableContainer::addWidget(QWidget *widget, int stretch, Qt::Alignment alignment)
{
    if (!widget) {
        qDebug() << "ScrollableContainer: 无法添加空部件";
        return;
    }

    // 插入到伸缩项之前（最后一项是stretch）
    int insertIndex = m_contentLayout->count() - 1;
    m_contentLayout->insertWidget(insertIndex, widget, stretch, alignment);
    m_widgets.append(widget);
}

// 从布局中移除子部件
void ScrollableContainer::removeWidget(QWidget *widget)
{
    if (!widget) return;

    // 从布局中移除（不删除部件，由外部管理生命周期）
    m_contentLayout->removeWidget(widget);
    m_widgets.removeOne(widget);
}

// 获取内部布局（供外部定制）
QVBoxLayout* ScrollableContainer::contentLayout() const
{
    return m_contentLayout;
}

// 获取滚动区域（供外部设置属性）
QScrollArea* ScrollableContainer::scrollArea() const
{
    return m_scrollArea;
}
void ScrollableContainer::applyOBStyle()
{
    // 隐藏滚动滑块但保留滚动功能的样式
    const QString scrollBarStyle = R"(
        /* 隐藏滚动区域边框 */
        QScrollArea {
            border: none;
        }

        /* 垂直滚动条整体 - 完全透明 */
        QScrollBar:vertical {
            background: transparent;  /* 透明背景 */
            width: 0px;               /* 宽度为0，视觉上隐藏 */
            margin: 0px;
            border: none;
        }

        /* 垂直滚动条滑块 - 完全透明且不可见 */
        QScrollBar::handle:vertical {
            background: transparent;
            border: none;
        }

        /* 隐藏所有垂直滚动条元素 */
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: transparent;
            height: 0px;
            border: none;
        }

        /* 水平滚动条整体 - 完全透明 */
        QScrollBar:horizontal {
            background: transparent;
            height: 0px;              /* 高度为0，视觉上隐藏 */
            margin: 0px;
            border: none;
        }

        /* 水平滚动条滑块 - 完全透明且不可见 */
        QScrollBar::handle:horizontal {
            background: transparent;
            border: none;
        }

        /* 隐藏所有水平滚动条元素 */
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal,
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background: transparent;
            width: 0px;
            border: none;
        }
    )";

    // 应用样式到滚动区域
    m_scrollArea->setStyleSheet(scrollBarStyle);

    // 内容区域样式
    m_contentWidget->setStyleSheet(R"(
        background-color: #1E1E1E;
    )");
}

