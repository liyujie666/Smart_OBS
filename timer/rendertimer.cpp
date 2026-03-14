#include "rendertimer.h"

#include <QApplication>
#include <chrono>
#include <QDebug>

// 初始化静态事件类型（全局唯一标识）
const QEvent::Type RenderTimer::RenderTriggerEvent = static_cast<QEvent::Type>(QEvent::User + 100);

// 构造函数：转换间隔为微秒，验证目标对象线程
RenderTimer::RenderTimer(QObject* target, double intervalUs, QObject* parent)
    : QThread(parent),
    m_target(target),
    m_intervalUs(static_cast<int64_t>(intervalUs)),
    m_running(false)
{
    // 强制目标对象必须在主线程（否则事件无法正确投递）
    if (target->thread() != QCoreApplication::instance()->thread()) {
        qFatal("RenderTimer: Target object must be in main thread!");
    }
}

// 启动定时器线程
void RenderTimer::startTimer()
{
    if (!m_running.load()) {
        m_running.store(true);
        start(); // 启动线程（执行run()）
    }
}

// 停止定时器线程（安全退出）
void RenderTimer::stopTimer()
{
    if (m_running.load()) {
        m_running.store(false);
        wait(); // 等待线程结束
    }
}

void RenderTimer::run() {
    using namespace std::chrono;
    high_resolution_clock::time_point nextTrigger = high_resolution_clock::now();

    while (m_running.load()) {
        nextTrigger += microseconds(m_intervalUs);
        std::this_thread::sleep_until(nextTrigger);

        if (m_running.load()) {
            QApplication::postEvent(m_target, new QEvent(RenderTriggerEvent));
        }
    }
}
