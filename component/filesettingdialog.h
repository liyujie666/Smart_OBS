#ifndef FILESETTINGDIALOG_H
#define FILESETTINGDIALOG_H

#include <QDialog>

namespace Ui {
class FileSettingDialog;
}
struct FileSetting
{
    QString filePath;
    QString nickName;
    int speed;
};


class FileSettingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FileSettingDialog(const QString& title,QWidget *parent = nullptr);
    ~FileSettingDialog();

    void initDevice(QWidget *parent);
    void setInitialSetting(const FileSetting& setting);
    FileSetting getFileSetting() const;

private slots:
    void on_openFileBtn_clicked();

    void on_speedSlider_valueChanged(int value);

private:
    Ui::FileSettingDialog *ui;
    QString title_;
};
Q_DECLARE_METATYPE(FileSetting)
#endif // FILESETTINGDIALOG_H
