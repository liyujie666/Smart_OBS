#include "audioitemwidget.h"
#include "ui_audioitemwidget.h"
#include "component/componentinitializer.h"
#include "component/audiomixerdialog.h"
#include "component/scrollablecontainer.h"

AudioItemWidget::AudioItemWidget(AudioSource *audioSource,QWidget *parent)
    : QWidget(parent),m_source(audioSource)
    , ui(new Ui::AudioItemWidget)
{
    ui->setupUi(this);
    ui->audioNameLabel->setText(audioSource->name());
    m_levelBar = new AudioLevelBar(this);
    m_levelBar->setFixedHeight(20);
    m_levelBar->setLevel(0.0f); // 假设当前为 -15 dB

    ui->hlayout2->addWidget(m_levelBar);
    ui->hlayout2->setSpacing(5);

    ui->volumeSlider->setRange(-600,0);
    ui->volumeSlider->setSingleStep(1);
    ui->volumeSlider->setValue(0);
    m_sliderOriginalStyle = ui->volumeSlider->styleSheet();
    initMixerPopup();
}

AudioItemWidget::~AudioItemWidget()
{
    delete ui;
}

void AudioItemWidget::setAudioName(QString name)
{
    ui->audioNameLabel->setText(name);
}

void AudioItemWidget::setHided(bool hided)
{
    m_hided = hided;
    setVisible(!hided);
}

void AudioItemWidget::initMixerPopup()
{
    auto* lockBtn = ComponentInitializer::getInstance()->createIconButton("锁定音量",this);
    auto* hideBtn = ComponentInitializer::getInstance()->createIconButton("隐藏",this);
    auto* renameBtn = ComponentInitializer::getInstance()->createIconButton("重命名",this);
    auto* settingBtn = ComponentInitializer::getInstance()->createIconButton("设置",this);
    auto* mixerSettingBtn = ComponentInitializer::getInstance()->createIconButton("混音器设置",this);

    QList<QPushButton*> buttons = {lockBtn, hideBtn, renameBtn,settingBtn,mixerSettingBtn};
    auto* popup = ComponentInitializer::getInstance()->createIconPopup(this, buttons);

    // 锁定音量
    connect(lockBtn,&QPushButton::clicked,this,[=](){
        handleVolumeLockBtn(lockBtn);
    });

    // 重命名
    connect(renameBtn,&QPushButton::clicked,this,[=](){
        handleRenameBtn(renameBtn);
    });

    // 隐藏
    connect(hideBtn,&QPushButton::clicked,this,[=](){
        handleHideBtn(hideBtn);
        if(m_hided){
            if (popup->isVisible()) {
                popup->hide();
                return;
            }
        }
    });

    // 设置
    connect(settingBtn,&QPushButton::clicked,this,[=](){
        handleSettingBtn(settingBtn);
    });

    // 混音器设置
    connect(mixerSettingBtn,&QPushButton::clicked,this,[=](){
        handleMixertSettingBtn(mixerSettingBtn);
    });

    // TODO 连接每个按钮信号
    connect(ui->audioSettingBtn, &QPushButton::clicked, this, [=]() {
        if (popup->isVisible()) {
            popup->hide();
            return;
        }

        popup->adjustSize();
        QPoint globalPos = ui->audioSettingBtn->mapToGlobal(QPoint(0, -popup->height()));
        QRect screenRect = QApplication::primaryScreen()->geometry();
        if (QScreen* screen = QGuiApplication::screenAt(globalPos)) {
            screenRect = screen->geometry();
        }

        int x = qBound(screenRect.left(), globalPos.x(), screenRect.right() - popup->width());
        int y = qBound(screenRect.top(), globalPos.y(), screenRect.bottom() - popup->height());

        popup->move(x, y);
        popup->show();
    });
}


void AudioItemWidget::setVolumeLevel(float level_dB)
{
    // Map -60 ~ 0 dB to 0 ~ 100
    float val = qBound(-60.0f, level_dB, 0.0f);
    m_levelBar->setLevel(val);
    // m_source->setVolume(level_dB);
}

void AudioItemWidget::setVolumeGain(double gain)
{
    float fGain = static_cast<float>(gain);
    if(!m_source->isMute()){
        lastGain_ = fGain;
    }
    m_levelBar->setVolumeGain(fGain);
    ui->dBLabel->setText(QString::number(fGain,'f',1));
    emit volumeGainChanged(fGain);
}


void AudioItemWidget::on_muteBtn_clicked()
{
    if(m_source->isMute()){
        m_source->unmute();
        ui->muteBtn->setIcon(QIcon(":/sources/volume_2.png"));
        emit volumeGainChanged(lastGain_);
        qDebug() << "lastGain_" << lastGain_;
    }
    else{
        m_source->mute();
        ui->muteBtn->setIcon(QIcon(":/sources/mute_red.png"));
        emit volumeGainChanged(-60.0);
        qDebug() << "-60.0";
    }
}


void AudioItemWidget::on_volumeSlider_valueChanged(int value)
{
    // 转化为double型
    double dValue = value / 10.0;

    if(dValue <= -60.0)
    {
        m_source->mute();
        ui->muteBtn->setIcon(QIcon(":/sources/mute_red.png"));
        return;
    }

    if(m_source->isMute())
    {
        m_source->unmute();
        ui->muteBtn->setIcon(QIcon(":/sources/volume_2.png"));
    }
    m_source->setVolume(dValue);
    float gain = static_cast<float>(dValue);
    lastGain_ = gain;
    m_levelBar->setVolumeGain(gain);
    ui->dBLabel->setText(QString::number(dValue, 'f', 1));
    emit volumeGainChanged(gain);
}

void AudioItemWidget::handleVolumeLockBtn(QPushButton *button)
{
    m_volumeLocked = !m_volumeLocked;

    // 禁用volumeSlider
    ui->volumeSlider->setEnabled(!m_volumeLocked);

    // 更新按钮文本和图标
    if(m_volumeLocked)
    {
        button->setText("取消锁定音量");
        button->setIcon(QIcon(":/sources/tick.png"));
        QString lockedStyle = R"(
                    QSlider::sub-page:horizontal {
                        border:none;
                        background: #3C404D;
                        border-radius: 3px;
                    }
                    QSlider::handle:horizontal {
                        background: #ADADAD;
                        border: none;
                        width: 20px;
                        height: 8px;
                        margin: -4px 0;
                        border-radius: 6px;
                    }
                )";
        ui->volumeSlider->setStyleSheet(lockedStyle);
    }else
    {
        button->setText("锁定音量");
        button->setIcon(QIcon());
        ui->volumeSlider->setStyleSheet(m_sliderOriginalStyle);
    }
}

void AudioItemWidget::handleRenameBtn(QPushButton *button)
{

    QString newName = ComponentInitializer::getInstance()->showCreateDialog("音频重命名","请输入名称",m_source->name(),this);
    if(newName.isEmpty()) return;

    m_source->rename(newName);
    ui->audioNameLabel->setText(m_source->name());

}

void AudioItemWidget::handleHideBtn(QPushButton *button)
{
    setHided(true);
}

void AudioItemWidget::handleSettingBtn(QPushButton *button)
{

}

void AudioItemWidget::handleMixertSettingBtn(QPushButton *button)
{
    AudioMixerDialog::getInstance()->initTableItem();
    AudioMixerDialog::getInstance()->show();
}

void AudioItemWidget::checkWidgetInContainer(const QString& stage)
{
    // 找到 ScrollableContainer（根据实际父级层级调整）
    ScrollableContainer* container = nullptr;
    QWidget* currentParent = parentWidget();
    while (currentParent) {
        container = qobject_cast<ScrollableContainer*>(currentParent);
        if (container) break;
        currentParent = currentParent->parentWidget();
    }

    if (!container) {
        qDebug() << stage << "：未找到 ScrollableContainer";
        return;
    }

    // 检查是否在 contentLayout 中
    QVBoxLayout* layout = container->contentLayout();
    bool isInLayout = false;
    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem* item = layout->itemAt(i);
        if (item->widget() == this) {
            isInLayout = true;
            break;
        }
    }

    qDebug() << stage << "：是否在布局中：" << isInLayout;
    qDebug() << stage << "：父对象是否有效：" << (parentWidget() != nullptr);
    qDebug() << stage << "：自身可见性：" << isVisible();
    qDebug() << stage << "：父部件可见性：" << (parentWidget() ? parentWidget()->isVisible() : false);
}

bool AudioItemWidget::isAudioItemWidget(QWidget *widget) {
    return qobject_cast<AudioItemWidget *>(widget) != nullptr;
}

