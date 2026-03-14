#include "filesettingdialog.h"
#include "ui_filesettingdialog.h"
#include <QFileDialog>
FileSettingDialog::FileSettingDialog(const QString& title,QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::FileSettingDialog)
    ,title_(title)
{
    ui->setupUi(this);
    setWindowTitle("设置 " + title);
    ui->speedSlider->setRange(5,20);
    ui->speedSlider->setValue(10);
    ui->speedSlider->setSingleStep(5);
    ui->speedSlider->setPageStep(5);
    ui->speedSlider->setTickInterval(5);
    ui->speedSlider->setTickPosition(QSlider::TicksBelow);

    connect(ui->speedSlider, &QSlider::sliderMoved, this, [=](int value){
        int snapped = ((value + 2) / 5) * 5;
        if (snapped != value)
            ui->speedSlider->setValue(snapped);
    });
}

FileSettingDialog::~FileSettingDialog()
{
    delete ui;
}

void FileSettingDialog::initDevice(QWidget *parent)
{


}

void FileSettingDialog::setInitialSetting(const FileSetting &setting)
{
    ui->filePathLineEdit->setText(setting.filePath);
    ui->speedSlider->setValue(setting.speed);
}

FileSetting FileSettingDialog::getFileSetting() const
{
    FileSetting setting;
    setting.filePath = ui->filePathLineEdit->text();
    setting.speed = ui->speedSlider->value();
    setting.nickName = title_;
    return setting;
}

void FileSettingDialog::on_openFileBtn_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(this,
                                                    tr("选择视频文件"),
                                                    QDir::homePath(),
                                                    tr("视频文件 (*.mp4 *.avi *.mkv *.mov *.wmv *.flv *.rmvb *.3gp)"));
    if(filePath.isEmpty()) return;
    ui->filePathLineEdit->setText(filePath);
}


void FileSettingDialog::on_speedSlider_valueChanged(int value)
{

    ui->speedLabel->setText(QString("%1x").arg(value/10.0, 0, 'f', 1));
}

