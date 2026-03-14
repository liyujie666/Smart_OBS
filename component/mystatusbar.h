#ifndef MYSTATUSBAR_H
#define MYSTATUSBAR_H

#include <QStatusBar>
#include <QLabel>
#include <QHBoxLayout>
#include <QWidget>
#include "statusbarmanager.h"

class MyStatusBar : public QStatusBar
{
    Q_OBJECT
public:
    explicit MyStatusBar(QWidget *parent = nullptr);
    ~MyStatusBar() override;

    // 初始化状态栏样式
    void initStyle();

public slots:
    // 处理消息更新
    void onMessageUpdated(const QString& text, MessageType type, bool show);

    // 处理系统信息更新
    void onStatusInfoUpdated(const QString& recordStatus,const QString& streamStatus);

    void onCPUInfoUpdated(double cpuUsage);
    void onFPSInfoUpdated(double frameRate);

private:
    // 状态栏各组成部分
    QLabel* m_recordLabel;      // 录制状态
    QLabel* m_streamLabel;      // 推流状态
    QLabel* m_cpuLabel;         // CPU占用率
    QLabel* m_fpsLabel;         // 帧率
    QLabel* m_messageLabel;     // 消息显示区域

    // 分隔线部件
    QFrame* createSeparator();

    // 根据消息类型设置样式
    void applyMessageStyle(QLabel* label, MessageType type);

    // 创建状态标签
    QPixmap createIconLabel(const QPixmap& icon,const QString& text);
};

#endif // MYSTATUSBAR_H
