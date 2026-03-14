#ifndef FPSCOUNTER_H
#define FPSCOUNTER_H
#include <chrono>
#include <deque>
#include <QObject>
#include <QDebug>

class FPSCounter : public QObject
{
    Q_OBJECT
public:
    FPSCounter(double winSeconds = 1.0, double maxFPS = 30.0)
        : winSeconds_(winSeconds), maxFPS_(maxFPS), lastTickTime_(std::chrono::high_resolution_clock::now())
    {
        targetInterval_ = 1.0 / maxFPS_;

        qDebug() <<"maxFPS" << maxFPS <<  "targetInterval_" << targetInterval_;
    }
    ~FPSCounter() = default;

    void tick()
    {
        auto now = std::chrono::high_resolution_clock::now();
        // std::chrono::duration<double> elapsed = now - lastTickTime_;

        // // 如果距离上一次tick的时间小于目标间隔，则等待补足剩余时间
        // if (elapsed.count() < targetInterval_)
        // {
        //     std::chrono::duration<double> waitTime(targetInterval_ - elapsed.count());
        //     std::this_thread::sleep_for(waitTime);
        //     now = std::chrono::high_resolution_clock::now();
        // }

        // lastTickTime_ = now;

        timestamps_.push_back(now);

        // 移除窗口时间外的时间戳
        while(!timestamps_.empty() && std::chrono::duration<double>(now - timestamps_.front()).count() > winSeconds_)
        {
            timestamps_.pop_front();
        }

        currentFPS_ = timestamps_.size() / winSeconds_;

        if (std::chrono::duration<double>(now - lastNotifyTime_).count() >= 1.0) {
            // lastNotifyTime_ = now;
            emit fpsInfoUpdated(currentFPS_);
        }
    }

    double getFPS() const {
        return currentFPS_;
    }

    void setMaxFPS(double maxFPS) {
        if (maxFPS > 0) {
            maxFPS_ = maxFPS;
            targetInterval_ = 1.0 / maxFPS_;
        }
    }

signals:
    void fpsInfoUpdated(double fps);

private:
    double winSeconds_;                  // 计算FPS的窗口时间(秒)
    double maxFPS_;                      // 最大FPS限制
    double targetInterval_;              // 目标帧率对应的时间间隔(秒)
    double currentFPS_ = 0.0;            // 当前FPS值
    std::deque<std::chrono::high_resolution_clock::time_point> timestamps_;  // 时间戳队列
    std::chrono::high_resolution_clock::time_point lastNotifyTime_;  // 上次通知时间
    std::chrono::high_resolution_clock::time_point lastTickTime_;    // 上次tick时间
};

#endif // FPSCOUNTER_H
