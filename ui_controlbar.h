/********************************************************************************
** Form generated from reading UI file 'controlbar.ui'
**
** Created by: Qt User Interface Compiler version 6.9.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CONTROLBAR_H
#define UI_CONTROLBAR_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ControlBar
{
public:
    QGridLayout *gridLayout;
    QPushButton *startBtn;
    QPushButton *stopBtn;
    QSlider *processSlider;
    QLabel *curTimeLabel;
    QLabel *label_3;
    QLabel *totalTimeLabel;

    void setupUi(QWidget *ControlBar)
    {
        if (ControlBar->objectName().isEmpty())
            ControlBar->setObjectName("ControlBar");
        ControlBar->resize(932, 30);
        ControlBar->setStyleSheet(QString::fromUtf8("QWidget{\n"
"	border-radius:3px;\n"
"	background-color:rgb(39, 42, 51);\n"
"	min-height:30px;\n"
"	max-height:30px;\n"
"}\n"
"\n"
"QLabel{\n"
"	color:white;\n"
"}\n"
"\n"
"QSlider::groove:horizontal {\n"
"    border: none;\n"
"    height: 6px;\n"
"    background: #3c3f41;\n"
"    border-radius: 3px;\n"
"}\n"
"\n"
"QSlider::handle:horizontal {\n"
"    background: white;         /* \346\273\221\345\235\227\351\242\234\350\211\262\357\274\210\345\217\257\350\207\252\345\256\232\344\271\211\344\270\272 OBS \351\253\230\344\272\256\350\211\262\357\274\211 */\n"
"    border: none;\n"
"    width: 22px;\n"
"    height: 12px;\n"
"    margin: -4px 0;              /* \345\236\202\347\233\264\345\261\205\344\270\255 handle */\n"
"    border-radius: 6px;\n"
"}\n"
"\n"
"QSlider::sub-page:horizontal {\n"
"    background: #476BD7;         /* \350\277\233\345\272\246\345\241\253\345\205\205\351\242\234\350\211\262 */\n"
"    border-radius: 3px;\n"
"}\n"
"\n"
"QSlider::add-page:horizontal {\n"
"    background: #464B59;         "
                        "/* \346\234\252\345\241\253\345\205\205\351\203\250\345\210\206 */\n"
"    border-radius: 3px;\n"
"}\n"
"\n"
"QPushButton{\n"
"	min-width:35px;\n"
"	min-height:28px;\n"
"	max-width:35px;\n"
"	max-height:28px;\n"
"	background-color:#464B59\n"
"}"));
        gridLayout = new QGridLayout(ControlBar);
        gridLayout->setObjectName("gridLayout");
        gridLayout->setContentsMargins(-1, 0, -1, 0);
        startBtn = new QPushButton(ControlBar);
        startBtn->setObjectName("startBtn");
        startBtn->setStyleSheet(QString::fromUtf8(""));
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/sources/play.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        startBtn->setIcon(icon);
        startBtn->setIconSize(QSize(20, 20));

        gridLayout->addWidget(startBtn, 0, 0, 1, 1);

        stopBtn = new QPushButton(ControlBar);
        stopBtn->setObjectName("stopBtn");
        stopBtn->setStyleSheet(QString::fromUtf8("QPushButton{\n"
"	min-width:35px;\n"
"	min-height:28px;\n"
"	max-width:35px;\n"
"	max-height:28px;\n"
"	background-color:#464B59\n"
"}"));
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/sources/stop.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        stopBtn->setIcon(icon1);
        stopBtn->setIconSize(QSize(20, 20));

        gridLayout->addWidget(stopBtn, 0, 1, 1, 1);

        processSlider = new QSlider(ControlBar);
        processSlider->setObjectName("processSlider");
        processSlider->setOrientation(Qt::Orientation::Horizontal);

        gridLayout->addWidget(processSlider, 0, 2, 1, 1);

        curTimeLabel = new QLabel(ControlBar);
        curTimeLabel->setObjectName("curTimeLabel");

        gridLayout->addWidget(curTimeLabel, 0, 3, 1, 1);

        label_3 = new QLabel(ControlBar);
        label_3->setObjectName("label_3");

        gridLayout->addWidget(label_3, 0, 4, 1, 1);

        totalTimeLabel = new QLabel(ControlBar);
        totalTimeLabel->setObjectName("totalTimeLabel");

        gridLayout->addWidget(totalTimeLabel, 0, 5, 1, 1);


        retranslateUi(ControlBar);

        QMetaObject::connectSlotsByName(ControlBar);
    } // setupUi

    void retranslateUi(QWidget *ControlBar)
    {
        ControlBar->setWindowTitle(QCoreApplication::translate("ControlBar", "Form", nullptr));
        startBtn->setText(QString());
        stopBtn->setText(QString());
        curTimeLabel->setText(QCoreApplication::translate("ControlBar", "00:00:00", nullptr));
        label_3->setText(QCoreApplication::translate("ControlBar", "/", nullptr));
        totalTimeLabel->setText(QCoreApplication::translate("ControlBar", "00:00:00", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ControlBar: public Ui_ControlBar {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CONTROLBAR_H
