#ifndef AUDIOMONITOR_H
#define AUDIOMONITOR_H
#include "component/audiomixerdialog.h"
#include <QAudioOutput>
#include <QMediaDevices>
#include <QAudioSink>
#include <QObject>
extern"C"
{
#include "libavutil/frame.h";
}
class AudioMonitor : public QObject
{
    Q_OBJECT
public:
    explicit AudioMonitor(QObject *parent = nullptr);
    ~AudioMonitor();

    void start();
    void stop();
    void setMute(bool mute);
    bool isMute();
    QIODevice* getAudioDevice();

public slots:
    void playAudio(const QByteArray& data);
    void setListenMode(int sourceId,MixerListenMode mode);

private:
    QAudioSink* audioSink_ = nullptr;
    QIODevice* audioOutDev_ = nullptr;
    bool isMuted_ = false;
    bool isRunning_ = false;
    MixerListenMode currentMode_ = MixerListenMode::CLOSE;

};

#endif // AUDIOMONITOR_H
