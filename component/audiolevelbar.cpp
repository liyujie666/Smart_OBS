#include "audiolevelbar.h"
#include <QPainter>
#include <QLinearGradient>
#include <QDebug>

AudioLevelBar::AudioLevelBar(QWidget* parent)
    : QWidget(parent)
{
}

void AudioLevelBar::setLevel(float dB)
{
    if (dB < -60.f) dB = -60.f;
    if (dB > 0.f) dB = 0.f;

    if (qFuzzyCompare(level_dB_, dB))
        return;

    level_dB_ = dB;
    update();
}

void AudioLevelBar::setVolumeGain(float dB)
{
    volumeGain_dB_ = dB;
    update();
}

void AudioLevelBar::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRectF rect = this->rect();
    painter.fillRect(rect, QColor(40, 40, 40));

    // 增益（volume slider
    float adjusted_dB = level_dB_ + volumeGain_dB_;
    if (adjusted_dB < -60.f) adjusted_dB = -60.f;
    if (adjusted_dB > 0.f) adjusted_dB = 0.f;

    float progress = (60.0f + adjusted_dB) / 60.0f;
    int levelWidth = static_cast<int>(rect.width() * progress);

    // 渐变色填充（音量颜色渐变）
    QLinearGradient gradient(0, 0, rect.width(), 0);
    gradient.setColorAt(0.0, Qt::green);             // -60dB
    gradient.setColorAt(0.583f, Qt::yellow);         // -25dB
    gradient.setColorAt(0.833f, QColor(255, 128, 0)); // -10dB
    gradient.setColorAt(1.0, Qt::red);               // 0dB

    QRectF levelRect(rect.left(), rect.top(), levelWidth, rect.height());
    painter.fillRect(levelRect, gradient);

    // 刻度线与文字
    static const QPen pen(QColor("#999999"));
    static QFont font = painter.font();
    font.setPointSize(7);
    painter.setFont(font);
    painter.setPen(pen);

    const int step_dB = 10;
    for (int db = -60; db <= 0; db += step_dB) {
        float x = rect.width() * (60.0f + db) / 60.0f;
        painter.drawLine(QPointF(x, 0), QPointF(x, rect.height()));
        QString label = QString::number(db) + "dB";
        painter.drawText(QPointF(x + 2, rect.height() - 2), label);
    }

    QWidget::paintEvent(event);
}

