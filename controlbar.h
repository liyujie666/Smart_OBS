#ifndef CONTROLBAR_H
#define CONTROLBAR_H
#include "controller/mediasourcecontroller.h"
#include <QWidget>

namespace Ui {
class ControlBar;
}

class ControlBar : public QWidget
{
    Q_OBJECT

public:
    explicit ControlBar(QWidget *parent = nullptr);
    ~ControlBar();

    void connectSignals();
    void setCurrentSourceId(int sourceId);
    void setCurrentSceneId(int sceneId);
    void setMediaSourceContrller(MediaSourceController* mediaSourceController);

    int currentSourceId() const { return currentSourceId_; }
    int currentSceneId() const { return currentSceneId_; }

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
private slots:
    void on_startBtn_clicked();
    void on_stopBtn_clicked();
    void on_processSlider_sliderPressed();
    void on_processSlider_sliderReleased();
    void on_processSlider_valueChanged(int value);

    // 调整：信号带sourceId参数，仅处理当前源的信号
    void onPlayStateChanged(int sourceId, bool isPlaying,int sceneId);
    void onTimeChanged(int sourceId, int64_t currentUs,int sceneId);
    void onMediaEnded(int sourceId,int sceneId);

private:
    Ui::ControlBar *ui;
    MediaSourceController* mediaSourceController_ = nullptr;
    bool isDraggingProgress_ = false;
    int currentSourceId_ = -1;
    int currentSceneId_ = -1;


    QString formatTime(int64_t sec);
    void updateControlState();
    void refreshCurrentSourceInfo();
    int calculateProgressValue(const QPoint &pos);
};

#endif // CONTROLBAR_H
