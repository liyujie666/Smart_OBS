#include "audiomonitor.h"
#include <QDebug>
AudioMonitor::AudioMonitor(QObject *parent)
    : QObject{parent}
{
    start();
}

AudioMonitor::~AudioMonitor()
{
    stop();
}

void AudioMonitor::start()
{
    if (audioSink_ && audioOutDev_ && audioSink_->state() == QAudio::ActiveState) {
        return;
    }

    stop();
    isRunning_ = true;

    // 获取输出的设备
    QAudioDevice outputDevice = QMediaDevices::defaultAudioOutput();
    const QAudioFormat format = outputDevice.preferredFormat();
    // qDebug() << "audio fotmat" << format;
    // 检查格式
    if(!outputDevice.isFormatSupported(format))
    {
        qWarning() << "audio output format is not supported!";
        return;
    }

    // 创建QAudioSink
    audioSink_ = new QAudioSink(outputDevice,format,this);
    audioOutDev_ = audioSink_->start();
    return;
}

void AudioMonitor::stop()
{
    isRunning_ = false;
    if(audioSink_)
    {
        audioSink_->stop();
        delete audioSink_;
        audioSink_ = nullptr;
        audioOutDev_ = nullptr;
        qDebug() << "AudioMonitor is closed!";
    }
}

void AudioMonitor::setMute(bool mute)
{
    if(isMuted_ == mute) return;
    isMuted_ = mute;
}

bool AudioMonitor::isMute()
{
    return isMuted_;
}

QIODevice *AudioMonitor::getAudioDevice()
{
    return audioOutDev_;
}

void AudioMonitor::playAudio(const QByteArray &data)
{
    if (isMuted_ || !isRunning_ || !audioOutDev_ || data.isEmpty()) {
        return;
    }
    audioOutDev_->write(data);
}

void AudioMonitor::setListenMode(int sourceId,MixerListenMode mode)
{
    if (currentMode_ == mode) return;
    currentMode_ = mode;

    switch (mode) {
    case MixerListenMode::CLOSE:
        stop();
        break;
    case MixerListenMode::LISTEN_MUTE:
    case MixerListenMode::LISTEN_OUTPUT:
        start();
        break;
    default:
        stop();
        break;
    }
}

