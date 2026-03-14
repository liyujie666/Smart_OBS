#ifndef AUDIOLEVELBAR_H
#define AUDIOLEVELBAR_H

#include <QWidget>

class AudioLevelBar : public QWidget {
    Q_OBJECT
public:
    explicit AudioLevelBar(QWidget* parent = nullptr);

    void setLevel(float dB);  // dB: -60 ~ 0
    void setVolumeGain(float dB);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    float level_dB_ = -60.0f;
    float volumeGain_dB_ = 0.f;
};

#endif // AUDIOLEVELBAR_H
