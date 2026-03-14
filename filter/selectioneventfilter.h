// selectioneventfilter.h
#pragma once

#include <QObject>
#include <QWidget>
#include <QEvent>
#include <QMouseEvent>

class SelectionEventFilter : public QObject
{
    Q_OBJECT

public:
    SelectionEventFilter(QObject* parent = nullptr);

    void setSelectedStyle(const QString& style);
    void setNormalStyle(const QString& style);
    void setHoverStyle(const QString& style);
    void setSelected(QWidget* widget);
    QWidget* selectedWidget();
    void clearSelection();
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void widgetSelected(QWidget* selectedWidget);
    void sourceItemSelected(int sourceId);

private:
    QWidget* selectedWidget_ = nullptr;
    QWidget* hoveredWidget_ = nullptr;
    QString selectedStyle_;
    QString hoverStyle_;
    QString normalStyle_;
};
