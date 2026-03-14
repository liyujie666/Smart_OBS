#ifndef RENDERTIMER_H
#define RENDERTIMER_H

#include <QThread>
#include <QEvent>
#include <QObject>
#include <atomic>

// 非模板类，直接继承QThread
class RenderTimer : public QThread
{
    Q_OBJECT
public:
    // 自定义事件类型（全局唯一）
    static const QEvent::Type RenderTriggerEvent;

    explicit RenderTimer(QObject* target, double intervalUs = 3333333, QObject* parent = nullptr);

    void startTimer(); // 启动定时
    void stopTimer();  // 停止定时

protected:
    void run() override; // 线程执行逻辑

private:
    QObject* m_target;        // 主线程目标对象（必须在主线程创建）
    int64_t m_intervalUs;     // 触发间隔（微秒）
    std::atomic<bool> m_running; // 线程运行状态

};

#endif // RENDERTIMER_H

