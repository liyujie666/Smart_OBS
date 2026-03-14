#ifndef SCREENSETTINGDIALOG_H
#define SCREENSETTINGDIALOG_H

#include <QDialog>
#include <QMediaDevices>
#include <QCameraDevice>
#include "infotools.h"
namespace Ui {
class ScreenSettingDialog;
}
struct ScreenSetting{
    QString screenName;
    QString nickName;
    QString resolution;
    int screenIndex;
    int offset_X;
    int offset_Y;
    DesktopCaptureType captureType;
    bool enaleDrawMouse;

};

class ScreenSettingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ScreenSettingDialog(const QString& title,QWidget *parent = nullptr);
    ~ScreenSettingDialog();

    void initDevice();
    void setInitialSetting(const ScreenSetting& setting);
    ScreenSetting getCurrentScreenSetting();

private:
    Ui::ScreenSettingDialog *ui;
    std::vector<ScreenSetting> screenSettings;
    int currentScreenIndex = 0;
};
Q_DECLARE_METATYPE(ScreenSetting)
#endif // SCREENSETTINGDIALOG_H
