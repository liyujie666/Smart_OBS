/********************************************************************************
** Form generated from reading UI file 'screensettingdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.9.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SCREENSETTINGDIALOG_H
#define UI_SCREENSETTINGDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_ScreenSettingDialog
{
public:
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout;
    QLabel *label;
    QComboBox *captureTypeCBBox;
    QHBoxLayout *horizontalLayout_2;
    QLabel *label_2;
    QComboBox *screenCBBox;
    QCheckBox *drawMouseCKBox;
    QSpacerItem *verticalSpacer;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *ScreenSettingDialog)
    {
        if (ScreenSettingDialog->objectName().isEmpty())
            ScreenSettingDialog->setObjectName("ScreenSettingDialog");
        ScreenSettingDialog->resize(500, 388);
        ScreenSettingDialog->setStyleSheet(QString::fromUtf8("QDialog{\n"
"	background-color:#1D1F26\n"
"}\n"
"QLabel{\n"
"	color:white;\n"
"	font-size:14px;\n"
"	min-width:60px;\n"
"	max-width:60px;\n"
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
"    selection-background-co"
                        "lor: #284CB8;\n"
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
        verticalLayout = new QVBoxLayout(ScreenSettingDialog);
        verticalLayout->setObjectName("verticalLayout");
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        label = new QLabel(ScreenSettingDialog);
        label->setObjectName("label");
        label->setStyleSheet(QString::fromUtf8(""));

        horizontalLayout->addWidget(label);

        captureTypeCBBox = new QComboBox(ScreenSettingDialog);
        captureTypeCBBox->addItem(QString());
        captureTypeCBBox->addItem(QString());
        captureTypeCBBox->setObjectName("captureTypeCBBox");

        horizontalLayout->addWidget(captureTypeCBBox);


        verticalLayout->addLayout(horizontalLayout);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        label_2 = new QLabel(ScreenSettingDialog);
        label_2->setObjectName("label_2");

        horizontalLayout_2->addWidget(label_2);

        screenCBBox = new QComboBox(ScreenSettingDialog);
        screenCBBox->setObjectName("screenCBBox");

        horizontalLayout_2->addWidget(screenCBBox);


        verticalLayout->addLayout(horizontalLayout_2);

        drawMouseCKBox = new QCheckBox(ScreenSettingDialog);
        drawMouseCKBox->setObjectName("drawMouseCKBox");
        drawMouseCKBox->setStyleSheet(QString::fromUtf8("QCheckBox{\n"
"    color: white;\n"
"	font-size:14px;\n"
"	margin-left:10px;\n"
"	spacing: 10px;\n"
"	margin-left: 64px;\n"
"}\n"
"QCheckBox::indicator {\n"
"    width: 20px;    /* \350\256\276\347\275\256\345\213\276\351\200\211\346\241\206\347\232\204\345\256\275\345\272\246 */\n"
"    height: 20px;   /* \350\256\276\347\275\256\345\213\276\351\200\211\346\241\206\347\232\204\351\253\230\345\272\246 */\n"
"}"));
        drawMouseCKBox->setChecked(true);

        verticalLayout->addWidget(drawMouseCKBox);

        verticalSpacer = new QSpacerItem(20, 238, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout->addItem(verticalSpacer);

        buttonBox = new QDialogButtonBox(ScreenSettingDialog);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setStyleSheet(QString::fromUtf8(""));
        buttonBox->setOrientation(Qt::Orientation::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Cancel|QDialogButtonBox::StandardButton::Ok);

        verticalLayout->addWidget(buttonBox);


        retranslateUi(ScreenSettingDialog);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, ScreenSettingDialog, qOverload<>(&QDialog::accept));
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, ScreenSettingDialog, qOverload<>(&QDialog::reject));

        QMetaObject::connectSlotsByName(ScreenSettingDialog);
    } // setupUi

    void retranslateUi(QDialog *ScreenSettingDialog)
    {
        ScreenSettingDialog->setWindowTitle(QCoreApplication::translate("ScreenSettingDialog", "Dialog", nullptr));
        label->setText(QCoreApplication::translate("ScreenSettingDialog", "\351\207\207\351\233\206\346\226\271\345\274\217", nullptr));
        captureTypeCBBox->setItemText(0, QCoreApplication::translate("ScreenSettingDialog", "DXGI", nullptr));
        captureTypeCBBox->setItemText(1, QCoreApplication::translate("ScreenSettingDialog", "gdigrab", nullptr));

        label_2->setText(QCoreApplication::translate("ScreenSettingDialog", "\346\230\276\347\244\272\345\231\250", nullptr));
        screenCBBox->setPlaceholderText(QString());
        drawMouseCKBox->setText(QCoreApplication::translate("ScreenSettingDialog", "\347\273\230\345\210\266\351\274\240\346\240\207", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ScreenSettingDialog: public Ui_ScreenSettingDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SCREENSETTINGDIALOG_H
