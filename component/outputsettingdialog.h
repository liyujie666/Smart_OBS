#ifndef OUTPUTSETTINGDIALOG_H
#define OUTPUTSETTINGDIALOG_H
#include <controller/streamcontroller.h>
#include <QDialog>

namespace Ui {
class outputSettingDialog;
}

class outputSettingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit outputSettingDialog(QWidget *parent = nullptr);
    ~outputSettingDialog();

    void init(const StreamConfig& cfg);
    void updateConfig();
    StreamConfig getStreamConfig();

private slots:
    void on_changePathBtn_clicked();

private:
    Ui::outputSettingDialog *ui;
    StreamConfig config_;

};

#endif // OUTPUTSETTINGDIALOG_H
