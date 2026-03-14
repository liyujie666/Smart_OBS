#include "filter/selectioneventfilter.h"

SelectionEventFilter::SelectionEventFilter(QObject* parent)
    : QObject(parent)
{
}

void SelectionEventFilter::setSelectedStyle(const QString& style)
{
    selectedStyle_ = style;
}

void SelectionEventFilter::setNormalStyle(const QString& style)
{
    normalStyle_ = style;
}

void SelectionEventFilter::setHoverStyle(const QString &style)
{
    hoverStyle_ = style;
}

QWidget *SelectionEventFilter::selectedWidget()
{
    return selectedWidget_;
}

void SelectionEventFilter::clearSelection()
{
    selectedWidget_ = nullptr;
}

bool SelectionEventFilter::eventFilter(QObject* watched, QEvent* event)
{
    QWidget* widget = qobject_cast<QWidget*>(watched);
    if (!widget) return QObject::eventFilter(watched, event);

    switch (event->type()) {
    case QEvent::MouseButtonPress:{
        if (selectedWidget_ && selectedWidget_ != widget) {
            selectedWidget_->setStyleSheet(normalStyle_);
        }
        widget->setStyleSheet(selectedStyle_);
        selectedWidget_ = widget;
        int sourceId = widget->property("sourceId").toInt();
        emit widgetSelected(widget);
        emit sourceItemSelected(sourceId);
        return false;
    }
    case QEvent::Enter:
        if (hoveredWidget_ && hoveredWidget_ != widget && hoveredWidget_ != selectedWidget_) {
            // 上一个悬浮控件恢复样式
            if (hoveredWidget_ == selectedWidget_)
                hoveredWidget_->setStyleSheet(selectedStyle_);
            else
                hoveredWidget_->setStyleSheet(normalStyle_);
        }
        // 当前悬浮控件设置悬浮样式（如果不是选中）
        if (widget != selectedWidget_) {
            widget->setStyleSheet(hoverStyle_);
        }
        hoveredWidget_ = widget;
        return false;

    case QEvent::Leave:
        if (hoveredWidget_) {
            if (hoveredWidget_ != selectedWidget_)
                hoveredWidget_->setStyleSheet(normalStyle_);
            else
                hoveredWidget_->setStyleSheet(selectedStyle_);
            hoveredWidget_ = nullptr;
        }
        return false;

    default:
        break;
    }

    return QObject::eventFilter(watched, event);
}

void SelectionEventFilter::setSelected(QWidget* widget)
{
    if (!widget) return;

    if (selectedWidget_ && selectedWidget_ != widget)
        selectedWidget_->setStyleSheet(normalStyle_);

    selectedWidget_ = widget;
    selectedWidget_->setStyleSheet(selectedStyle_);


}
