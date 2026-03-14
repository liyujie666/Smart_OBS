#ifndef AUDIOMIXERDIALOG_H
#define AUDIOMIXERDIALOG_H

#include <QDialog>
#include <QComboBox>
namespace Ui {
class AudioMixerDialog;
}

class AudioItemWidget;

enum class MixerListenMode{
    CLOSE,           // 关闭监听
    LISTEN_MUTE,    // 监听但输出静音
    LISTEN_OUTPUT,         // 监听  
};

struct AudioMixerConfig{
    int sourceId;
    QString nickName;
    QString iconUrl;
    QString state;
    double volumeDB;
    double offset;
    MixerListenMode mode;

    AudioMixerConfig():state("已激活"),volumeDB(0.0),offset(0),mode(MixerListenMode::CLOSE){}
};

class AudioMixerDialog : public QDialog
{
    Q_OBJECT

public:
    static AudioMixerDialog* getInstance()
    {

        if (!s_instance) {
            s_instance = new AudioMixerDialog; // 此时QApplication已存在，可安全创建
        }
        return s_instance;
    }

    static void destroyInstance()
    {
        if (s_instance) {
            delete s_instance;
            s_instance = nullptr;
        }
    }


    AudioMixerDialog(const AudioMixerDialog&) = delete;
    AudioMixerDialog& operator=(const AudioMixerDialog&) = delete;


    void initTableWidget();
    void initTableItem();
    void addAudioItem(int sourceId,AudioMixerConfig& config,AudioItemWidget* widget);
    void removeAudioItem(int sourceId);
    void createTableItem(AudioMixerConfig& config);
    QComboBox* createMonitorComboBox();
    void clearTableItems();
    void clearAMediaItem();
    AudioItemWidget* getAudioItemWidget(int sourceId);

signals:
    void listenModeChanged(int sourceId, MixerListenMode mode);

private:
    explicit AudioMixerDialog(QWidget *parent = nullptr);
    ~AudioMixerDialog();
    Ui::AudioMixerDialog *ui;

    static AudioMixerDialog* s_instance;
    std::unordered_map<int,AudioMixerConfig> mixerConfigs_;
    std::unordered_map<int,AudioItemWidget*> mixerWidgets_;
};

#endif // AUDIOMIXERDIALOG_H
