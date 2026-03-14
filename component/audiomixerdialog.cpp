#include "audiomixerdialog.h"
#include "ui_audiomixerdialog.h"
#include "component/audioitemwidget.h"
#include "sync/offsetmanager.h"
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
AudioMixerDialog* AudioMixerDialog::s_instance = nullptr;

AudioMixerDialog::AudioMixerDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AudioMixerDialog)
{
    ui->setupUi(this);
    setWindowTitle("混音器高级设置");
    initTableWidget();
}

AudioMixerDialog::~AudioMixerDialog()
{
    delete ui;
}

void AudioMixerDialog::initTableWidget()
{
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->tableWidget->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    ui->tableWidget->verticalHeader()->setDefaultSectionSize(40);
    ui->tableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->tableWidget->setFocusPolicy(Qt::NoFocus);
    ui->tableWidget->setColumnWidth(0, 250);
    ui->tableWidget->setColumnWidth(1, 90);
    ui->tableWidget->setColumnWidth(2, 110);
    ui->tableWidget->setColumnWidth(3, 110);
    ui->tableWidget->setColumnWidth(4, 150);
}


void AudioMixerDialog::initTableItem()
{
    clearTableItems();

    // 媒体音频
    for(auto& config : mixerConfigs_)
    {
        createTableItem(config.second);
    }
}

void AudioMixerDialog::addAudioItem(int sourceId, AudioMixerConfig &config,AudioItemWidget* widget)
{
    if(mixerConfigs_.find(sourceId) != mixerConfigs_.end() || mixerWidgets_.find(sourceId) != mixerWidgets_.end()) return;
    mixerConfigs_[sourceId] = config;
    mixerWidgets_[sourceId] = widget;
}


void AudioMixerDialog::removeAudioItem(int sourceId)
{
    if(mixerConfigs_.find(sourceId) == mixerConfigs_.end() || mixerWidgets_.find(sourceId) == mixerWidgets_.end()) return;
    mixerConfigs_.erase(mixerConfigs_.find(sourceId));
    mixerWidgets_.erase(mixerWidgets_.find(sourceId));
}

void AudioMixerDialog::createTableItem(AudioMixerConfig &config)
{
    int row = ui->tableWidget->rowCount();
    ui->tableWidget->insertRow(row);

    // 名称
    QWidget* nameWidget = new QWidget(this);
    QHBoxLayout* nameLayout = new QHBoxLayout(nameWidget);
    nameLayout->setContentsMargins(0, 0, 0, 0);
    nameLayout->setSpacing(3);

    QLabel* iconLabel = new QLabel();
    QIcon icon(config.iconUrl);
    iconLabel->setPixmap(icon.pixmap(16, 16));
    iconLabel->setContentsMargins(0, 0, 0, 0);
    iconLabel->setStyleSheet("max-width:20px;min-width:20px;");

    QLabel* nameLabel = new QLabel(config.nickName);
    nameLabel->setAlignment(Qt::AlignLeft);
    nameLabel->setStyleSheet("font-size:14px;color:white;padding-top:2px;");
    nameLabel->setContentsMargins(2, 0, 0, 0);

    nameLayout->addWidget(iconLabel);
    nameLayout->addWidget(nameLabel);
    nameLayout->addStretch();
    ui->tableWidget->setCellWidget(row, 0, nameWidget);

    // 状态
    QLabel* stateLabel = new QLabel(config.state);
    if(config.state == "已激活")
    {
        stateLabel->setStyleSheet("font-size:14px;color:green;");
    }else if (config.state == "未激活")
    {
        stateLabel->setStyleSheet("font-size:14px;color:red;");
    }
    stateLabel->setFocusPolicy(Qt::NoFocus);
    ui->tableWidget->setCellWidget(row,1,stateLabel);


    // 音量
    QDoubleSpinBox* volumeSpin = new QDoubleSpinBox();
    volumeSpin->setSingleStep(0.1);
    volumeSpin->setDecimals(1);
    volumeSpin->setRange(-60.0,10.0);
    volumeSpin->setValue(config.volumeDB);
    volumeSpin->setSuffix("dB");
    ui->tableWidget->setCellWidget(row,2,volumeSpin);

    // 同步偏移
    QSpinBox* offsetSpin = new QSpinBox();
    offsetSpin->setRange(-2000,2000);    // ms
    offsetSpin->setValue(config.offset);
    offsetSpin->setSuffix("ms");
    ui->tableWidget->setCellWidget(row,3,offsetSpin);

    // 音频监听
    QComboBox* moniterCBBox = createMonitorComboBox();
    switch (config.mode) {
    case MixerListenMode::CLOSE:
        moniterCBBox->setCurrentIndex(0);
        break;
    case MixerListenMode::LISTEN_MUTE:
        moniterCBBox->setCurrentIndex(1);
        break;
    case MixerListenMode::LISTEN_OUTPUT:
        moniterCBBox->setCurrentIndex(2);
        break;
    default:
        moniterCBBox->setCurrentIndex(0);
        break;
    }
    ui->tableWidget->setCellWidget(row,4,moniterCBBox);

    // 信号槽
    connect(volumeSpin,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[=](double value){
        mixerConfigs_[config.sourceId].volumeDB = value;
        mixerWidgets_[config.sourceId]->setVolumeGain(value);
        mixerWidgets_[config.sourceId]->findChild<QSlider*>("volumeSlider")->setValue(value * 10);
    });

    connect(offsetSpin,QOverload<int>::of(&QSpinBox::valueChanged),this,[=](int value){
        mixerConfigs_[config.sourceId].offset = value;
        OffsetManager::getInstance().setSourceOffset(true,config.sourceId,value);
    });
    connect(moniterCBBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, sourceId = config.sourceId](int index) {
        MixerListenMode mode = static_cast<MixerListenMode>(index);
        mixerConfigs_[sourceId].mode = mode; // 访问有效key，无野指针
        emit listenModeChanged(sourceId, mode);
    });

}

QComboBox* AudioMixerDialog::createMonitorComboBox()
{
    QComboBox* combo = new QComboBox();
    // 添加音频监听模式选项
    combo->addItem("关闭监听");
    combo->addItem("仅监听（输出静音）");
    combo->addItem("监听并输出");

    return combo;
}

void AudioMixerDialog::clearTableItems()
{
    while (ui->tableWidget->rowCount() > 0) {
        ui->tableWidget->removeRow(0);
    }
}

void AudioMixerDialog::clearAMediaItem()
{

    for (auto it = mixerConfigs_.begin(); it != mixerConfigs_.end();)
    {
        if (it->first != 0 && it->first != 1) // 注意这里应该是 && 而不是 ||
        {
            it = mixerConfigs_.erase(it); // erase 返回下一个有效迭代器
        }
        else
        {
            ++it;
        }
    }
    initTableItem();
}

AudioItemWidget *AudioMixerDialog::getAudioItemWidget(int sourceId)
{
    return mixerWidgets_[sourceId];
}


