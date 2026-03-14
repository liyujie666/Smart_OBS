/********************************************************************************
** Form generated from reading UI file 'outputsettingdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.9.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_OUTPUTSETTINGDIALOG_H
#define UI_OUTPUTSETTINGDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_outputSettingDialog
{
public:
    QVBoxLayout *verticalLayout_5;
    QWidget *widget;
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout;
    QLabel *label;
    QSpacerItem *horizontalSpacer;
    QHBoxLayout *horizontalLayout_2;
    QLabel *pushUrlLabel;
    QLineEdit *pushUrl;
    QWidget *widget_2;
    QVBoxLayout *verticalLayout_2;
    QHBoxLayout *horizontalLayout_3;
    QLabel *label_3;
    QSpacerItem *horizontalSpacer_2;
    QHBoxLayout *horizontalLayout_4;
    QLabel *label_4;
    QLineEdit *recordPath;
    QPushButton *changePathBtn;
    QHBoxLayout *horizontalLayout_9;
    QLabel *label_9;
    QComboBox *videoFormatCBBox;
    QWidget *widget_3;
    QVBoxLayout *verticalLayout_3;
    QHBoxLayout *horizontalLayout_5;
    QLabel *label_5;
    QSpacerItem *horizontalSpacer_3;
    QHBoxLayout *horizontalLayout_6;
    QLabel *label_6;
    QComboBox *resolutionCBBox;
    QHBoxLayout *horizontalLayout_10;
    QLabel *label_10;
    QSpinBox *frameRate;
    QHBoxLayout *horizontalLayout_11;
    QLabel *label_11;
    QSpinBox *videoBitRate;
    QHBoxLayout *horizontalLayout_12;
    QLabel *label_12;
    QSpinBox *gop;
    QHBoxLayout *horizontalLayout_13;
    QLabel *label_13;
    QSpinBox *bframe;
    QWidget *widget_4;
    QVBoxLayout *verticalLayout_4;
    QHBoxLayout *horizontalLayout_7;
    QLabel *label_7;
    QSpacerItem *horizontalSpacer_4;
    QHBoxLayout *horizontalLayout_8;
    QLabel *label_8;
    QComboBox *sampleRateCBBox;
    QHBoxLayout *horizontalLayout_15;
    QLabel *label_15;
    QComboBox *audioBitRateCBBox;
    QHBoxLayout *horizontalLayout_14;
    QLabel *label_14;
    QComboBox *channelsCBBox;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *outputSettingDialog)
    {
        if (outputSettingDialog->objectName().isEmpty())
            outputSettingDialog->setObjectName("outputSettingDialog");
        outputSettingDialog->resize(776, 714);
        outputSettingDialog->setStyleSheet(QString::fromUtf8("QDialog{\n"
"	background-color:#1D1F26\n"
"}\n"
"\n"
"\n"
"QLabel{\n"
"	color:white;\n"
"	font-size:14px;\n"
"	min-width:100px;\n"
"	max-width:100px;\n"
"}\n"
"\n"
"QLineEdit{\n"
"	 background-color: #3C404D;\n"
"    border: 1px solid #3C3C3C;\n"
"    border-radius: 5px;\n"
"    padding: 4px 8px;\n"
"    color: white;\n"
"    font-size: 13px;\n"
"	min-height:25px;\n"
"	max-height:25px;\n"
"\n"
"}\n"
"QComboBox {\n"
"    background-color: #3C404D;\n"
"    border: 1px solid #3C3C3C;\n"
"    border-radius: 5px;\n"
"    padding: 4px 8px;\n"
"    color: white;\n"
"    font-size: 13px;\n"
"	min-height:25px;\n"
"	max-height:25px;\n"
"}\n"
"\n"
"QComboBox::drop-down {\n"
"    subcontrol-origin: padding;\n"
"    subcontrol-position: top right;\n"
"    width: 20px;\n"
"    border-left: 1px solid #3C3C3C;\n"
"    background-color: #3C404D;\n"
"}\n"
"\n"
"QComboBox::down-arrow {\n"
"    image: url(:/sources/down.png);  /* \344\275\240\345\217\257\344\273\245\347\224\250\350\207\252\345\267\261\347\232\204\347\256\255\345\244"
                        "\264\345\233\276\346\240\207 */\n"
"	background-color: #3C404D;\n"
"    width: 12px;\n"
"    height: 12px;\n"
"}\n"
"\n"
"QComboBox QAbstractItemView {\n"
"    background-color: #3C404D;\n"
"    border: 1px solid #3C3C3C;\n"
"    selection-background-color: #284CB8;\n"
"    selection-color: white;\n"
"    color: white;\n"
"    font-size: 13px;\n"
"    outline: 0;\n"
"}\n"
"\n"
"QPushButton {\n"
"        background-color: #3C404D;\n"
"        border: none;\n"
"         border-radius: 4px;\n"
"         padding: 6px 12px;\n"
"         color: white;\n"
"		min-width:50px;\n"
"		max-width:50px;\n"
"		min-height:25px;\n"
"		max-height:25px;\n"
"\n"
"}\n"
"QPushButton:hover { background-color: #3c3d47; }\n"
"QPushButton:pressed { background-color: #50515b; }\n"
"\n"
"\n"
"QSpinBox {\n"
"    background-color: #3C404D;\n"
"    border: 1px solid #3C3C3C;\n"
"    border-radius: 5px;\n"
"    padding: 4px 8px;\n"
"    color: white;\n"
"    font-size: 13px;\n"
"    min-height: 25px;\n"
"    max-height: 25px;\n"
"}\n"
"\n"
"/"
                        "* \345\216\273\351\231\244\350\276\271\346\241\206\351\230\264\345\275\261\347\255\211\351\273\230\350\256\244\346\225\210\346\236\234 */\n"
"QSpinBox:focus {\n"
"    border: 1px solid #284CB8;\n"
"}\n"
"\n"
"/* \344\270\212\344\270\213\346\214\211\351\222\256\345\214\272\345\237\237 */\n"
"QSpinBox::up-button, QSpinBox::down-button {\n"
"    background-color: #3C404D;\n"
"    subcontrol-origin: border;\n"
"    width: 20px;\n"
"    border-left: 1px solid #3C3C3C;\n"
"}\n"
"\n"
"/* \344\270\212\346\214\211\351\222\256\345\233\276\346\240\207 */\n"
"QSpinBox::up-arrow {\n"
"    image: url(:/sources/up.png); /* \344\275\277\347\224\250\344\275\240\350\207\252\345\267\261\347\232\204\345\233\276\346\240\207 */\n"
"    width: 12px;\n"
"    height: 12px;\n"
"}\n"
"\n"
"/* \344\270\213\346\214\211\351\222\256\345\233\276\346\240\207 */\n"
"QSpinBox::down-arrow {\n"
"    image: url(:/sources/down.png); /* \344\275\277\347\224\250\344\275\240\350\207\252\345\267\261\347\232\204\345\233\276\346\240\207 */\n"
"    width:"
                        " 12px;\n"
"    height: 12px;\n"
"}\n"
"\n"
"/* \345\205\263\351\227\255\344\270\212\344\270\213\346\214\211\351\222\256\346\214\211\344\270\213\346\227\266\347\232\204\351\253\230\344\272\256 */\n"
"QSpinBox::up-button:pressed,\n"
"QSpinBox::down-button:pressed {\n"
"    background-color: #50555E;\n"
"}\n"
"\n"
"/* \345\216\273\351\231\244\350\276\223\345\205\245\347\204\246\347\202\271\350\276\271\346\241\206 */\n"
"QSpinBox::edit-field {\n"
"    background: transparent;\n"
"    padding-right: 24px; /* \347\273\231\346\214\211\351\222\256\347\225\231\347\251\272\351\227\264 */\n"
"}"));
        verticalLayout_5 = new QVBoxLayout(outputSettingDialog);
        verticalLayout_5->setObjectName("verticalLayout_5");
        widget = new QWidget(outputSettingDialog);
        widget->setObjectName("widget");
        widget->setStyleSheet(QString::fromUtf8("QWidget{\n"
"	background-color:#272A33;\n"
"	border-radius:5px;\n"
"}"));
        verticalLayout = new QVBoxLayout(widget);
        verticalLayout->setObjectName("verticalLayout");
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        label = new QLabel(widget);
        label->setObjectName("label");

        horizontalLayout->addWidget(label);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);


        verticalLayout->addLayout(horizontalLayout);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        pushUrlLabel = new QLabel(widget);
        pushUrlLabel->setObjectName("pushUrlLabel");
        pushUrlLabel->setLayoutDirection(Qt::LayoutDirection::LeftToRight);
        pushUrlLabel->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_2->addWidget(pushUrlLabel);

        pushUrl = new QLineEdit(widget);
        pushUrl->setObjectName("pushUrl");

        horizontalLayout_2->addWidget(pushUrl);


        verticalLayout->addLayout(horizontalLayout_2);


        verticalLayout_5->addWidget(widget);

        widget_2 = new QWidget(outputSettingDialog);
        widget_2->setObjectName("widget_2");
        widget_2->setStyleSheet(QString::fromUtf8("QWidget{\n"
"	background-color:#272A33;\n"
"	border-radius:5px;\n"
"}"));
        verticalLayout_2 = new QVBoxLayout(widget_2);
        verticalLayout_2->setObjectName("verticalLayout_2");
        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName("horizontalLayout_3");
        label_3 = new QLabel(widget_2);
        label_3->setObjectName("label_3");

        horizontalLayout_3->addWidget(label_3);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_3->addItem(horizontalSpacer_2);


        verticalLayout_2->addLayout(horizontalLayout_3);

        horizontalLayout_4 = new QHBoxLayout();
        horizontalLayout_4->setObjectName("horizontalLayout_4");
        label_4 = new QLabel(widget_2);
        label_4->setObjectName("label_4");
        label_4->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_4->addWidget(label_4);

        recordPath = new QLineEdit(widget_2);
        recordPath->setObjectName("recordPath");

        horizontalLayout_4->addWidget(recordPath);

        changePathBtn = new QPushButton(widget_2);
        changePathBtn->setObjectName("changePathBtn");
        changePathBtn->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"        background-color: #3C404D;\n"
"        border: none;\n"
"         border-radius: 4px;\n"
"         padding: 6px 12px;\n"
"         color: white;\n"
"		min-width:50px;\n"
"		max-width:50px;\n"
"		min-height:25px;\n"
"		max-height:25px;\n"
"\n"
"}\n"
"QPushButton:hover { background-color: #3c3d47; }\n"
"QPushButton:pressed { background-color: #50515b; }"));

        horizontalLayout_4->addWidget(changePathBtn);


        verticalLayout_2->addLayout(horizontalLayout_4);

        horizontalLayout_9 = new QHBoxLayout();
        horizontalLayout_9->setObjectName("horizontalLayout_9");
        label_9 = new QLabel(widget_2);
        label_9->setObjectName("label_9");
        label_9->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_9->addWidget(label_9);

        videoFormatCBBox = new QComboBox(widget_2);
        videoFormatCBBox->addItem(QString());
        videoFormatCBBox->addItem(QString());
        videoFormatCBBox->addItem(QString());
        videoFormatCBBox->setObjectName("videoFormatCBBox");

        horizontalLayout_9->addWidget(videoFormatCBBox);


        verticalLayout_2->addLayout(horizontalLayout_9);


        verticalLayout_5->addWidget(widget_2);

        widget_3 = new QWidget(outputSettingDialog);
        widget_3->setObjectName("widget_3");
        widget_3->setStyleSheet(QString::fromUtf8("QWidget{\n"
"	background-color:#272A33;\n"
"	border-radius:5px;\n"
"}"));
        verticalLayout_3 = new QVBoxLayout(widget_3);
        verticalLayout_3->setObjectName("verticalLayout_3");
        horizontalLayout_5 = new QHBoxLayout();
        horizontalLayout_5->setObjectName("horizontalLayout_5");
        label_5 = new QLabel(widget_3);
        label_5->setObjectName("label_5");

        horizontalLayout_5->addWidget(label_5);

        horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_5->addItem(horizontalSpacer_3);


        verticalLayout_3->addLayout(horizontalLayout_5);

        horizontalLayout_6 = new QHBoxLayout();
        horizontalLayout_6->setObjectName("horizontalLayout_6");
        label_6 = new QLabel(widget_3);
        label_6->setObjectName("label_6");
        label_6->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_6->addWidget(label_6);

        resolutionCBBox = new QComboBox(widget_3);
        resolutionCBBox->setObjectName("resolutionCBBox");

        horizontalLayout_6->addWidget(resolutionCBBox);


        verticalLayout_3->addLayout(horizontalLayout_6);

        horizontalLayout_10 = new QHBoxLayout();
        horizontalLayout_10->setObjectName("horizontalLayout_10");
        label_10 = new QLabel(widget_3);
        label_10->setObjectName("label_10");
        label_10->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_10->addWidget(label_10);

        frameRate = new QSpinBox(widget_3);
        frameRate->setObjectName("frameRate");
        frameRate->setMaximum(60);
        frameRate->setValue(30);

        horizontalLayout_10->addWidget(frameRate);


        verticalLayout_3->addLayout(horizontalLayout_10);

        horizontalLayout_11 = new QHBoxLayout();
        horizontalLayout_11->setObjectName("horizontalLayout_11");
        label_11 = new QLabel(widget_3);
        label_11->setObjectName("label_11");
        label_11->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_11->addWidget(label_11);

        videoBitRate = new QSpinBox(widget_3);
        videoBitRate->setObjectName("videoBitRate");
        videoBitRate->setMaximum(10000);
        videoBitRate->setSingleStep(50);
        videoBitRate->setValue(2500);

        horizontalLayout_11->addWidget(videoBitRate);


        verticalLayout_3->addLayout(horizontalLayout_11);

        horizontalLayout_12 = new QHBoxLayout();
        horizontalLayout_12->setObjectName("horizontalLayout_12");
        label_12 = new QLabel(widget_3);
        label_12->setObjectName("label_12");
        label_12->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_12->addWidget(label_12);

        gop = new QSpinBox(widget_3);
        gop->setObjectName("gop");
        gop->setMaximum(200);
        gop->setValue(60);

        horizontalLayout_12->addWidget(gop);


        verticalLayout_3->addLayout(horizontalLayout_12);

        horizontalLayout_13 = new QHBoxLayout();
        horizontalLayout_13->setObjectName("horizontalLayout_13");
        label_13 = new QLabel(widget_3);
        label_13->setObjectName("label_13");
        label_13->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_13->addWidget(label_13);

        bframe = new QSpinBox(widget_3);
        bframe->setObjectName("bframe");

        horizontalLayout_13->addWidget(bframe);


        verticalLayout_3->addLayout(horizontalLayout_13);


        verticalLayout_5->addWidget(widget_3);

        widget_4 = new QWidget(outputSettingDialog);
        widget_4->setObjectName("widget_4");
        widget_4->setStyleSheet(QString::fromUtf8("QWidget{\n"
"	background-color:#272A33;\n"
"	border-radius:5px;\n"
"}"));
        verticalLayout_4 = new QVBoxLayout(widget_4);
        verticalLayout_4->setObjectName("verticalLayout_4");
        horizontalLayout_7 = new QHBoxLayout();
        horizontalLayout_7->setObjectName("horizontalLayout_7");
        label_7 = new QLabel(widget_4);
        label_7->setObjectName("label_7");

        horizontalLayout_7->addWidget(label_7);

        horizontalSpacer_4 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_7->addItem(horizontalSpacer_4);


        verticalLayout_4->addLayout(horizontalLayout_7);

        horizontalLayout_8 = new QHBoxLayout();
        horizontalLayout_8->setObjectName("horizontalLayout_8");
        label_8 = new QLabel(widget_4);
        label_8->setObjectName("label_8");
        label_8->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_8->addWidget(label_8);

        sampleRateCBBox = new QComboBox(widget_4);
        sampleRateCBBox->setObjectName("sampleRateCBBox");

        horizontalLayout_8->addWidget(sampleRateCBBox);


        verticalLayout_4->addLayout(horizontalLayout_8);

        horizontalLayout_15 = new QHBoxLayout();
        horizontalLayout_15->setObjectName("horizontalLayout_15");
        label_15 = new QLabel(widget_4);
        label_15->setObjectName("label_15");
        label_15->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_15->addWidget(label_15);

        audioBitRateCBBox = new QComboBox(widget_4);
        audioBitRateCBBox->setObjectName("audioBitRateCBBox");

        horizontalLayout_15->addWidget(audioBitRateCBBox);


        verticalLayout_4->addLayout(horizontalLayout_15);

        horizontalLayout_14 = new QHBoxLayout();
        horizontalLayout_14->setObjectName("horizontalLayout_14");
        label_14 = new QLabel(widget_4);
        label_14->setObjectName("label_14");
        label_14->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_14->addWidget(label_14);

        channelsCBBox = new QComboBox(widget_4);
        channelsCBBox->addItem(QString());
        channelsCBBox->setObjectName("channelsCBBox");

        horizontalLayout_14->addWidget(channelsCBBox);


        verticalLayout_4->addLayout(horizontalLayout_14);


        verticalLayout_5->addWidget(widget_4);

        buttonBox = new QDialogButtonBox(outputSettingDialog);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setOrientation(Qt::Orientation::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Cancel|QDialogButtonBox::StandardButton::Ok);

        verticalLayout_5->addWidget(buttonBox);


        retranslateUi(outputSettingDialog);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, outputSettingDialog, qOverload<>(&QDialog::accept));
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, outputSettingDialog, qOverload<>(&QDialog::reject));

        QMetaObject::connectSlotsByName(outputSettingDialog);
    } // setupUi

    void retranslateUi(QDialog *outputSettingDialog)
    {
        outputSettingDialog->setWindowTitle(QCoreApplication::translate("outputSettingDialog", "Dialog", nullptr));
        label->setText(QCoreApplication::translate("outputSettingDialog", "\346\216\250\346\265\201", nullptr));
        pushUrlLabel->setText(QCoreApplication::translate("outputSettingDialog", "\346\216\250\346\265\201\345\234\260\345\235\200", nullptr));
        pushUrl->setText(QString());
        label_3->setText(QCoreApplication::translate("outputSettingDialog", "\345\275\225\345\210\266", nullptr));
        label_4->setText(QCoreApplication::translate("outputSettingDialog", "\344\277\235\345\255\230\345\234\260\345\235\200", nullptr));
        recordPath->setText(QString());
        changePathBtn->setText(QCoreApplication::translate("outputSettingDialog", "\344\277\256\346\224\271", nullptr));
        label_9->setText(QCoreApplication::translate("outputSettingDialog", "\350\247\206\351\242\221\346\240\274\345\274\217", nullptr));
        videoFormatCBBox->setItemText(0, QCoreApplication::translate("outputSettingDialog", "mp4", nullptr));
        videoFormatCBBox->setItemText(1, QCoreApplication::translate("outputSettingDialog", "mkv", nullptr));
        videoFormatCBBox->setItemText(2, QCoreApplication::translate("outputSettingDialog", "flv", nullptr));

        label_5->setText(QCoreApplication::translate("outputSettingDialog", "\350\247\206\351\242\221", nullptr));
        label_6->setText(QCoreApplication::translate("outputSettingDialog", "\345\210\206\350\276\250\347\216\207", nullptr));
        label_10->setText(QCoreApplication::translate("outputSettingDialog", "\345\270\247\347\216\207", nullptr));
        frameRate->setSuffix(QCoreApplication::translate("outputSettingDialog", " fps", nullptr));
        label_11->setText(QCoreApplication::translate("outputSettingDialog", "\347\240\201\347\216\207", nullptr));
        videoBitRate->setSuffix(QCoreApplication::translate("outputSettingDialog", " Kbps", nullptr));
        videoBitRate->setPrefix(QString());
        label_12->setText(QCoreApplication::translate("outputSettingDialog", "GOP", nullptr));
        label_13->setText(QCoreApplication::translate("outputSettingDialog", "B\345\270\247", nullptr));
        label_7->setText(QCoreApplication::translate("outputSettingDialog", "\351\237\263\351\242\221", nullptr));
        label_8->setText(QCoreApplication::translate("outputSettingDialog", "\351\207\207\346\240\267\347\216\207", nullptr));
        sampleRateCBBox->setCurrentText(QString());
        label_15->setText(QCoreApplication::translate("outputSettingDialog", "\347\240\201\347\216\207", nullptr));
        label_14->setText(QCoreApplication::translate("outputSettingDialog", "\345\243\260\351\201\223\346\225\260", nullptr));
        channelsCBBox->setItemText(0, QCoreApplication::translate("outputSettingDialog", "2", nullptr));

    } // retranslateUi

};

namespace Ui {
    class outputSettingDialog: public Ui_outputSettingDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_OUTPUTSETTINGDIALOG_H
