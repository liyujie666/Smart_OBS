#ifndef AUDIOITEMWIDGET_H
#define AUDIOITEMWIDGET_H
#include "audiolevelbar.h"
#include "source/audio/audiosource.h"
#include <QWidget>
#include <QObject>
#include <QPushButton>
namespace Ui {
class AudioItemWidget;
}

class AudioItemWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AudioItemWidget(AudioSource* audioSource,QWidget *parent = nullptr);
    ~AudioItemWidget();


    void setVolumeLevel(float level_dB);
    void setVolumeGain(double gain);
    void setAudioName(QString name);
    void setHided(bool hided);
    void initMixerPopup();

    void checkWidgetInContainer(const QString& stage);
    static bool isAudioItemWidget(QWidget *widget);

signals:
    void volumeGainChanged(float gain);
private slots:
    void on_muteBtn_clicked();
    void on_volumeSlider_valueChanged(int value);

private:
    void handleVolumeLockBtn(QPushButton* button);
    void handleRenameBtn(QPushButton* button);
    void handleHideBtn(QPushButton* button);
    void handleSettingBtn(QPushButton* button);
    void handleMixertSettingBtn(QPushButton* button);

private:
    Ui::AudioItemWidget *ui;
    AudioSource* m_source;
    AudioLevelBar* m_levelBar;
    QString m_name;
    float lastGain_ = 0.0f;

    bool m_volumeLocked = false;
    bool m_hided = false;
    QString m_sliderOriginalStyle;


};

#endif // AUDIOITEMWIDGET_H
