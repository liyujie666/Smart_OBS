#include "mystatusbar.h"
#include <QStyle>
#include <QPalette>
#include <QFont>

MyStatusBar::MyStatusBar(QWidget *parent)
    : QStatusBar(parent),
    m_recordLabel(new QLabel(this)),
    m_streamLabel(new QLabel(this)),
    m_cpuLabel(new QLabel(this)),
    m_fpsLabel(new QLabel(this)),
    m_messageLabel(new QLabel(this))
{
    // 初始化布局和样式
    initStyle();

    // 连接状态管理类的信号
    auto& statusManager = StatusBarManager::getInstance();
    connect(&statusManager, &StatusBarManager::statusUpdated,
            this, &MyStatusBar::onMessageUpdated);
    connect(&statusManager, &StatusBarManager::statusInfoUpdated,
            this, &MyStatusBar::onStatusInfoUpdated);

    connect(&statusManager, &StatusBarManager::cpuInfoUpdated,
            this, &MyStatusBar::onCPUInfoUpdated);
    connect(&statusManager, &StatusBarManager::fpsInfoUpdated,
            this, &MyStatusBar::onFPSInfoUpdated);

    onStatusInfoUpdated(statusManager.getInstance().recordText(),statusManager.getInstance().streamText());
    onCPUInfoUpdated(statusManager.getInstance().cpuUsage());
    onFPSInfoUpdated(statusManager.getInstance().frameRate());
}

MyStatusBar::~MyStatusBar()
{
    // 自动释放子部件
}

void MyStatusBar::initStyle()
{
    // 设置状态栏整体样式

    setStyleSheet(R"(
        QStatusBar {
            background-color: rgb(60, 64, 77);
            color: #ffffff;
            border-top: 1px solid #3a3a3a;
        }
        QStatusBar::item{
            border:0px;

        }
        QStatusBar QFrame {
            background: transparent;
        }
    )");
    setMinimumHeight(26);

    // 配置字体
    QFont statusFont = font();
    statusFont.setPointSize(9);
    setFont(statusFont);

    // 初始化各标签样式
    m_recordLabel->setStyleSheet("color: #747d94; padding: 0 8px;");
    m_streamLabel->setStyleSheet("color: #747d94; padding: 0 8px;");
    m_cpuLabel->setStyleSheet("color: #ffffff; padding: 0 8px;");
    m_fpsLabel->setStyleSheet("color: #ffffff; padding: 0 8px;");
    m_messageLabel->setStyleSheet("color: #f8f8f2; padding: 0 12px;");

    // 设置标签最小宽度，避免内容闪烁
    m_recordLabel->setMinimumWidth(80);
    m_streamLabel->setMinimumWidth(80);
    m_cpuLabel->setMinimumWidth(50);
    m_fpsLabel->setMinimumWidth(50);
    m_messageLabel->setMinimumWidth(150); // 消息区域需要更大的最小宽度

    // 最左侧：消息区域（占据主要空间）
    addWidget(m_messageLabel, 1);

    // 分隔线
    addWidget(createSeparator());

    // 最右侧：依次是状态、CPU、FPS
    addWidget(m_streamLabel);
    addWidget(createSeparator());
    addWidget(m_recordLabel);
    addWidget(createSeparator());
    addWidget(m_cpuLabel);
    addWidget(createSeparator());
    addWidget(m_fpsLabel);
}

QFrame* MyStatusBar::createSeparator()
{
    QFrame* separator = new QFrame(this);
    separator->setFrameShape(QFrame::VLine);   // 竖线形态
    separator->setFrameShadow(QFrame::Plain);  // 无阴影，保持简洁
    separator->setStyleSheet("color: #545a6b;"); // 分割线颜色（关键！用 color 控制线条色）
    separator->setFixedWidth(1);               // 强制1px宽，避免拉伸
    separator->setMinimumHeight(16);           // 与状态栏高度适配
    separator->setMaximumHeight(18);           // 限制最大高度，避免变形
    return separator;
}

void MyStatusBar::applyMessageStyle(QLabel* label, MessageType type)
{
    // 根据消息类型设置不同颜色
    switch (type) {
    case MessageType::Error:
        label->setStyleSheet("color: #f44747; padding: 0 12px; font-weight: bold;");
        break;
    case MessageType::Warning:
        label->setStyleSheet("color: #ffd700; padding: 0 12px;");
        break;
    case MessageType::Success:
        label->setStyleSheet("color: #50fa7b; padding: 0 12px;");
        break;
    case MessageType::Debug:
        label->setStyleSheet("color: #6272a4; padding: 0 12px;");
        break;
    default: // Info
        label->setStyleSheet("color: #f8f8f2; padding: 0 12px;");
    }
}


void MyStatusBar::onMessageUpdated(const QString& text, MessageType type, bool show)
{
    if (show) {
        // 显示临时消息并应用样式
        m_messageLabel->setText(text);
        applyMessageStyle(m_messageLabel, type);
    } else {
        // 清除消息，恢复默认样式
        m_messageLabel->clear();
        m_messageLabel->setStyleSheet("color: #f8f8f2; padding: 0 12px;");
    }
}

void MyStatusBar::onCPUInfoUpdated(double cpuUsage)
{
    m_cpuLabel->setText(QString("CPU: %1%").arg(cpuUsage));
}

void MyStatusBar::onFPSInfoUpdated(double frameRate)
{
    m_fpsLabel->setText(QString("FPS: %1 / 30").arg(frameRate));
}

void MyStatusBar::onStatusInfoUpdated(const QString& recordStatus,const QString& streamStatus)
{
    // 更新各状态标签内容
    QString recordPath,streamPath;
    if(recordStatus == "录制中")
    {
        recordPath = ":/sources/point_green.png";
        m_recordLabel->setStyleSheet("color: #ffffff; padding: 0 8px;");
    }else
    {
        recordPath = ":/sources/point_grey.png";
        m_recordLabel->setStyleSheet("color: #747d94; padding: 0 8px;");
    }

    if(streamStatus == "推流中")
    {
        streamPath = ":/sources/signal_blue.png";
        m_streamLabel->setStyleSheet("color: #ffffff; padding: 0 8px;");
    }else
    {
        streamPath = ":/sources/signal_grey.png";
        m_streamLabel->setStyleSheet("color: #747d94; padding: 0 8px;");
    }


    m_recordLabel->setText(QString("<span style='vertical-align: middle;'>"
                                   "<img src='%1' width='15' height='15' style='position: relative; top: 1px;'/>"
                                   " %2</span>")
                               .arg(recordPath)
                               .arg(recordStatus));
    m_streamLabel->setText(QString("<span style='vertical-align: middle;'>"
                                   "<img src='%1' width='15' height='15' style='position: relative; top: 1px;'/>"
                                   " %2</span>")
                               .arg(streamPath)
                               .arg(streamStatus));

}
