/********************************************************************************
** Form generated from reading UI file 'textsettingdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.9.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TEXTSETTINGDIALOG_H
#define UI_TEXTSETTINGDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_TextSettingDialog
{
public:
    QGridLayout *gridLayout_2;
    QWidget *textContanier;
    QGridLayout *gridLayout;
    QSpacerItem *horizontalSpacer;
    QWidget *textWidget;
    QGridLayout *gridLayout_3;
    QLabel *previewLabel;
    QSpacerItem *horizontalSpacer_2;
    QWidget *widget;
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout;
    QLabel *label;
    QLabel *fontNameLabel;
    QPushButton *chooseFontBtn;
    QHBoxLayout *horizontalLayout_2;
    QLabel *label_3;
    QTextEdit *textEdit;
    QHBoxLayout *horizontalLayout_3;
    QLabel *label_4;
    QComboBox *transformCBBOX;
    QHBoxLayout *horizontalLayout_4;
    QLabel *label_5;
    QLabel *colorLabel;
    QPushButton *chooseColorBtn;
    QHBoxLayout *horizontalLayout_5;
    QLabel *label_10;
    QSlider *colorTransparencySlider;
    QLabel *colorPercentLabel;
    QHBoxLayout *horizontalLayout_6;
    QLabel *label_6;
    QLabel *bgColorLabel;
    QPushButton *chooseBgColorBtn;
    QHBoxLayout *horizontalLayout_7;
    QLabel *label_2;
    QSlider *bgColorTransparencySlider;
    QLabel *bgColorPercentLabel;
    QHBoxLayout *horizontalLayout_8;
    QLabel *label_8;
    QComboBox *horizonAlignmentCBBox;
    QHBoxLayout *horizontalLayout_9;
    QLabel *label_9;
    QComboBox *verticalAlignmentCBBox;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *TextSettingDialog)
    {
        if (TextSettingDialog->objectName().isEmpty())
            TextSettingDialog->setObjectName("TextSettingDialog");
        TextSettingDialog->resize(1098, 905);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(TextSettingDialog->sizePolicy().hasHeightForWidth());
        TextSettingDialog->setSizePolicy(sizePolicy);
        TextSettingDialog->setStyleSheet(QString::fromUtf8("QDialog{\n"
"	background-color:#1d1f26;\n"
"}\n"
"QLabel{\n"
"	color:white;\n"
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
"    image: url(:/sources/down.png);  /* \344\275\240\345\217\257\344\273\245\347\224\250\350\207\252\345\267\261\347\232\204\347\256\255\345\244\264\345\233\276\346\240\207 */\n"
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
"    color: w"
                        "hite;\n"
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
"		\n"
"\n"
"}\n"
"QPushButton:hover { background-color: #3c3d47; }\n"
"QPushButton:pressed { background-color: #50515b; }\n"
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
"    width: 20px;\n"
"    height: 12px;\n"
"    margin: -4px 0;              /* \345\236\202\347\233\264\345\261\205\344\270\255 handle */\n"
"    border-radius: 6px;\n"
"}\n"
"\n"
"QSlider::sub-page:horizontal {\n"
"    background: #476BD7"
                        ";         /* \350\277\233\345\272\246\345\241\253\345\205\205\351\242\234\350\211\262 */\n"
"    border-radius: 3px;\n"
"}\n"
"\n"
"QSlider::add-page:horizontal {\n"
"    background: #464B59;         /* \346\234\252\345\241\253\345\205\205\351\203\250\345\210\206 */\n"
"    border-radius: 3px;\n"
"}\n"
"\n"
""));
        gridLayout_2 = new QGridLayout(TextSettingDialog);
        gridLayout_2->setObjectName("gridLayout_2");
        textContanier = new QWidget(TextSettingDialog);
        textContanier->setObjectName("textContanier");
        textContanier->setStyleSheet(QString::fromUtf8("QWidget{\n"
"	background-color:#1d1f26;\n"
"}"));
        gridLayout = new QGridLayout(textContanier);
        gridLayout->setObjectName("gridLayout");
        horizontalSpacer = new QSpacerItem(80, 20, QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Minimum);

        gridLayout->addItem(horizontalSpacer, 0, 0, 1, 1);

        textWidget = new QWidget(textContanier);
        textWidget->setObjectName("textWidget");
        sizePolicy.setHeightForWidth(textWidget->sizePolicy().hasHeightForWidth());
        textWidget->setSizePolicy(sizePolicy);
        textWidget->setStyleSheet(QString::fromUtf8("QWidget{\n"
"	background-color:black;\n"
"\n"
"}"));
        gridLayout_3 = new QGridLayout(textWidget);
        gridLayout_3->setObjectName("gridLayout_3");
        previewLabel = new QLabel(textWidget);
        previewLabel->setObjectName("previewLabel");
        sizePolicy.setHeightForWidth(previewLabel->sizePolicy().hasHeightForWidth());
        previewLabel->setSizePolicy(sizePolicy);

        gridLayout_3->addWidget(previewLabel, 0, 0, 1, 1);


        gridLayout->addWidget(textWidget, 0, 1, 1, 1);

        horizontalSpacer_2 = new QSpacerItem(80, 20, QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Minimum);

        gridLayout->addItem(horizontalSpacer_2, 0, 2, 1, 1);


        gridLayout_2->addWidget(textContanier, 0, 0, 1, 1);

        widget = new QWidget(TextSettingDialog);
        widget->setObjectName("widget");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(widget->sizePolicy().hasHeightForWidth());
        widget->setSizePolicy(sizePolicy1);
        verticalLayout = new QVBoxLayout(widget);
        verticalLayout->setObjectName("verticalLayout");
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        label = new QLabel(widget);
        label->setObjectName("label");
        label->setStyleSheet(QString::fromUtf8("QLabel{\n"
"	min-width:100px;\n"
"	max-width:100px;\n"
"font-size:14px;\n"
"}"));
        label->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout->addWidget(label);

        fontNameLabel = new QLabel(widget);
        fontNameLabel->setObjectName("fontNameLabel");
        fontNameLabel->setStyleSheet(QString::fromUtf8("QLabel{\n"
"	background-color:#1d1f26;\n"
"	border:1px solid #272a33;\n"
"font-size:14px;\n"
"}"));
        fontNameLabel->setAlignment(Qt::AlignmentFlag::AlignCenter);

        horizontalLayout->addWidget(fontNameLabel);

        chooseFontBtn = new QPushButton(widget);
        chooseFontBtn->setObjectName("chooseFontBtn");

        horizontalLayout->addWidget(chooseFontBtn);


        verticalLayout->addLayout(horizontalLayout);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        label_3 = new QLabel(widget);
        label_3->setObjectName("label_3");
        label_3->setMinimumSize(QSize(100, 80));
        label_3->setMaximumSize(QSize(100, 80));
        label_3->setStyleSheet(QString::fromUtf8("QLabel{\n"
"	min-width:100px;\n"
"	max-width:100px;\n"
"font-size:14px;\n"
"}"));
        label_3->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTop|Qt::AlignmentFlag::AlignTrailing);

        horizontalLayout_2->addWidget(label_3);

        textEdit = new QTextEdit(widget);
        textEdit->setObjectName("textEdit");
        textEdit->setMinimumSize(QSize(0, 80));
        textEdit->setMaximumSize(QSize(16777215, 80));
        textEdit->setStyleSheet(QString::fromUtf8("QTextEdit{\n"
"	background-color: #3C404D;\n"
"	border:none;\n"
"	border-radius:5px;\n"
"	color:white;\n"
"}"));

        horizontalLayout_2->addWidget(textEdit);


        verticalLayout->addLayout(horizontalLayout_2);

        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName("horizontalLayout_3");
        label_4 = new QLabel(widget);
        label_4->setObjectName("label_4");
        label_4->setStyleSheet(QString::fromUtf8("QLabel{\n"
"	min-width:100px;\n"
"	max-width:100px;\n"
"	font-size:14px;\n"
"}"));
        label_4->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_3->addWidget(label_4);

        transformCBBOX = new QComboBox(widget);
        transformCBBOX->addItem(QString());
        transformCBBOX->addItem(QString());
        transformCBBOX->addItem(QString());
        transformCBBOX->addItem(QString());
        transformCBBOX->setObjectName("transformCBBOX");

        horizontalLayout_3->addWidget(transformCBBOX);


        verticalLayout->addLayout(horizontalLayout_3);

        horizontalLayout_4 = new QHBoxLayout();
        horizontalLayout_4->setObjectName("horizontalLayout_4");
        label_5 = new QLabel(widget);
        label_5->setObjectName("label_5");
        label_5->setStyleSheet(QString::fromUtf8("QLabel{\n"
"	min-width:100px;\n"
"	max-width:100px;\n"
"	font-size:14px;\n"
"}"));
        label_5->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_4->addWidget(label_5);

        colorLabel = new QLabel(widget);
        colorLabel->setObjectName("colorLabel");
        colorLabel->setStyleSheet(QString::fromUtf8("QLabel{\n"
"	background-color:#ffffff;\n"
"	border:0.1px solid #272a33;\n"
"	font-size:14px;\n"
"	color:black;\n"
"}\n"
""));
        colorLabel->setAlignment(Qt::AlignmentFlag::AlignCenter);

        horizontalLayout_4->addWidget(colorLabel);

        chooseColorBtn = new QPushButton(widget);
        chooseColorBtn->setObjectName("chooseColorBtn");

        horizontalLayout_4->addWidget(chooseColorBtn);


        verticalLayout->addLayout(horizontalLayout_4);

        horizontalLayout_5 = new QHBoxLayout();
        horizontalLayout_5->setObjectName("horizontalLayout_5");
        label_10 = new QLabel(widget);
        label_10->setObjectName("label_10");
        label_10->setStyleSheet(QString::fromUtf8("QLabel{\n"
"	min-width:100px;\n"
"	max-width:100px;\n"
"	font-size:14px;\n"
"}"));
        label_10->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_5->addWidget(label_10);

        colorTransparencySlider = new QSlider(widget);
        colorTransparencySlider->setObjectName("colorTransparencySlider");
        colorTransparencySlider->setValue(99);
        colorTransparencySlider->setOrientation(Qt::Orientation::Horizontal);

        horizontalLayout_5->addWidget(colorTransparencySlider);

        colorPercentLabel = new QLabel(widget);
        colorPercentLabel->setObjectName("colorPercentLabel");

        horizontalLayout_5->addWidget(colorPercentLabel);


        verticalLayout->addLayout(horizontalLayout_5);

        horizontalLayout_6 = new QHBoxLayout();
        horizontalLayout_6->setObjectName("horizontalLayout_6");
        label_6 = new QLabel(widget);
        label_6->setObjectName("label_6");
        label_6->setStyleSheet(QString::fromUtf8("QLabel{\n"
"	min-width:100px;\n"
"	max-width:100px;\n"
"	font-size:14px;\n"
"}"));
        label_6->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_6->addWidget(label_6);

        bgColorLabel = new QLabel(widget);
        bgColorLabel->setObjectName("bgColorLabel");
        bgColorLabel->setStyleSheet(QString::fromUtf8("QLabel{\n"
"	background-color:#000000;\n"
"	border:0.1px solid #272a33;\n"
"	font-size:14px;\n"
"	color:white;\n"
"}\n"
""));
        bgColorLabel->setAlignment(Qt::AlignmentFlag::AlignCenter);

        horizontalLayout_6->addWidget(bgColorLabel);

        chooseBgColorBtn = new QPushButton(widget);
        chooseBgColorBtn->setObjectName("chooseBgColorBtn");

        horizontalLayout_6->addWidget(chooseBgColorBtn);


        verticalLayout->addLayout(horizontalLayout_6);

        horizontalLayout_7 = new QHBoxLayout();
        horizontalLayout_7->setObjectName("horizontalLayout_7");
        label_2 = new QLabel(widget);
        label_2->setObjectName("label_2");
        label_2->setStyleSheet(QString::fromUtf8("QLabel{\n"
"	min-width:100px;\n"
"	max-width:100px;\n"
"	font-size:14px;\n"
"}"));
        label_2->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_7->addWidget(label_2);

        bgColorTransparencySlider = new QSlider(widget);
        bgColorTransparencySlider->setObjectName("bgColorTransparencySlider");
        bgColorTransparencySlider->setValue(99);
        bgColorTransparencySlider->setOrientation(Qt::Orientation::Horizontal);

        horizontalLayout_7->addWidget(bgColorTransparencySlider);

        bgColorPercentLabel = new QLabel(widget);
        bgColorPercentLabel->setObjectName("bgColorPercentLabel");

        horizontalLayout_7->addWidget(bgColorPercentLabel);


        verticalLayout->addLayout(horizontalLayout_7);

        horizontalLayout_8 = new QHBoxLayout();
        horizontalLayout_8->setObjectName("horizontalLayout_8");
        label_8 = new QLabel(widget);
        label_8->setObjectName("label_8");
        label_8->setStyleSheet(QString::fromUtf8("QLabel{\n"
"	min-width:100px;\n"
"	max-width:100px;\n"
"	font-size:14px;\n"
"}"));
        label_8->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_8->addWidget(label_8);

        horizonAlignmentCBBox = new QComboBox(widget);
        horizonAlignmentCBBox->addItem(QString());
        horizonAlignmentCBBox->addItem(QString());
        horizonAlignmentCBBox->addItem(QString());
        horizonAlignmentCBBox->addItem(QString());
        horizonAlignmentCBBox->setObjectName("horizonAlignmentCBBox");

        horizontalLayout_8->addWidget(horizonAlignmentCBBox);


        verticalLayout->addLayout(horizontalLayout_8);

        horizontalLayout_9 = new QHBoxLayout();
        horizontalLayout_9->setObjectName("horizontalLayout_9");
        label_9 = new QLabel(widget);
        label_9->setObjectName("label_9");
        label_9->setStyleSheet(QString::fromUtf8("QLabel{\n"
"	min-width:100px;\n"
"	max-width:100px;\n"
"	font-size:14px;\n"
"}"));
        label_9->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_9->addWidget(label_9);

        verticalAlignmentCBBox = new QComboBox(widget);
        verticalAlignmentCBBox->addItem(QString());
        verticalAlignmentCBBox->addItem(QString());
        verticalAlignmentCBBox->addItem(QString());
        verticalAlignmentCBBox->setObjectName("verticalAlignmentCBBox");

        horizontalLayout_9->addWidget(verticalAlignmentCBBox);


        verticalLayout->addLayout(horizontalLayout_9);


        gridLayout_2->addWidget(widget, 1, 0, 1, 1);

        buttonBox = new QDialogButtonBox(TextSettingDialog);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setStyleSheet(QString::fromUtf8("QPushButton{\n"
"	min-width:50px;\n"
"		max-width:50px;\n"
"		min-height:20px;\n"
"		max-height:20px;\n"
"\n"
"}"));
        buttonBox->setOrientation(Qt::Orientation::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Cancel|QDialogButtonBox::StandardButton::Ok);

        gridLayout_2->addWidget(buttonBox, 2, 0, 1, 1);


        retranslateUi(TextSettingDialog);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, TextSettingDialog, qOverload<>(&QDialog::accept));
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, TextSettingDialog, qOverload<>(&QDialog::reject));

        QMetaObject::connectSlotsByName(TextSettingDialog);
    } // setupUi

    void retranslateUi(QDialog *TextSettingDialog)
    {
        TextSettingDialog->setWindowTitle(QCoreApplication::translate("TextSettingDialog", "Dialog", nullptr));
        previewLabel->setText(QString());
        label->setText(QCoreApplication::translate("TextSettingDialog", "\345\255\227\344\275\223", nullptr));
        fontNameLabel->setText(QCoreApplication::translate("TextSettingDialog", "\345\276\256\350\275\257\351\233\205\351\273\221", nullptr));
        chooseFontBtn->setText(QCoreApplication::translate("TextSettingDialog", "\351\200\211\346\213\251\345\255\227\344\275\223", nullptr));
        label_3->setText(QCoreApplication::translate("TextSettingDialog", "\346\226\207\346\234\254", nullptr));
        textEdit->setHtml(QCoreApplication::translate("TextSettingDialog", "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\n"
"<html><head><meta name=\"qrichtext\" content=\"1\" /><meta charset=\"utf-8\" /><style type=\"text/css\">\n"
"p, li { white-space: pre-wrap; }\n"
"hr { height: 1px; border-width: 0; }\n"
"li.unchecked::marker { content: \"\\2610\"; }\n"
"li.checked::marker { content: \"\\2612\"; }\n"
"</style></head><body style=\" font-family:'Microsoft YaHei UI'; font-size:9pt; font-weight:400; font-style:normal;\">\n"
"<p style=\"-qt-paragraph-type:empty; margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><br /></p></body></html>", nullptr));
        label_4->setText(QCoreApplication::translate("TextSettingDialog", "\346\226\207\346\234\254\350\275\254\346\215\242", nullptr));
        transformCBBOX->setItemText(0, QCoreApplication::translate("TextSettingDialog", "\346\227\240", nullptr));
        transformCBBOX->setItemText(1, QCoreApplication::translate("TextSettingDialog", "\345\244\247\345\206\231", nullptr));
        transformCBBOX->setItemText(2, QCoreApplication::translate("TextSettingDialog", "\345\260\217\345\206\231", nullptr));
        transformCBBOX->setItemText(3, QCoreApplication::translate("TextSettingDialog", "\351\246\226\345\255\227\346\257\215\345\244\247\345\206\231", nullptr));

        label_5->setText(QCoreApplication::translate("TextSettingDialog", "\346\226\207\346\234\254\351\242\234\350\211\262", nullptr));
        colorLabel->setText(QCoreApplication::translate("TextSettingDialog", "#ffffff", nullptr));
        chooseColorBtn->setText(QCoreApplication::translate("TextSettingDialog", "\351\200\211\346\213\251\351\242\234\350\211\262", nullptr));
        label_10->setText(QCoreApplication::translate("TextSettingDialog", "\344\270\215\351\200\217\346\230\216\345\272\246", nullptr));
        colorPercentLabel->setText(QCoreApplication::translate("TextSettingDialog", "100%", nullptr));
        label_6->setText(QCoreApplication::translate("TextSettingDialog", "\350\203\214\346\231\257\351\242\234\350\211\262", nullptr));
        bgColorLabel->setText(QCoreApplication::translate("TextSettingDialog", "#000000", nullptr));
        chooseBgColorBtn->setText(QCoreApplication::translate("TextSettingDialog", "\351\200\211\346\213\251\351\242\234\350\211\262", nullptr));
        label_2->setText(QCoreApplication::translate("TextSettingDialog", "\350\203\214\346\231\257\344\270\215\351\200\217\346\230\216\345\272\246", nullptr));
        bgColorPercentLabel->setText(QCoreApplication::translate("TextSettingDialog", "100%", nullptr));
        label_8->setText(QCoreApplication::translate("TextSettingDialog", "\346\260\264\345\271\263\345\257\271\351\275\220", nullptr));
        horizonAlignmentCBBox->setItemText(0, QCoreApplication::translate("TextSettingDialog", "\345\267\246\345\257\271\351\275\220", nullptr));
        horizonAlignmentCBBox->setItemText(1, QCoreApplication::translate("TextSettingDialog", "\345\217\263\345\257\271\351\275\220", nullptr));
        horizonAlignmentCBBox->setItemText(2, QCoreApplication::translate("TextSettingDialog", "\346\260\264\345\271\263\345\261\205\344\270\255", nullptr));
        horizonAlignmentCBBox->setItemText(3, QCoreApplication::translate("TextSettingDialog", "\344\270\244\347\253\257\345\257\271\351\275\220", nullptr));

        label_9->setText(QCoreApplication::translate("TextSettingDialog", "\345\236\202\347\233\264\345\257\271\351\275\220", nullptr));
        verticalAlignmentCBBox->setItemText(0, QCoreApplication::translate("TextSettingDialog", "\351\241\266\351\203\250\345\257\271\351\275\220", nullptr));
        verticalAlignmentCBBox->setItemText(1, QCoreApplication::translate("TextSettingDialog", "\345\236\202\347\233\264\345\261\205\344\270\255", nullptr));
        verticalAlignmentCBBox->setItemText(2, QCoreApplication::translate("TextSettingDialog", "\345\272\225\351\203\250\345\257\271\351\275\220", nullptr));

    } // retranslateUi

};

namespace Ui {
    class TextSettingDialog: public Ui_TextSettingDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TEXTSETTINGDIALOG_H
