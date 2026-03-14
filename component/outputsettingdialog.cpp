#include "outputsettingdialog.h"
#include "ui_outputsettingdialog.h"
#include <QFileDialog>
outputSettingDialog::outputSettingDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::outputSettingDialog)
{
    ui->setupUi(this);
    setWindowTitle("设置");
    ui->resolutionCBBox->addItem("1920 x 1080", "1920x1080");
    ui->resolutionCBBox->addItem("2560 x 1440", "2560x1440");
    ui->resolutionCBBox->addItem("1280 x 720", "1280x720");
    ui->sampleRateCBBox->addItem("48 KHz", QVariant(48000));
    ui->sampleRateCBBox->addItem("44.1 KHz", QVariant(44100));

    ui->audioBitRateCBBox->addItem("64 Kbps", QVariant(64000));
    ui->audioBitRateCBBox->addItem("96 Kbps", QVariant(96000));
    ui->audioBitRateCBBox->addItem("128 Kbps", QVariant(128000));
    ui->audioBitRateCBBox->addItem("160 Kbps", QVariant(160000));
    ui->audioBitRateCBBox->addItem("192 Kbps", QVariant(192000));
    ui->audioBitRateCBBox->addItem("256 Kbps", QVariant(256000));
    ui->audioBitRateCBBox->addItem("320 Kbps", QVariant(320000));

    // 默认选择 128 Kbps：
    ui->audioBitRateCBBox->setCurrentIndex(2);

}

outputSettingDialog::~outputSettingDialog()
{
    delete ui;
}

void outputSettingDialog::updateConfig()
{
    config_.filePath = ui->recordPath->text();
    config_.streamUrl = ui->pushUrl->text();

    // 视频参数
    QStringList parts = ui->resolutionCBBox->currentText().split("x");
    if(parts.size() == 2)
    {
        config_.vEnConfig.width = parts[0].toInt();
        config_.vEnConfig.height = parts[1].toInt();
    }
    config_.vEnConfig.bitrate = ui->videoBitRate->value() * 1000;
    config_.vEnConfig.framerate = ui->frameRate->value();
    config_.vEnConfig.format = ui->videoFormatCBBox->currentText();
    config_.vEnConfig.gop_size = ui->gop->value();
    config_.vEnConfig.max_b_frames = ui->bframe->value();

    // 音频参数
    config_.aEnConfig.sampleRate = ui->sampleRateCBBox->currentData().toInt();
    config_.aEnConfig.bitRate = ui->audioBitRateCBBox->currentData().toInt();
    config_.aEnConfig.nbChannels = ui->channelsCBBox->currentText().toInt();
    config_.aEnConfig.sampleFmt = AV_SAMPLE_FMT_FLTP;
}

StreamConfig outputSettingDialog::getStreamConfig()
{
    updateConfig();
    return config_;
}

void outputSettingDialog::on_changePathBtn_clicked()
{
    // 弹出选择目录的对话框，返回用户选择的目录路径
    QString dir = QFileDialog::getExistingDirectory(this, tr("选择文件夹"),
                                                    "",
                                                    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dir.isEmpty()) {
        // 这里 dir 是用户选择的路径，你可以用它去做后续处理，比如显示到 UI 上
        ui->recordPath->setText(dir);  // 假设你有个 QLineEdit 用于显示路径
    }
}


void outputSettingDialog::init(const StreamConfig& cfg)
{
    config_ = cfg;

    // 恢复 UI 值
    ui->recordPath->setText(cfg.filePath);
    ui->pushUrl->setText(cfg.streamUrl);

    ui->videoBitRate->setValue(cfg.vEnConfig.bitrate / 1000);
    ui->frameRate->setValue(cfg.vEnConfig.framerate);
    ui->videoFormatCBBox->setCurrentText(cfg.vEnConfig.format);
    ui->gop->setValue(cfg.vEnConfig.gop_size);
    ui->bframe->setValue(cfg.vEnConfig.max_b_frames);

    QString res = QString("%1x%2").arg(cfg.vEnConfig.width).arg(cfg.vEnConfig.height);
    int resolutionIndex = ui->resolutionCBBox->findData(res);
    if(resolutionIndex != -1)
    {
        ui->resolutionCBBox->setCurrentIndex(resolutionIndex);
    }
    int sampleRateIndex = ui->sampleRateCBBox->findData(cfg.aEnConfig.sampleRate);
    if (sampleRateIndex != -1)
        ui->sampleRateCBBox->setCurrentIndex(sampleRateIndex);

    int audioBitRateIndex = ui->audioBitRateCBBox->findData(cfg.aEnConfig.bitRate);
    if (audioBitRateIndex != -1)
        ui->audioBitRateCBBox->setCurrentIndex(audioBitRateIndex);

    int channelIndex = ui->channelsCBBox->findText(QString::number(cfg.aEnConfig.nbChannels));
    if (channelIndex != -1)
        ui->channelsCBBox->setCurrentIndex(channelIndex);

    int formatIndex = ui->videoFormatCBBox->findData(cfg.vEnConfig.format);
    if(formatIndex != -1)
    {
        ui->videoFormatCBBox->setCurrentIndex(formatIndex);
    }
}
