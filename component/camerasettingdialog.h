#ifndef CAMERASETTINGDIALOG_H
#define CAMERASETTINGDIALOG_H

#include <QDialog>
#include <QMediaDevices>
#include <QCameraDevice>

namespace Ui {
class CameraSettingDialog;
}
struct CameraSetting {
    QString deviceName;
    QString nickName;
    int frameRate;
    QString resolution;
    QString videoFormat;
};

class CameraSettingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CameraSettingDialog(const QString& title,QWidget *parent = nullptr);
    ~CameraSettingDialog();

    void initDevice();
    void updateFormats(int index);
    void onTypeChanged(int index);
    void setInitialSetting(const CameraSetting& setting);
    CameraSetting getCameraSetting() const;


private slots:
    void on_pushButton_clicked();

private:
    Ui::CameraSettingDialog *ui;
    QList<QCameraDevice> camerasList_;


};
Q_DECLARE_METATYPE(CameraSetting)
#endif // CAMERASETTINGDIALOG_H
