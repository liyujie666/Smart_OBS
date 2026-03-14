#include "screensettingdialog.h"
#include "ui_screensettingdialog.h"
#include <QScreen>
ScreenSettingDialog::ScreenSettingDialog(const QString& title,QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ScreenSettingDialog)
{
    ui->setupUi(this);
    setWindowTitle("设置 " + title);
    initDevice();
}

ScreenSettingDialog::~ScreenSettingDialog()
{
    delete ui;
}

void ScreenSettingDialog::initDevice()
{
    ui->screenCBBox->clear();
    screenSettings.clear();

    const QList<QScreen *> screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        QScreen* screen = screens[i];
        QString name = screen->name();
        QRect geometry = screen->geometry();

        ScreenSetting setting;
        setting.screenName = name;
        setting.screenIndex = i;
        setting.resolution = QString("%1x%2").arg(geometry.width()).arg(geometry.height());
        setting.offset_X = geometry.x();
        setting.offset_Y = geometry.y();
        screenSettings.push_back(setting);

        QString label = QString(": %1x%2 @ %3 (显示器 %4)")
                            .arg(geometry.width())
                            .arg(geometry.height())
                            .arg(i)
                            .arg(name);
        ui->screenCBBox->addItem(label, QVariant::fromValue(i));
    }
}

void ScreenSettingDialog::setInitialSetting(const ScreenSetting &setting)
{
    for (int i = 0; i < ui->screenCBBox->count(); ++i) {
        int index = ui->screenCBBox->itemData(i).toInt();
        if (index == setting.screenIndex) {
            ui->screenCBBox->setCurrentIndex(i);
            break;
        }
    }
    if(setting.captureType == DesktopCaptureType::DXGI)
        ui->captureTypeCBBox->setCurrentIndex(0);
    else
        ui->captureTypeCBBox->setCurrentIndex(1);

    if(setting.enaleDrawMouse)
        ui->drawMouseCKBox->setCheckState(Qt::Checked);
    else
        ui->drawMouseCKBox->setCheckState(Qt::Unchecked);


}
ScreenSetting ScreenSettingDialog::getCurrentScreenSetting()
{
    currentScreenIndex = ui->screenCBBox->currentIndex();
    int index = ui->captureTypeCBBox->currentIndex();
    if(index == 0)
    {
        screenSettings[currentScreenIndex].captureType = DesktopCaptureType::DXGI;
    }else {
        screenSettings[currentScreenIndex].captureType = DesktopCaptureType::FFmpegGdiGrab;
    }
    screenSettings[currentScreenIndex].enaleDrawMouse = ui->drawMouseCKBox->isChecked();
    return screenSettings[currentScreenIndex];
}
