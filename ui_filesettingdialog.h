/********************************************************************************
** Form generated from reading UI file 'filesettingdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.9.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_FILESETTINGDIALOG_H
#define UI_FILESETTINGDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_FileSettingDialog
{
public:
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout;
    QLabel *label;
    QLineEdit *filePathLineEdit;
    QPushButton *openFileBtn;
    QHBoxLayout *horizontalLayout_2;
    QLabel *label_2;
    QSlider *speedSlider;
    QLabel *speedLabel;
    QSpacerItem *verticalSpacer;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *FileSettingDialog)
    {
        if (FileSettingDialog->objectName().isEmpty())
            FileSettingDialog->setObjectName("FileSettingDialog");
        FileSettingDialog->resize(675, 163);
        FileSettingDialog->setStyleSheet(QString::fromUtf8("QDialog{\n"
"	background-color:#1D1F26\n"
"}\n"
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
"    image: url(:/sources/down.png);  /* \344\275\240\345\217\257\344\273\245\347\224\250\350\207\252\345\267\261\347\232\204\347\256\255\345\244\264\345"
                        "\233\276\346\240\207 */\n"
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
"QSlider::groove:horizontal {\n"
"    border: none;\n"
"    height: 6px;\n"
"    background: #3c3f41;\n"
"    border-radius: 3px;\n"
"}\n"
"\n"
"QSlider::handle:horizontal {\n"
"    background: white;         /* \346\273\221\345\235\227\351\242\234\350\211\262\357\274\210"
                        "\345\217\257\350\207\252\345\256\232\344\271\211\344\270\272 OBS \351\253\230\344\272\256\350\211\262\357\274\211 */\n"
"    border: none;\n"
"    width: 20px;\n"
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
"    background: #464B59;         /* \346\234\252\345\241\253\345\205\205\351\203\250\345\210\206 */\n"
"    border-radius: 3px;\n"
"}\n"
"\n"
""));
        verticalLayout = new QVBoxLayout(FileSettingDialog);
        verticalLayout->setObjectName("verticalLayout");
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        label = new QLabel(FileSettingDialog);
        label->setObjectName("label");
        label->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout->addWidget(label);

        filePathLineEdit = new QLineEdit(FileSettingDialog);
        filePathLineEdit->setObjectName("filePathLineEdit");

        horizontalLayout->addWidget(filePathLineEdit);

        openFileBtn = new QPushButton(FileSettingDialog);
        openFileBtn->setObjectName("openFileBtn");

        horizontalLayout->addWidget(openFileBtn);


        verticalLayout->addLayout(horizontalLayout);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        label_2 = new QLabel(FileSettingDialog);
        label_2->setObjectName("label_2");
        label_2->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_2->addWidget(label_2);

        speedSlider = new QSlider(FileSettingDialog);
        speedSlider->setObjectName("speedSlider");
        speedSlider->setOrientation(Qt::Orientation::Horizontal);

        horizontalLayout_2->addWidget(speedSlider);

        speedLabel = new QLabel(FileSettingDialog);
        speedLabel->setObjectName("speedLabel");
        speedLabel->setStyleSheet(QString::fromUtf8("QLabel{\n"
"	min-width:70px;\n"
"	max-width:70px;\n"
"\n"
"}"));

        horizontalLayout_2->addWidget(speedLabel);


        verticalLayout->addLayout(horizontalLayout_2);

        verticalSpacer = new QSpacerItem(20, 24, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout->addItem(verticalSpacer);

        buttonBox = new QDialogButtonBox(FileSettingDialog);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setOrientation(Qt::Orientation::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Cancel|QDialogButtonBox::StandardButton::Ok);

        verticalLayout->addWidget(buttonBox);


        retranslateUi(FileSettingDialog);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, FileSettingDialog, qOverload<>(&QDialog::accept));
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, FileSettingDialog, qOverload<>(&QDialog::reject));

        QMetaObject::connectSlotsByName(FileSettingDialog);
    } // setupUi

    void retranslateUi(QDialog *FileSettingDialog)
    {
        FileSettingDialog->setWindowTitle(QCoreApplication::translate("FileSettingDialog", "Dialog", nullptr));
        label->setText(QCoreApplication::translate("FileSettingDialog", "\346\234\254\345\234\260\346\226\207\344\273\266", nullptr));
        filePathLineEdit->setText(QCoreApplication::translate("FileSettingDialog", "C:/Users/liyuj/Videos/2025-08-17_18-48-32.mp4", nullptr));
        openFileBtn->setText(QCoreApplication::translate("FileSettingDialog", "\346\265\217\350\247\210", nullptr));
        label_2->setText(QCoreApplication::translate("FileSettingDialog", "\351\200\237\345\272\246", nullptr));
        speedLabel->setText(QCoreApplication::translate("FileSettingDialog", "1.0x", nullptr));
    } // retranslateUi

};

namespace Ui {
    class FileSettingDialog: public Ui_FileSettingDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_FILESETTINGDIALOG_H
