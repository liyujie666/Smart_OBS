#include "camerasettingdialog.h"
#include "ui_camerasettingdialog.h"

CameraSettingDialog::CameraSettingDialog(const QString& title,QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CameraSettingDialog)
{
    ui->setupUi(this);
    setWindowTitle("设置 " + title);
    initDevice();
}

CameraSettingDialog::~CameraSettingDialog()
{
    delete ui;
}

void CameraSettingDialog::initDevice()
{
    ui->cameraCBBox->clear();
    camerasList_ = QMediaDevices::videoInputs();
    for (const QCameraDevice &camera : camerasList_) {
        ui->cameraCBBox->addItem(camera.description());
    }

    if (!camerasList_.isEmpty()) {
        updateFormats(0);
        onTypeChanged(0);
    }

    connect(ui->cameraCBBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CameraSettingDialog::updateFormats);
    connect(ui->typeCBBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CameraSettingDialog::onTypeChanged);

}


void CameraSettingDialog::updateFormats(int index)
{
    if (index < 0 || index >= camerasList_.size()) return;

    const QCameraDevice &camera = camerasList_.at(index);
    const QList<QCameraFormat> formats = camera.videoFormats();

    ui->resolutionCBBox->clear();
    ui->fpsCBBox->clear();
    ui->videoFormatCBBox->clear();

    // 用 QSize 方便按面积排序
    QList<QSize>  sizes;
    QList<int>    fpsList;
    QSet<QString> formatSet;

    for (const QCameraFormat &fmt : formats) {
        sizes.append(fmt.resolution());
        fpsList.append(static_cast<int>(fmt.maxFrameRate()));
        formatSet.insert(
            QVideoFrameFormat::pixelFormatToString(fmt.pixelFormat()));
    }

    // 去重
    std::sort(sizes.begin(), sizes.end(),
              [](const QSize &a, const QSize &b) {
                  return a.width() * a.height() < b.width() * b.height();
              });
    sizes.erase(std::unique(sizes.begin(), sizes.end()), sizes.end());

    std::sort(fpsList.begin(), fpsList.end());
    fpsList.erase(std::unique(fpsList.begin(), fpsList.end()), fpsList.end());

    // 填充下拉框
    for (const QSize &s : sizes)
        ui->resolutionCBBox->addItem(QString("%1x%2").arg(s.width()).arg(s.height()));
    for (int fps : fpsList)
        ui->fpsCBBox->addItem(QString::number(fps));
    for (const QString &f : formatSet)
        ui->videoFormatCBBox->addItem(f);

    // 默认选中最大值
    if (!sizes.isEmpty())
        ui->resolutionCBBox->setCurrentIndex(sizes.size() - 1);
    if (!fpsList.isEmpty())
        ui->fpsCBBox->setCurrentIndex(fpsList.size() - 1);
}

CameraSetting CameraSettingDialog::getCameraSetting() const
{
    CameraSetting setting;
    setting.deviceName = ui->cameraCBBox->currentText();
    setting.resolution = ui->resolutionCBBox->currentText();
    setting.frameRate = ui->fpsCBBox->currentText().toInt();
    setting.videoFormat = ui->videoFormatCBBox->currentText();

    return setting;
}

void CameraSettingDialog::onTypeChanged(int index)
{
    bool isCustom = (ui->typeCBBox->currentText() == "自定义");
    ui->resolutionCBBox->setEnabled(isCustom);
    ui->fpsCBBox->setEnabled(isCustom);
    ui->videoFormatCBBox->setEnabled(isCustom);
    if(isCustom){
        QString style1 = "color:white;";
        ui->resolutionCBBox->setStyleSheet(style1);
        ui->fpsCBBox->setStyleSheet(style1);
        ui->videoFormatCBBox->setStyleSheet(style1);
        ui->videoFormatLabel->setStyleSheet(style1);
        ui->fpsLabel->setStyleSheet(style1);
        ui->resolutionLabel->setStyleSheet(style1);
    }else{
        QString style2 = "color:#999999;";
        ui->resolutionCBBox->setStyleSheet(style2);
        ui->fpsCBBox->setStyleSheet(style2);
        ui->videoFormatCBBox->setStyleSheet(style2);
        ui->videoFormatLabel->setStyleSheet(style2);
        ui->fpsLabel->setStyleSheet(style2);
        ui->resolutionLabel->setStyleSheet(style2);
    }

}

void CameraSettingDialog::setInitialSetting(const CameraSetting &setting)
{
    for(int i = 0;i < camerasList_.size();i++)
    {
        if(camerasList_[i].description() == setting.deviceName)
        {
            ui->cameraCBBox->setCurrentIndex(i);
            updateFormats(i);
            break;
        }
    }

    // 分辨率设置
    int resIndex = ui->resolutionCBBox->findText(setting.resolution);
    if (resIndex != -1)
        ui->resolutionCBBox->setCurrentIndex(resIndex);

    // 帧率设置
    ui->fpsCBBox->setCurrentText(QString::number(setting.frameRate));

    // 视频格式设置
    int fmtIndex = ui->videoFormatCBBox->findText(setting.videoFormat);
    if (fmtIndex != -1)
        ui->videoFormatCBBox->setCurrentIndex(fmtIndex);

}

void CameraSettingDialog::on_pushButton_clicked()
{

}

