/********************************************************************************
** Form generated from reading UI file 'audioitemwidget.ui'
**
** Created by: Qt User Interface Compiler version 6.9.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_AUDIOITEMWIDGET_H
#define UI_AUDIOITEMWIDGET_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_AudioItemWidget
{
public:
    QGridLayout *gridLayout;
    QHBoxLayout *hlayout1;
    QLabel *audioNameLabel;
    QSpacerItem *horizontalSpacer;
    QLabel *dBLabel;
    QLabel *label;
    QHBoxLayout *hlayout2;
    QPushButton *audioSettingBtn;
    QHBoxLayout *hlayout3;
    QPushButton *muteBtn;
    QSlider *volumeSlider;

    void setupUi(QWidget *AudioItemWidget)
    {
        if (AudioItemWidget->objectName().isEmpty())
            AudioItemWidget->setObjectName("AudioItemWidget");
        AudioItemWidget->resize(435, 86);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(AudioItemWidget->sizePolicy().hasHeightForWidth());
        AudioItemWidget->setSizePolicy(sizePolicy);
        AudioItemWidget->setStyleSheet(QString::fromUtf8("QWidget{\n"
"	background-color: #1E1F29;\n"
"	border:none;\n"
"	\n"
"}\n"
"QSlider{\n"
"	border:none;;\n"
"}\n"
"QSlider::groove:horizontal {\n"
"    border: none;\n"
"    height: 5px;\n"
"    background: #3c3f41;\n"
"    border-radius: 3px;\n"
"}\n"
"\n"
"QSlider::handle:horizontal {\n"
"    background: white;         /* \346\273\221\345\235\227\351\242\234\350\211\262\357\274\210\345\217\257\350\207\252\345\256\232\344\271\211\344\270\272 OBS \351\253\230\344\272\256\350\211\262\357\274\211 */\n"
"    border: none;\n"
"    width: 20px;\n"
"    height: 8px;\n"
"    margin: -4px 0;              /* \345\236\202\347\233\264\345\261\205\344\270\255 handle */\n"
"    border-radius: 6px;\n"
"}\n"
"\n"
"QSlider::sub-page:horizontal {\n"
"	border:none;\n"
"    background: #476BD7;         /* \350\277\233\345\272\246\345\241\253\345\205\205\351\242\234\350\211\262 */\n"
"    border-radius: 3px;\n"
"}\n"
"\n"
"QSlider::add-page:horizontal {\n"
"	border:none;\n"
"    background: #464B59;         /* \346\234\252\345\241\253"
                        "\345\205\205\351\203\250\345\210\206 */\n"
"    border-radius: 3px;\n"
"}\n"
"\n"
"\n"
""));
        gridLayout = new QGridLayout(AudioItemWidget);
        gridLayout->setSpacing(1);
        gridLayout->setObjectName("gridLayout");
        gridLayout->setContentsMargins(0, 0, 0, 1);
        hlayout1 = new QHBoxLayout();
        hlayout1->setSpacing(0);
        hlayout1->setObjectName("hlayout1");
        audioNameLabel = new QLabel(AudioItemWidget);
        audioNameLabel->setObjectName("audioNameLabel");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(audioNameLabel->sizePolicy().hasHeightForWidth());
        audioNameLabel->setSizePolicy(sizePolicy1);
        audioNameLabel->setMinimumSize(QSize(0, 0));
        audioNameLabel->setMaximumSize(QSize(16777215, 16777215));
        audioNameLabel->setStyleSheet(QString::fromUtf8("QLabel{\n"
"    color: white;\n"
"	padding:0px; \n"
"	margin:0px;\n"
"	border:none;\n"
"}"));

        hlayout1->addWidget(audioNameLabel);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        hlayout1->addItem(horizontalSpacer);

        dBLabel = new QLabel(AudioItemWidget);
        dBLabel->setObjectName("dBLabel");
        sizePolicy1.setHeightForWidth(dBLabel->sizePolicy().hasHeightForWidth());
        dBLabel->setSizePolicy(sizePolicy1);
        dBLabel->setMinimumSize(QSize(17, 0));
        dBLabel->setMaximumSize(QSize(16777215, 16777215));
        dBLabel->setStyleSheet(QString::fromUtf8("QLabel{\n"
"	color:#999999;\n"
"	padding:0px; \n"
"	margin:0px;\n"
"	border:none;\n"
"}"));

        hlayout1->addWidget(dBLabel);

        label = new QLabel(AudioItemWidget);
        label->setObjectName("label");
        QSizePolicy sizePolicy2(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Fixed);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(label->sizePolicy().hasHeightForWidth());
        label->setSizePolicy(sizePolicy2);
        label->setStyleSheet(QString::fromUtf8("QLabel{\n"
"	color:#999999;\n"
"	padding:0px; \n"
"	margin:0px;	\n"
"	border:none;\n"
"}"));

        hlayout1->addWidget(label);


        gridLayout->addLayout(hlayout1, 0, 0, 1, 1);

        hlayout2 = new QHBoxLayout();
        hlayout2->setObjectName("hlayout2");
        audioSettingBtn = new QPushButton(AudioItemWidget);
        audioSettingBtn->setObjectName("audioSettingBtn");
        audioSettingBtn->setStyleSheet(QString::fromUtf8("QPushButton{\n"
"	min-width:24px;\n"
"	min-height:24px;\n"
"	max-width:24px;\n"
"	max-height:24px;\n"
"	border:none;\n"
"	border-radius:3px;\n"
"	background-color:#3C404D\n"
"}\n"
"\n"
"QPushButton:hover{\n"
"	background-color:#4c5261;\n"
"}"));
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/sources/three_points.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        audioSettingBtn->setIcon(icon);
        audioSettingBtn->setIconSize(QSize(20, 20));

        hlayout2->addWidget(audioSettingBtn);


        gridLayout->addLayout(hlayout2, 1, 0, 1, 1);

        hlayout3 = new QHBoxLayout();
        hlayout3->setSpacing(5);
        hlayout3->setObjectName("hlayout3");
        muteBtn = new QPushButton(AudioItemWidget);
        muteBtn->setObjectName("muteBtn");
        muteBtn->setStyleSheet(QString::fromUtf8("QPushButton{\n"
"	min-width:24px;\n"
"	min-height:24px;\n"
"	max-width:24px;\n"
"	max-height:24px;\n"
"	border:none;\n"
"	border-radius:3px;\n"
"	background-color:#3C404D\n"
"}\n"
"\n"
"QPushButton:hover{\n"
"	background-color:#4c5261;\n"
"}"));
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/sources/volume_2.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        muteBtn->setIcon(icon1);
        muteBtn->setIconSize(QSize(20, 20));

        hlayout3->addWidget(muteBtn);

        volumeSlider = new QSlider(AudioItemWidget);
        volumeSlider->setObjectName("volumeSlider");
        volumeSlider->setOrientation(Qt::Orientation::Horizontal);

        hlayout3->addWidget(volumeSlider);


        gridLayout->addLayout(hlayout3, 2, 0, 1, 1);


        retranslateUi(AudioItemWidget);

        QMetaObject::connectSlotsByName(AudioItemWidget);
    } // setupUi

    void retranslateUi(QWidget *AudioItemWidget)
    {
        AudioItemWidget->setWindowTitle(QCoreApplication::translate("AudioItemWidget", "Form", nullptr));
        audioNameLabel->setText(QCoreApplication::translate("AudioItemWidget", "\351\272\246\345\205\213\351\243\216", nullptr));
        dBLabel->setText(QCoreApplication::translate("AudioItemWidget", "0.0", nullptr));
        label->setText(QCoreApplication::translate("AudioItemWidget", "dB", nullptr));
        audioSettingBtn->setText(QString());
        muteBtn->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class AudioItemWidget: public Ui_AudioItemWidget {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_AUDIOITEMWIDGET_H
