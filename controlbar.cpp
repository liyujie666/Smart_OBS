#include "controlbar.h"
#include "ui_controlbar.h"
#include <QDebug>
#include <QToolTip>
ControlBar::ControlBar(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ControlBar)
{
    ui->setupUi(this);
    ui->processSlider->installEventFilter(this);
    ui->processSlider->setMouseTracking(true);
}

ControlBar::~ControlBar()
{
    qDebug() << "ControlBar被销毁，this=" << this;
    delete ui;
}

// 设置当前选中的源ID（核心方法，由外部点击源时调用）
void ControlBar::setCurrentSourceId(int sourceId)
{
    // 同一个源被点击，不做处理
    if (currentSourceId_ == sourceId) return;

    currentSourceId_ = sourceId;
    updateControlState();

    // 如果是有效源，刷新UI显示该源的信息
    if (currentSourceId_ != -1) {
        refreshCurrentSourceInfo();
    } else {
        // 无选中源时重置UI
        ui->processSlider->setValue(0);
        ui->curTimeLabel->setText(formatTime(0));
        ui->totalTimeLabel->setText(formatTime(0));
        ui->startBtn->setIcon(QIcon(":/sources/play.png"));
    }
}

void ControlBar::setCurrentSceneId(int sceneId)
{
    if (currentSceneId_ == sceneId) return;

    currentSceneId_ = sceneId;
    updateControlState();
    refreshCurrentSourceInfo();
}

void ControlBar::setMediaSourceContrller(MediaSourceController *mediaSourceController)
{

    mediaSourceController_ = mediaSourceController;
    connectSignals();
    updateControlState();
}

bool ControlBar::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->processSlider) {
        // 鼠标移动：仅显示tooltip，不干扰任何逻辑
        if (event->type() == QEvent::MouseMove) {
            if(!mediaSourceController_->isPlaying(currentSourceId_,currentSceneId_)) return false;

            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            int value = calculateProgressValue(mouseEvent->pos());
            QPoint globalPos = ui->processSlider->mapToGlobal(QPoint(mouseEvent->x(), 0));
            globalPos.setY(globalPos.y() + 20);
            QToolTip::showText(globalPos, formatTime(value), ui->processSlider);
            return false; // 仅拦截移动事件（不影响拖拽）
        }

        // 鼠标按下：仅处理“点击跳转”，不阻断拖拽起始信号
        else if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                int newVal = calculateProgressValue(mouseEvent->pos());
                // 先设置滑块值（视觉同步）
                ui->processSlider->setValue(newVal);

                // 若当前未处于拖拽状态（纯点击），直接seek
                if (!isDraggingProgress_) {
                    if (mediaSourceController_ && currentSourceId_ != -1 && currentSceneId_ != -1) {
                        mediaSourceController_->seek(currentSourceId_, newVal,currentSceneId_);
                        ui->curTimeLabel->setText(formatTime(newVal));
                    }
                }

                return false;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}
// 更新控件状态（根据当前是否有选中源）
void ControlBar::updateControlState()
{
    bool hasValidSource = (currentSourceId_ != -1) && (mediaSourceController_ != nullptr) && (currentSceneId_ != -1);
    ui->startBtn->setEnabled(hasValidSource);
    ui->stopBtn->setEnabled(hasValidSource);
    ui->processSlider->setEnabled(hasValidSource);
}

void ControlBar::refreshCurrentSourceInfo()
{
    if (!mediaSourceController_ || currentSourceId_ == -1 || currentSceneId_ == -1) {
        // 无有效信息，重置UI
        ui->processSlider->setValue(0);
        ui->curTimeLabel->setText(formatTime(0));
        ui->totalTimeLabel->setText(formatTime(0));
        ui->startBtn->setIcon(QIcon(":/sources/play.png"));
        return;
    }

    // 传入当前场景ID，获取当前场景下该源的状态
    bool isPlaying = mediaSourceController_->isPlaying(currentSourceId_, currentSceneId_);
    double currentSec = mediaSourceController_->currentTime(currentSourceId_, currentSceneId_) / 1000000.0; // 微秒转秒
    double totalSec = mediaSourceController_->duration(currentSourceId_, currentSceneId_);

    // 更新UI
    ui->startBtn->setIcon(isPlaying ? QIcon(":/sources/pause.png") : QIcon(":/sources/play.png"));
    ui->processSlider->setRange(0, static_cast<int>(totalSec));
    ui->processSlider->setValue(static_cast<int>(currentSec));
    ui->curTimeLabel->setText(formatTime(static_cast<int64_t>(currentSec)));
    ui->totalTimeLabel->setText(formatTime(static_cast<int64_t>(totalSec)));
    ui->stopBtn->setEnabled(true);
}

void ControlBar::on_startBtn_clicked()
{
    if (!mediaSourceController_ || currentSourceId_ == -1 || currentSceneId_ == -1) return;

    if (mediaSourceController_->isPlaying(currentSourceId_,currentSceneId_)) {
        mediaSourceController_->pause(currentSourceId_,currentSceneId_);
    } else {
        mediaSourceController_->play(currentSourceId_,currentSceneId_);
    }
}

void ControlBar::on_stopBtn_clicked()
{
    if (!mediaSourceController_ || currentSourceId_ == -1 || currentSceneId_ == -1) return;

    mediaSourceController_->stop(currentSourceId_,currentSceneId_);
    ui->startBtn->setIcon(QIcon(":/sources/play.png"));
    ui->processSlider->setValue(0);
    ui->curTimeLabel->setText(formatTime(0));
    ui->totalTimeLabel->setText(formatTime(0));
}

// 进度条拖拽开始
void ControlBar::on_processSlider_sliderPressed()
{
    isDraggingProgress_ = true;
}

// 进度条拖拽结束（执行seek）
void ControlBar::on_processSlider_sliderReleased()
{
    if (!mediaSourceController_ || currentSourceId_ == -1 || !isDraggingProgress_) return;

    int position = ui->processSlider->value();
    mediaSourceController_->seek(currentSourceId_, position,currentSceneId_);
    ui->curTimeLabel->setText(formatTime(position));
    isDraggingProgress_ = false;
}

// 进度条值变化（仅在拖拽时更新标签，不执行seek）
void ControlBar::on_processSlider_valueChanged(int value)
{
    if (isDraggingProgress_) {
        ui->curTimeLabel->setText(formatTime(value));
    }
}

// 播放状态变化（仅处理当前选中源的信号）
void ControlBar::onPlayStateChanged(int sourceId, bool isPlaying,int sceneId)
{
    if (sourceId != currentSourceId_ || sceneId != currentSceneId_) return;

    if (isPlaying) {
        ui->startBtn->setIcon(QIcon(":/sources/pause.png"));
    } else {
        ui->startBtn->setIcon(QIcon(":/sources/play.png"));
    }
}

// 时间变化（仅处理当前选中源的信号）
void ControlBar::onTimeChanged(int sourceId, int64_t currentUs,int sceneId)
{
    if (sourceId != currentSourceId_ || sceneId != currentSceneId_ || isDraggingProgress_) return;

    double duration = mediaSourceController_->duration(currentSourceId_,sceneId);
    ui->processSlider->setRange(0, static_cast<int>(duration));
    ui->processSlider->setValue(static_cast<int>(currentUs / 1000000));
    ui->curTimeLabel->setText(formatTime(currentUs / 1000000));
    ui->totalTimeLabel->setText(formatTime(duration));
}

// 媒体播放结束（仅处理当前选中源的信号）
void ControlBar::onMediaEnded(int sourceId,int sceneId)
{
    if (sourceId != currentSourceId_ || sceneId != currentSceneId_) return;

    ui->startBtn->setIcon(QIcon(":/sources/play.png"));
    ui->processSlider->setValue(0);
    ui->curTimeLabel->setText(formatTime(0));
    ui->totalTimeLabel->setText(formatTime(0));
    qDebug() << "媒体源" << sourceId << "播放结束";
}

void ControlBar::connectSignals()
{
    // 连接媒体控制器的信号（带sourceId参数）
    connect(mediaSourceController_, &MediaSourceController::playStateChanged,
            this, &ControlBar::onPlayStateChanged);
    connect(mediaSourceController_, &MediaSourceController::timeChanged,
            this, &ControlBar::onTimeChanged);
    connect(mediaSourceController_, &MediaSourceController::mediaEnded,
            this, &ControlBar::onMediaEnded);
}

QString ControlBar::formatTime(int64_t sec)
{
    int totalSeconds = static_cast<int>(sec);
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;

    if (hours > 0) {
        return QString("%1:%2:%3")
        .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    } else {
        return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }
}

int ControlBar::calculateProgressValue(const QPoint &pos)
{
    int sliderMin = ui->processSlider->minimum();
    int sliderMax = ui->processSlider->maximum();
    // 计算点击位置占整个进度条宽度的比例
    double ratio = qBound(0.0, (double)pos.x() / ui->processSlider->width(), 1.0);
    // 计算对应的进度值
    return sliderMin + qRound(ratio * (sliderMax - sliderMin));
}
