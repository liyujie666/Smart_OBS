/********************************************************************************
** Form generated from reading UI file 'camerasettingdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.9.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CAMERASETTINGDIALOG_H
#define UI_CAMERASETTINGDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_CameraSettingDialog
{
public:
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout;
    QLabel *label;
    QComboBox *cameraCBBox;
    QHBoxLayout *horizontalLayout_6;
    QSpacerItem *horizontalSpacer;
    QPushButton *pushButton;
    QSpacerItem *horizontalSpacer_2;
    QHBoxLayout *horizontalLayout_2;
    QLabel *label_2;
    QComboBox *typeCBBox;
    QHBoxLayout *horizontalLayout_3;
    QLabel *resolutionLabel;
    QComboBox *resolutionCBBox;
    QHBoxLayout *horizontalLayout_4;
    QLabel *fpsLabel;
    QComboBox *fpsCBBox;
    QHBoxLayout *horizontalLayout_5;
    QLabel *videoFormatLabel;
    QComboBox *videoFormatCBBox;
    QSpacerItem *verticalSpacer;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *CameraSettingDialog)
    {
        if (CameraSettingDialog->objectName().isEmpty())
            CameraSettingDialog->setObjectName("CameraSettingDialog");
        CameraSettingDialog->resize(583, 497);
        CameraSettingDialog->setStyleSheet(QString::fromUtf8("QDialog{\n"
"	background-color:#1D1F26\n"
"}\n"
"QLabel{\n"
"	color:white;\n"
"	font-size:14px;\n"
"	min-width:115px;\n"
"	max-width:115px;\n"
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
"    selection-background-"
                        "color: #284CB8;\n"
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
"QPushButton:pressed { background-color: #50515b; }"));
        verticalLayout = new QVBoxLayout(CameraSettingDialog);
        verticalLayout->setObjectName("verticalLayout");
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        label = new QLabel(CameraSettingDialog);
        label->setObjectName("label");
        label->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout->addWidget(label);

        cameraCBBox = new QComboBox(CameraSettingDialog);
        cameraCBBox->setObjectName("cameraCBBox");

        horizontalLayout->addWidget(cameraCBBox);


        verticalLayout->addLayout(horizontalLayout);

        horizontalLayout_6 = new QHBoxLayout();
        horizontalLayout_6->setObjectName("horizontalLayout_6");
        horizontalSpacer = new QSpacerItem(120, 20, QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Minimum);

        horizontalLayout_6->addItem(horizontalSpacer);

        pushButton = new QPushButton(CameraSettingDialog);
        pushButton->setObjectName("pushButton");

        horizontalLayout_6->addWidget(pushButton);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_6->addItem(horizontalSpacer_2);


        verticalLayout->addLayout(horizontalLayout_6);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        label_2 = new QLabel(CameraSettingDialog);
        label_2->setObjectName("label_2");
        label_2->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_2->addWidget(label_2);

        typeCBBox = new QComboBox(CameraSettingDialog);
        typeCBBox->addItem(QString());
        typeCBBox->addItem(QString());
        typeCBBox->setObjectName("typeCBBox");

        horizontalLayout_2->addWidget(typeCBBox);


        verticalLayout->addLayout(horizontalLayout_2);

        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName("horizontalLayout_3");
        resolutionLabel = new QLabel(CameraSettingDialog);
        resolutionLabel->setObjectName("resolutionLabel");
        resolutionLabel->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_3->addWidget(resolutionLabel);

        resolutionCBBox = new QComboBox(CameraSettingDialog);
        resolutionCBBox->setObjectName("resolutionCBBox");

        horizontalLayout_3->addWidget(resolutionCBBox);


        verticalLayout->addLayout(horizontalLayout_3);

        horizontalLayout_4 = new QHBoxLayout();
        horizontalLayout_4->setObjectName("horizontalLayout_4");
        fpsLabel = new QLabel(CameraSettingDialog);
        fpsLabel->setObjectName("fpsLabel");
        fpsLabel->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_4->addWidget(fpsLabel);

        fpsCBBox = new QComboBox(CameraSettingDialog);
        fpsCBBox->setObjectName("fpsCBBox");

        horizontalLayout_4->addWidget(fpsCBBox);


        verticalLayout->addLayout(horizontalLayout_4);

        horizontalLayout_5 = new QHBoxLayout();
        horizontalLayout_5->setObjectName("horizontalLayout_5");
        videoFormatLabel = new QLabel(CameraSettingDialog);
        videoFormatLabel->setObjectName("videoFormatLabel");
        videoFormatLabel->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_5->addWidget(videoFormatLabel);

        videoFormatCBBox = new QComboBox(CameraSettingDialog);
        videoFormatCBBox->setObjectName("videoFormatCBBox");

        horizontalLayout_5->addWidget(videoFormatCBBox);


        verticalLayout->addLayout(horizontalLayout_5);

        verticalSpacer = new QSpacerItem(20, 173, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout->addItem(verticalSpacer);

        buttonBox = new QDialogButtonBox(CameraSettingDialog);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setOrientation(Qt::Orientation::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Cancel|QDialogButtonBox::StandardButton::Ok);

        verticalLayout->addWidget(buttonBox);


        retranslateUi(CameraSettingDialog);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, CameraSettingDialog, qOverload<>(&QDialog::accept));
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, CameraSettingDialog, qOverload<>(&QDialog::reject));

        QMetaObject::connectSlotsByName(CameraSettingDialog);
    } // setupUi

    void retranslateUi(QDialog *CameraSettingDialog)
    {
        CameraSettingDialog->setWindowTitle(QCoreApplication::translate("CameraSettingDialog", "Dialog", nullptr));
        label->setText(QCoreApplication::translate("CameraSettingDialog", "\350\256\276\345\244\207", nullptr));
        pushButton->setText(QCoreApplication::translate("CameraSettingDialog", "\345\217\226\346\266\210\346\277\200\346\264\273", nullptr));
        label_2->setText(QCoreApplication::translate("CameraSettingDialog", "\345\210\206\350\276\250\347\216\207/\345\270\247\347\216\207 \347\261\273\345\236\213", nullptr));
        typeCBBox->setItemText(0, QCoreApplication::translate("CameraSettingDialog", "\350\256\276\345\244\207\351\273\230\350\256\244", nullptr));
        typeCBBox->setItemText(1, QCoreApplication::translate("CameraSettingDialog", "\350\207\252\345\256\232\344\271\211", nullptr));

        resolutionLabel->setText(QCoreApplication::translate("CameraSettingDialog", "\345\210\206\350\276\250\347\216\207", nullptr));
        fpsLabel->setText(QCoreApplication::translate("CameraSettingDialog", "FPS", nullptr));
        videoFormatLabel->setText(QCoreApplication::translate("CameraSettingDialog", "\350\247\206\351\242\221\346\240\274\345\274\217", nullptr));
    } // retranslateUi

};

namespace Ui {
    class CameraSettingDialog: public Ui_CameraSettingDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CAMERASETTINGDIALOG_H
