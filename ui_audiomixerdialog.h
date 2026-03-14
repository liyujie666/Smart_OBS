/********************************************************************************
** Form generated from reading UI file 'audiomixerdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.9.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_AUDIOMIXERDIALOG_H
#define UI_AUDIOMIXERDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_AudioMixerDialog
{
public:
    QGridLayout *gridLayout;
    QDialogButtonBox *buttonBox;
    QWidget *containerWidget;
    QGridLayout *gridLayout_2;
    QTableWidget *tableWidget;

    void setupUi(QDialog *AudioMixerDialog)
    {
        if (AudioMixerDialog->objectName().isEmpty())
            AudioMixerDialog->setObjectName("AudioMixerDialog");
        AudioMixerDialog->resize(780, 306);
        AudioMixerDialog->setMinimumSize(QSize(780, 0));
        AudioMixerDialog->setMaximumSize(QSize(780, 16777215));
        AudioMixerDialog->setStyleSheet(QString::fromUtf8("QDialog{\n"
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
"}\n"
"\n"
"\n"
"\n"
"\n"
"QDoubleSpinBox {\n"
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
"/* \345\216\273\351\231\244\350\276\271\346\241\206\351\230\264\345\275\261\347\255\211\351\273\230\350\256\244\346\225\210\346\236\234 */\n"
"QDoubleSpinB"
                        "ox:focus {\n"
"    border: 1px solid #284CB8;\n"
"}\n"
"\n"
"/* \344\270\212\344\270\213\346\214\211\351\222\256\345\214\272\345\237\237 */\n"
"QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {\n"
"    background-color: #3C404D;\n"
"    subcontrol-origin: border;\n"
"    width: 20px;\n"
"    border-left: 1px solid #3C3C3C;\n"
"}\n"
"\n"
"/* \344\270\212\346\214\211\351\222\256\345\233\276\346\240\207 */\n"
"QDoubleSpinBox::up-arrow {\n"
"    image: url(:/sources/up.png); /* \344\275\277\347\224\250\344\275\240\350\207\252\345\267\261\347\232\204\345\233\276\346\240\207 */\n"
"    width: 12px;\n"
"    height: 12px;\n"
"}\n"
"\n"
"/* \344\270\213\346\214\211\351\222\256\345\233\276\346\240\207 */\n"
"QDoubleSpinBox::down-arrow {\n"
"    image: url(:/sources/down.png); /* \344\275\277\347\224\250\344\275\240\350\207\252\345\267\261\347\232\204\345\233\276\346\240\207 */\n"
"    width: 12px;\n"
"    height: 12px;\n"
"}\n"
"\n"
"/* \345\205\263\351\227\255\344\270\212\344\270\213\346\214\211\351\222\256\346\214"
                        "\211\344\270\213\346\227\266\347\232\204\351\253\230\344\272\256 */\n"
"QDoubleSpinBox::up-button:pressed,\n"
"QDoubleSpinBox::down-button:pressed {\n"
"    background-color: #50555E;\n"
"}\n"
"\n"
"/* \345\216\273\351\231\244\350\276\223\345\205\245\347\204\246\347\202\271\350\276\271\346\241\206 */\n"
"QDoubleSpinBox::edit-field {\n"
"    background: transparent;\n"
"    padding-right: 24px; /* \347\273\231\346\214\211\351\222\256\347\225\231\347\251\272\351\227\264 */\n"
"}"));
        gridLayout = new QGridLayout(AudioMixerDialog);
        gridLayout->setObjectName("gridLayout");
        gridLayout->setContentsMargins(15, 15, 15, 15);
        buttonBox = new QDialogButtonBox(AudioMixerDialog);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setOrientation(Qt::Orientation::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Cancel|QDialogButtonBox::StandardButton::Ok);

        gridLayout->addWidget(buttonBox, 1, 0, 1, 1);

        containerWidget = new QWidget(AudioMixerDialog);
        containerWidget->setObjectName("containerWidget");
        containerWidget->setStyleSheet(QString::fromUtf8("QWidget{\n"
"	background-color:#272A33;\n"
"	border-radius:8px;\n"
"}"));
        gridLayout_2 = new QGridLayout(containerWidget);
        gridLayout_2->setObjectName("gridLayout_2");
        tableWidget = new QTableWidget(containerWidget);
        if (tableWidget->columnCount() < 5)
            tableWidget->setColumnCount(5);
        QTableWidgetItem *__qtablewidgetitem = new QTableWidgetItem();
        tableWidget->setHorizontalHeaderItem(0, __qtablewidgetitem);
        QTableWidgetItem *__qtablewidgetitem1 = new QTableWidgetItem();
        tableWidget->setHorizontalHeaderItem(1, __qtablewidgetitem1);
        QTableWidgetItem *__qtablewidgetitem2 = new QTableWidgetItem();
        tableWidget->setHorizontalHeaderItem(2, __qtablewidgetitem2);
        QTableWidgetItem *__qtablewidgetitem3 = new QTableWidgetItem();
        tableWidget->setHorizontalHeaderItem(3, __qtablewidgetitem3);
        QTableWidgetItem *__qtablewidgetitem4 = new QTableWidgetItem();
        tableWidget->setHorizontalHeaderItem(4, __qtablewidgetitem4);
        tableWidget->setObjectName("tableWidget");
        tableWidget->setStyleSheet(QString::fromUtf8("/* \345\216\273\351\231\244\350\241\250\346\240\274\346\211\200\346\234\211\350\276\271\346\241\206\346\240\267\345\274\217 */\n"
"QTableWidget {\n"
"    border: none;               /* \345\216\273\351\231\244\350\241\250\346\240\274\346\234\200\345\244\226\345\261\202\350\276\271\346\241\206 */\n"
"    background-color: #272A33;  /* \350\203\214\346\231\257\350\211\262\357\274\210\345\217\257\346\240\271\346\215\256\351\234\200\350\246\201\350\260\203\346\225\264\357\274\211 */\n"
"    color: #ffffff;             /* \346\226\207\345\255\227\351\242\234\350\211\262 */\n"
"    gridline-color: transparent;/* \347\275\221\346\240\274\347\272\277\351\200\217\346\230\216 */\n"
"}\n"
"\n"
"QTableWidget::item {\n"
"    border: none;               /* \345\216\273\351\231\244\345\215\225\345\205\203\346\240\274\350\276\271\346\241\206 */\n"
"    padding: 4px 8px;           /* \345\215\225\345\205\203\346\240\274\345\206\205\350\276\271\350\267\235 */\n"
"	text-align: left; /* \346\260\264\345\271\263\351\235\240\345\267"
                        "\246 */\n"
"    vertical-align: middle; /* \345\236\202\347\233\264\345\261\205\344\270\255 */\n"
"}\n"
"\n"
"/* \350\241\250\345\244\264\346\240\267\345\274\217 */\n"
"QHeaderView::section {\n"
"    border: none;               /* \345\216\273\351\231\244\350\241\250\345\244\264\350\276\271\346\241\206 */\n"
"    background-color: #272A33;  /* \350\241\250\345\244\264\350\203\214\346\231\257\350\211\262 */\n"
"    color: #ffffff;             /* \350\241\250\345\244\264\346\226\207\345\255\227\351\242\234\350\211\262 */\n"
"    padding: 6px 8px;           /* \350\241\250\345\244\264\345\206\205\350\276\271\350\267\235 */\n"
"	text-align:left;\n"
"    vertical-align: middle; /* \345\236\202\347\233\264\345\261\205\344\270\255 */\n"
"}\n"
"\n"
"/* \346\273\232\345\212\250\345\214\272\345\237\237\350\276\271\346\241\206\345\216\273\351\231\244 */\n"
"QTableWidget QScrollArea {\n"
"    border: none;\n"
"}\n"
"\n"
"/* \346\273\232\345\212\250\346\235\241\346\240\267\345\274\217\357\274\210\345\217\257\351\200\211\357"
                        "\274\211 */\n"
"QTableWidget QScrollBar:vertical {\n"
"    background-color: #272822;\n"
"    width: 8px;\n"
"    margin: 0px;\n"
"}\n"
"\n"
"QTableWidget QScrollBar::handle:vertical {\n"
"    background-color: #555555;\n"
"    border-radius: 4px;\n"
"}\n"
"\n"
"QTableWidget QScrollBar:horizontal {\n"
"    background-color: #272822;\n"
"    height: 8px;\n"
"    margin: 0px;\n"
"}\n"
"\n"
"QTableWidget QScrollBar::handle:horizontal {\n"
"    background-color: #555555;\n"
"    border-radius: 4px;\n"
"}\n"
"\n"
""));
        tableWidget->setDefaultDropAction(Qt::DropAction::IgnoreAction);
        tableWidget->setSelectionMode(QAbstractItemView::SelectionMode::NoSelection);
        tableWidget->setShowGrid(false);
        tableWidget->setGridStyle(Qt::PenStyle::NoPen);
        tableWidget->horizontalHeader()->setVisible(true);
        tableWidget->horizontalHeader()->setHighlightSections(false);
        tableWidget->verticalHeader()->setVisible(false);
        tableWidget->verticalHeader()->setHighlightSections(false);

        gridLayout_2->addWidget(tableWidget, 0, 0, 1, 1);


        gridLayout->addWidget(containerWidget, 0, 0, 1, 1);


        retranslateUi(AudioMixerDialog);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, AudioMixerDialog, qOverload<>(&QDialog::accept));
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, AudioMixerDialog, qOverload<>(&QDialog::reject));

        QMetaObject::connectSlotsByName(AudioMixerDialog);
    } // setupUi

    void retranslateUi(QDialog *AudioMixerDialog)
    {
        AudioMixerDialog->setWindowTitle(QCoreApplication::translate("AudioMixerDialog", "Dialog", nullptr));
        QTableWidgetItem *___qtablewidgetitem = tableWidget->horizontalHeaderItem(0);
        ___qtablewidgetitem->setText(QCoreApplication::translate("AudioMixerDialog", "\345\220\215\347\247\260", nullptr));
        QTableWidgetItem *___qtablewidgetitem1 = tableWidget->horizontalHeaderItem(1);
        ___qtablewidgetitem1->setText(QCoreApplication::translate("AudioMixerDialog", "\347\212\266\346\200\201", nullptr));
        QTableWidgetItem *___qtablewidgetitem2 = tableWidget->horizontalHeaderItem(2);
        ___qtablewidgetitem2->setText(QCoreApplication::translate("AudioMixerDialog", "\351\237\263\351\207\217", nullptr));
        QTableWidgetItem *___qtablewidgetitem3 = tableWidget->horizontalHeaderItem(3);
        ___qtablewidgetitem3->setText(QCoreApplication::translate("AudioMixerDialog", "\345\220\214\346\255\245\345\201\217\347\247\273", nullptr));
        QTableWidgetItem *___qtablewidgetitem4 = tableWidget->horizontalHeaderItem(4);
        ___qtablewidgetitem4->setText(QCoreApplication::translate("AudioMixerDialog", "\351\237\263\351\242\221\347\233\221\345\220\254", nullptr));
    } // retranslateUi

};

namespace Ui {
    class AudioMixerDialog: public Ui_AudioMixerDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_AUDIOMIXERDIALOG_H
