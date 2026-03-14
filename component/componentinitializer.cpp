#include "componentinitializer.h"
#include "mainwindow.h"
#include "filter/selectioneventfilter.h"
#include "scene/scenemanager.h"
#include "source/video/desktopsource.h"
#include "source/video/camerasource.h"
#include "source/video/localvideosource.h"
#include "source/video/textsource.h"
#include "scene/scene.h"
#include "component/camerasettingdialog.h"
#include "component/filesettingdialog.h"
#include "component/screensettingdialog.h"
#include "render/cudarenderwidget.h"
#include "controlbar.h"
#include <QHBoxLayout>
#include <QDebug>
#include <QApplication>
#include <QScreen>
#include <QIcon>
#include <QDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QComboBox>
#include <QMediaDevices>
#include <QCameraDevice>
#include <QFontDialog>
#include <QColorDialog>
#include <QColor>
#include <QPalette>
#include <QPointer>
#include <QDebug>
extern"C"{
#include <libavdevice/avdevice.h>
}

ComponentInitializer* ComponentInitializer::instance_ = nullptr;
ComponentInitializer* ComponentInitializer::getInstance() {
    if (!instance_) {
        instance_ = new ComponentInitializer();
    }
    return instance_;
}

void ComponentInitializer::destroyInstance() {
    if (instance_) {
        delete instance_;
        instance_ = nullptr;
    }
}
ComponentInitializer::~ComponentInitializer()
{
    clear();
    avformat_network_deinit();
    if (m_selectFilter) {
        delete m_selectFilter;
        m_selectFilter = nullptr;
    }
}
void ComponentInitializer::init(QWidget *mainwindow,SceneManager* sceneManager)
{
    avformat_network_init();
    avdevice_register_all();
    // initStatusBar(mainwindow);
    initSceneListWidget(mainwindow);
    initFilter(mainwindow,sceneManager);
}

void ComponentInitializer::initStatusBar(QWidget *mainwindow)
{
    QStatusBar* statusBar = mainwindow->findChild<QStatusBar*>("statusbar");
    QLabel* cpuLabel = createLabel(mainwindow,"CPU:20% 帧率:30fps","QLabel{color:white;}");
    cpuLabel->setAlignment(Qt::AlignRight | Qt::AlignCenter);
    statusBar->setStyleSheet("QStatusBar { background-color: rgb(60, 64, 77);color:white;}");
    statusBar->addPermanentWidget(cpuLabel);
    statusBar->showMessage("软件启动成功！");
}

void ComponentInitializer::initSceneListWidget(QWidget *mainwindow)
{
    QListWidget* sceneListWidget = mainwindow->findChild<QListWidget*>("sceneListWidget");
    sceneListWidget->clear();
    sceneListWidget->setStyleSheet(R"(QListWidget {
                                        background-color:#1E1F29;
                                        border:0.5px solid #70778f;
                                        color: #ffffff;
                                        padding: 4px;
                                        outline: none;
                                    }

                                    QListWidget::item {
                                        background-color: transparent;
                                        color: #ffffff;
                                        padding: 6px 10px;
                                        border-radius: 4px;
                                    }

                                    QListWidget::item:hover {
                                        background-color: #2b2d35;
                                    }

                                    QListWidget::item:selected {
                                        background-color: #284CB8;
                                        color: #ffffff;
                                    })");
}

void ComponentInitializer::initFilter(QWidget *mainwindow,SceneManager* sceneManager)
{
    m_selectFilter = new SelectionEventFilter(mainwindow);
    m_selectFilter->setNormalStyle("background-color: transparent; border-radius: 5px;border:none;");
    m_selectFilter->setSelectedStyle("background-color: #284CB8; border-radius: 5px;border:none;");
    m_selectFilter->setHoverStyle("background-color: #2b2d35; border-radius: 5px;border:none;");

    connect(m_selectFilter,&SelectionEventFilter::widgetSelected,this,[=](QWidget* selectedWidget){
        on_widgetSelected(selectedWidget,mainwindow,sceneManager);
    });
    CudaRenderWidget* cudaRenderWidget = mainwindow->findChild<CudaRenderWidget*>("openGLWidget");
    connect(m_selectFilter,&SelectionEventFilter::sourceItemSelected,cudaRenderWidget,&CudaRenderWidget::setLayerSelected);
}

QString ComponentInitializer::showCreateSceneDialog(QWidget *mainwindow)
{
    QDialog dialog(mainwindow);
    dialog.setWindowTitle("添加场景");
    dialog.setFixedSize(400, 120);
    dialog.setStyleSheet("background-color: #1e1f25; border-radius: 8px; color: white;");

    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);
    mainLayout->setAlignment(Qt::AlignTop);

    QLabel* label = new QLabel("请输入场景名称", &dialog);
    label->setStyleSheet("font-size: 14px; color: #ffffff;");
    mainLayout->addWidget(label);

    QLineEdit* lineEdit = new QLineEdit(&dialog);
    lineEdit->setText("场景 " + QString::number(sceneIndex));
    lineEdit->setStyleSheet(
        "background-color: #2e2f38;"
        "border: none;"
        "border-radius: 6px;"
        "padding: 8px;"
        "color: #ffffff;"
        );
    mainLayout->addWidget(lineEdit);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    QPushButton* okButton = new QPushButton("确定", &dialog);
    QPushButton* cancelButton = new QPushButton("取消", &dialog);

    QString buttonStyle =
        "QPushButton {"
        " background-color: #2e2f38;"
        " border: none;"
        " border-radius: 4px;"
        " padding: 6px 12px;"
        " color: white;"
        "}"
        "QPushButton:hover { background-color: #3c3d47; }"
        "QPushButton:pressed { background-color: #50515b; }";

    okButton->setStyleSheet(buttonStyle);
    cancelButton->setStyleSheet(buttonStyle);

    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);

    connect(okButton, &QPushButton::clicked, [&]() { dialog.accept(); });
    connect(cancelButton, &QPushButton::clicked, [&]() { dialog.reject(); });

    if (dialog.exec() == QDialog::Accepted) {

        sceneIndex++;
        return lineEdit->text().trimmed();
    }

    return QString();

}

QString ComponentInitializer::showCreateDialog(const QString &title, const QString &labelName, const QString &placeHolder, QWidget *mainwindow)
{
    QDialog dialog(mainwindow);
    dialog.setWindowTitle(title);
    dialog.setFixedSize(400, 120);
    dialog.setStyleSheet("background-color: #1e1f25; border-radius: 8px; color: white;");

    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);
    mainLayout->setAlignment(Qt::AlignTop);

    QLabel* label = new QLabel(labelName, &dialog);
    label->setStyleSheet("font-size: 14px; color: #ffffff;");
    label->setFixedHeight(25);
    mainLayout->addWidget(label);

    QLineEdit* lineEdit = new QLineEdit(&dialog);
    lineEdit->setText(placeHolder);
    lineEdit->setStyleSheet(
        "background-color: #2e2f38;"
        "border: none;"
        "border-radius: 6px;"
        "padding: 8px;"
        "color: #ffffff;"
        );
    mainLayout->addWidget(lineEdit);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    QPushButton* okButton = new QPushButton("确定", &dialog);
    QPushButton* cancelButton = new QPushButton("取消", &dialog);

    QString buttonStyle =
        "QPushButton {"
        " background-color: #2e2f38;"
        " border: none;"
        " border-radius: 4px;"
        " padding: 6px 12px;"
        " color: white;"
        "}"
        "QPushButton:hover { background-color: #3c3d47; }"
        "QPushButton:pressed { background-color: #50515b; }";

    okButton->setStyleSheet(buttonStyle);
    cancelButton->setStyleSheet(buttonStyle);

    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);

    connect(okButton, &QPushButton::clicked, [&]() { dialog.accept(); });
    connect(cancelButton, &QPushButton::clicked, [&]() { dialog.reject(); });

    if (dialog.exec() == QDialog::Accepted) {
        return lineEdit->text().trimmed();
    }

    return QString();
}

bool ComponentInitializer::showMessageBox(QWidget *parent, const QString &title, const QString &text, QMessageBox::Icon icon,bool showCancelBtn)
{
    QMessageBox msgBox(parent);
    msgBox.setWindowTitle(title);
    msgBox.setIcon(icon);
    msgBox.setText(text);

    // 按钮样式
    QString buttonStyle =
        "QPushButton {"
        " background-color: #2e2f38;"
        " border: none;"
        " border-radius: 4px;"
        " padding: 6px 12px;"
        " color: white;"
        "}"
        "QPushButton:hover { background-color: #3c3d47; }"
        "QPushButton:pressed { background-color: #50515b; }";

    QPushButton *okButton = msgBox.addButton("确定", QMessageBox::AcceptRole);
    okButton->setStyleSheet(buttonStyle);
    if(showCancelBtn)
    {
        QPushButton *cancelButton = msgBox.addButton("取消", QMessageBox::RejectRole);
        cancelButton->setStyleSheet(buttonStyle);
    }
    // 整体样式
    msgBox.setStyleSheet(
        "QMessageBox {"
        "background-color: #1e1f25;"
        "border-radius: 8px;"
        "color: white;"
        "}"
        "QMessageBox QLabel {"
        "color: white;"
        "}"
        );

    // 执行
    msgBox.exec();

    if (msgBox.clickedButton() == okButton) {
        return true;
    } else {
        return false;
    }

}

QWidget* ComponentInitializer::createSourceItem(QWidget *sourceWidget, VideoSource* newSource, const QIcon& icon)
{
    QWidget* sourceItem = new QWidget;
    sourceItem->setFixedHeight(30);
    sourceItem->setAutoFillBackground(true);
    sourceItem->setStyleSheet("QWidget {background-color: transparent; border-radius: 5px;border:none;}"
                              "QWidget:hover {background-color: #2b2d35;}");

    QHBoxLayout* layout = new QHBoxLayout(sourceItem);
    layout->setContentsMargins(8, 0, 8, 0);
    layout->setSpacing(2);

    QLabel* iconLabel = new QLabel;
    iconLabel->setPixmap(icon.pixmap(16, 16));
    iconLabel->setFixedWidth(30);
    iconLabel->setStyleSheet("border:none;");

    QLabel* textLabel = new QLabel(newSource->name());
    textLabel->setStyleSheet("border:none;color: white; font-size: 13px;");

    layout->addWidget(iconLabel);
    layout->addWidget(textLabel);


    QPushButton* hideBtn = new QPushButton;
    if(newSource->isVisible())
        hideBtn->setIcon(QIcon(":/sources/present.png"));
    else
        hideBtn->setIcon(QIcon(":/sources/hide_grey.png"));

    hideBtn->setIconSize(QSize(18,18));
    hideBtn->setFixedSize(24, 24);
    hideBtn->setStyleSheet("background: transparent; border: none;");
    layout->addWidget(hideBtn);

    QPushButton* lockBtn = new QPushButton;
    if(newSource->isLocked())
        lockBtn->setIcon(QIcon(":/sources/lock.png"));
    else
        lockBtn->setIcon(QIcon(":/sources/unlock_grey.png"));

    hideBtn->setIconSize(QSize(18,18));
    lockBtn->setFixedSize(24, 24);
    lockBtn->setStyleSheet("background: transparent; border: none;");
    layout->addWidget(lockBtn);

    connect(hideBtn, &QPushButton::clicked, [=]() {
        if(newSource->isVisible())
        {
            newSource->setVisible(false);
            hideBtn->setIcon(QIcon(":/sources/hide_grey.png"));
        }
        else{
            newSource->setVisible(true);
            hideBtn->setIcon(QIcon(":/sources/present.png"));
        }

    });

    connect(lockBtn, &QPushButton::clicked, [=]() {
        if(newSource->isLocked())
        {
            newSource->setLocked(false);
            lockBtn->setIcon(QIcon(":/sources/unlock_grey.png"));
        }
        else{
            newSource->setLocked(true);
            lockBtn->setIcon(QIcon(":/sources/lock.png"));
        };
    });
    sourceItem->installEventFilter(m_selectFilter);
    // 添加到 sourceWidget 的布局中
    QVBoxLayout* listLayout = qobject_cast<QVBoxLayout*>(sourceWidget->layout());
    if (listLayout) {
        listLayout->addWidget(sourceItem);
    }

    return sourceItem;
}


QFrame *ComponentInitializer::createIconPopup(QWidget *mainwindow, const QList<QPushButton *> &buttons)
{
    auto* popup = new QFrame(mainwindow, Qt::Popup | Qt::FramelessWindowHint);

    popup->setStyleSheet("QFrame { background-color: #13141A; border-radius: 5px; }");
    popup->setFixedWidth(150);

    auto* layout = new QVBoxLayout(popup);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->setAlignment(Qt::AlignTop);
    for (auto* btn : buttons) {
        layout->addWidget(btn);
    }

    return popup;
}

QPushButton *ComponentInitializer::createIconButton(const QString &text, QWidget *mainwindow,const QIcon &icon)
{

    QPushButton *btn = nullptr;

    if (!icon.isNull()) {
        btn = new QPushButton(icon, " " + text, mainwindow);
    } else {
        btn = new QPushButton(text, mainwindow);
    }

    btn->setFixedHeight(32);
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btn->setStyleSheet(R"(
        QPushButton {
            background-color: #13141A;
            color: white;
            text-align: left;
            padding-left: 8px;
            border: none;
            border-radius: 5px;
        }
        QPushButton:hover {
            background-color: #284CB8;
        })");
    return btn;
}



QLabel *ComponentInitializer::createLabel(QWidget *mainwindow, const QString &text, const QString &styleSheet)
{
    QLabel* label = new QLabel(text,mainwindow);
    label->setStyleSheet(styleSheet);
    return label;
}

QPushButton *ComponentInitializer::createButton(QWidget *mainwindow, const QString &Path)
{
    QPushButton* button = new QPushButton();

    QString styleSheet = R"(
        QPushButton{
            min-width:20px;
            min-height:20px;
            max-width:20px;
            max-height:20px;
        }
    )";
    button->setStyleSheet(styleSheet);
    button->setIcon(QIcon(Path));
    button->setIconSize(QSize(20,20));

    return button;
}

void ComponentInitializer::createCameraTools(QWidget *toolsWidget,VideoSource* cameraSource)
{
    QHBoxLayout* layout = qobject_cast<QHBoxLayout*>(toolsWidget->layout());
    if (!layout) return;
    QPushButton* activeBtn = new QPushButton(toolsWidget);
    if(cameraSource->isActive())
        activeBtn->setText("取消激活");
    else
        activeBtn->setText("激活");

    activeBtn->setStyleSheet(R"(QPushButton{
                                    max-width:70px;
                                    min-width:70px;
                                    max-height:28px;
                                    min-height:28px;
                                    border-radius:3px;
                                    background-color:#3C404D;
                                    color:white;
                                }

                                QPushButton:hover{
                                    background-color:#464B59;
                                    border:0.5px solid #5b6174;
                                })");

    layout->addWidget(activeBtn);
    // layout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
    layout->addStretch(1);
    connect(activeBtn,&QPushButton::clicked,[=](){
        if(cameraSource->isActive())
        {
            qDebug() << "unActive";
            activeBtn->setText("激活");
            cameraSource->setActive(false);
        }
        else
        {
            qDebug() << "Active";
            activeBtn->setText("取消激活");
            cameraSource->setActive(true);
        }

    });
}

void ComponentInitializer::createDesktopTools(QWidget *toolsWidget,QWidget *mainwindow,VideoSource* desktopSource)
{
    QHBoxLayout* layout = qobject_cast<QHBoxLayout*>(toolsWidget->layout());
    if (!layout) return;
    auto source = dynamic_cast<DesktopSource*>(desktopSource);

    QLabel* screenLabel = new QLabel("显示器",toolsWidget);
    QComboBox* screenCBBox = new QComboBox(toolsWidget);
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        QScreen* screen = screens[i];
        QString name = screen->name();
        QRect geometry = screen->geometry();
        QString label = QString(": %1x%2 @ %3 (显示器 %4)")
                            .arg(geometry.width())
                            .arg(geometry.height())
                            .arg(i)
                            .arg(name);

        screenCBBox->addItem(label, QVariant::fromValue(i));

    }
    screenCBBox->setCurrentIndex(source->screenSetting().screenIndex);
    screenCBBox->setProperty("sourceId", desktopSource->sourceId());

    MainWindow* mainWin = qobject_cast<MainWindow*>(mainwindow);
    // 关联切换信号到主窗口的槽函数（假设主窗口有onScreenComboBoxChanged方法）
    connect(screenCBBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     mainWin, &MainWindow::on_screenComboBox_changed);


    layout->addWidget(screenLabel);
    layout->addWidget(screenCBBox);
    layout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

}

void ComponentInitializer::createFileTools(QWidget *toolsWidget,QWidget *mainwindow,VideoSource* fileSource,SceneManager* sceneManager)
{
    QHBoxLayout* layout = qobject_cast<QHBoxLayout*>(toolsWidget->layout());
    if (!layout) return;
    MainWindow* mainWin = qobject_cast<MainWindow*>(mainwindow);
    int sourceId = fileSource->sourceId();

    // 优先从缓存中获取，不存在则创建
    ControlBar* controlBar = controlBarCache_.value(sourceId, nullptr);
    if (!controlBar) {
        controlBar = new ControlBar(mainwindow);

        // connect(sceneManager,&SceneManager::sceneIdUpdate,controlBar,&ControlBar::setCurrentSceneId);
        controlBar->setMediaSourceContrller(mainWin->getMediaSourceController());
        controlBarCache_.insert(sourceId, controlBar); // 加入缓存
    }

    // 更新源ID并刷新信息（无需重新创建）
    controlBar->setCurrentSceneId(sceneManager->currentScene()->id());
    controlBar->setCurrentSourceId(sourceId);
    layout->addWidget(controlBar);
}

void ComponentInitializer::createTextTools(QWidget *toolsWidget, QWidget *mainwindow, VideoSource *textSource)
{
    QHBoxLayout* layout = qobject_cast<QHBoxLayout*>(toolsWidget->layout());
    if (!layout) return;
    TextSource* source = dynamic_cast<TextSource*>(textSource);
    if (!source || !toolsWidget) {
        qCritical() << "createTextTools: 无效的TextSource或toolsWidget";
        return;
    }

    // 字体按钮
    QPushButton* fontBtn = new QPushButton(toolsWidget);
    QPointer<QPushButton> fontBtnPtr = fontBtn;

    fontBtnPtr->setMinimumSize(160, 28);  // 强制宽160px，高28px
    fontBtnPtr->setMaximumSize(160, 28);
    fontBtnPtr->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    fontBtnPtr->setStyleSheet(R"(
        QPushButton{
            min-width:160px; max-width:160px;
            min-height:28px; max-height:28px;
            border-radius:3px;
            background-color:#3C404D;
            color:white;
            text-align:left;
            padding-left:8px;
            font-size:13px;
        }
        QPushButton:hover{
            background-color:#464B59;
            border:0.5px solid #5b6174;
        }
    )");

    auto updateFontBtnText = [=]() {
        if (!fontBtnPtr) return;
        QFont font = source->textSetting().font;
        QString btnText = QString("字体：%1 %2pt")
                              .arg(font.family().left(12))
                              .arg(font.pointSize());
        fontBtnPtr->setText(btnText);
    };
    updateFontBtnText();

    // 颜色按钮
    QPushButton* colorBtn = new QPushButton(toolsWidget);
    QPointer<QPushButton> colorBtnPtr = colorBtn;
    colorBtnPtr->setMinimumSize(120, 28);
    colorBtnPtr->setMaximumSize(120, 28);
    colorBtnPtr->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);


    auto updateColorBtnStyle = [=]() {
        if(!colorBtnPtr) return;
        QColor textColor(source->textSetting().color);
        QString textColorHex = textColor.name();
        int gray = (textColor.red() * 299 + textColor.green() * 587 + textColor.blue() * 114) / 1000;
        QString btnTextColor = (gray > 128) ? "color:black;" : "color:white;";

        QString fullStyle = QString(R"(
            QPushButton{
                border-radius:3px;
                background-color:%1;
                %2
                text-align:center;
                font-size:13px;
            }
            QPushButton:hover{
                border:0.5px solid #5b6174;
            }
        )").arg(textColorHex, btnTextColor);

        colorBtnPtr->setStyleSheet(fullStyle);
        colorBtnPtr->setText(textColorHex); // 显示颜色码
    };
    updateColorBtnStyle(); // 首次初始化

    // 文本框
    QLineEdit* textEdit = new QLineEdit(toolsWidget);
    QPointer<QLineEdit> textEditPtr = textEdit;
    textEditPtr->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    textEditPtr->setMinimumSize(500, 28);
    textEditPtr->setMaximumSize(QWIDGETSIZE_MAX, 28);
    textEditPtr->setStyleSheet(R"(
        QLineEdit{
            background-color: #3C404D;
            border: 1px solid #3C3C3C;
            border-radius: 5px;
            padding: 4px 8px;
            color: white;
            font-size: 13px;
        }
        QLineEdit:focus{
            border-color: #5b6174; /* 聚焦时边框高亮 */
            outline: none;
        }
    )");

    auto updateTextEdit = [=]() {
        if(!textEditPtr) return;
        TextSetting setting = source->textSetting();
        textEditPtr->setText(setting.text);
        textEditPtr->setFont(setting.font);
        textEditPtr->setStyleSheet(textEdit->styleSheet() +
                                QString("color:%1;").arg(setting.color));
    };
    updateTextEdit();

    // 按钮信号槽
    QObject::connect(fontBtn, &QPushButton::clicked,toolsWidget, [=]() {
        bool isConfirmed = false;
        QFont currentFont = source->textSetting().font;
        QFont selectedFont = QFontDialog::getFont(&isConfirmed, currentFont, toolsWidget, "选择文本字体");
        if (isConfirmed && selectedFont != currentFont) {
            TextSetting newSetting = source->textSetting();
            newSetting.font = selectedFont;
            source->setTextSetting(newSetting);
        }
    });

    QObject::connect(colorBtn, &QPushButton::clicked,toolsWidget, [=]() {
        QColor currentColor(source->textSetting().color);
        QColor selectedColor = QColorDialog::getColor(currentColor, toolsWidget, "选择文本颜色", QColorDialog::ShowAlphaChannel);
        if (selectedColor.isValid() && selectedColor != currentColor) {
            TextSetting newSetting = source->textSetting();
            newSetting.color = selectedColor.name();
            source->setTextSetting(newSetting);
        }
    });

    QObject::connect(textEdit, &QLineEdit::editingFinished,toolsWidget, [=]() {
        QString newText = textEdit->text().trimmed();
        QString currentText = source->textSetting().text;

        if (newText != currentText) {
            TextSetting newSetting = source->textSetting();
            newSetting.text = newText;
            source->setTextSetting(newSetting);
        }
    });

    QObject::connect(source, &TextSource::textSettingChanged,toolsWidget, [=](const TextSetting& newSetting, const QSize&) {
        Q_UNUSED(newSetting); // 直接从source获取最新配置（避免参数滞后）
        updateFontBtnText();   // 同步字体按钮
        updateColorBtnStyle(); // 同步颜色按钮
        updateTextEdit();      // 同步文本编辑框
    });

    layout->setSpacing(6);
    layout->addWidget(fontBtn);
    layout->addWidget(colorBtn);
    layout->addWidget(textEdit);
    layout->addStretch(1);

}

void ComponentInitializer::clearToolsWidget(QWidget *mainwindow)
{
    // 清除源名称和图标
    QLabel* sourceIconLabel = mainwindow->findChild<QLabel*>("sourceIconLabel");
    QLabel* sourceNameLabel = mainwindow->findChild<QLabel*>("sourceNameLabel");

    sourceIconLabel->clear();
    sourceNameLabel->setText("未选择源");

    // 清空toolsWidget
    QWidget* toolsWidget = mainwindow->findChild<QWidget*>("toolsWidget");
    if (!toolsWidget) return;

    QLayout* layout = toolsWidget->layout();
    if (!layout) return;

    QLayoutItem* item;
    while ((item = layout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            if(isCachedControlBar(item->widget()))
                item->widget()->setParent(nullptr);
            else
                delete item->widget();
        }
        delete item;
    }

    if(m_currentSourceItem)
    {
        m_currentSourceItem = nullptr;
    }

}

void ComponentInitializer::clearWidget(QWidget *widget)
{
    if (!widget) return;

    if (m_selectFilter) {
        m_selectFilter->clearSelection();
    }
    QLayout* layout = widget->layout();
    if (!layout) return;

    QLayoutItem* item;
    while ((item = layout->takeAt(0)) != nullptr) {
        QWidget* widget = item->widget();
        if (widget) {
            widget->deleteLater();
        }
        delete item;
    }
}

void ComponentInitializer::removeSourceItem(QWidget* sourceWidget, QWidget* targetItem)
{
    if (!sourceWidget || !targetItem) return;

    // 从m_sourceItemMap中移除targetItem
    int sourceId = targetItem->property("sourceId").toInt();
    unregisterSourceItem(sourceId);

    // 从布局中移除并销毁空间
    QLayout* layout = sourceWidget->layout();
    if (!layout) return;

    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem* item = layout->itemAt(i);
        if (item && item->widget() == targetItem) {

            if (m_selectFilter && m_selectFilter->selectedWidget() == targetItem) {
                m_selectFilter->clearSelection();
            }

            layout->takeAt(i);
            targetItem->deleteLater();
            delete item;
            break;
        }
    }
}

void ComponentInitializer::removeControlBarCaches(int sourceId)
{
    ControlBar* bar = controlBarCache_.take(sourceId);
    if (bar) {
        delete bar;
    }
}

void ComponentInitializer::clear()
{
    qDeleteAll(controlBarCache_);
    controlBarCache_.clear();
    if (m_selectFilter) {
        m_selectFilter->clearSelection();
    }
}

void ComponentInitializer::on_widgetSelected(QWidget *selectedWidget, QWidget *mainwindow,SceneManager* sceneManager)
{
    if(!selectedWidget) return;
    m_currentSourceItem = selectedWidget;

    QLabel* sourceIconLabel = mainwindow->findChild<QLabel*>("sourceIconLabel");
    QLabel* sourceNameLabel = mainwindow->findChild<QLabel*>("sourceNameLabel");
    QWidget* toolsWidget = mainwindow->findChild<QWidget*>("toolsWidget");

    QHBoxLayout* layout = qobject_cast<QHBoxLayout*>(toolsWidget->layout());
    if (!layout) {
        layout = new QHBoxLayout(toolsWidget);
        layout->setContentsMargins(10, 0, 0, 0);
        layout->setSpacing(5);
        toolsWidget->setLayout(layout);
    }


    QLayoutItem* item;
    while ((item = layout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            if(isCachedControlBar(item->widget()))
                item->widget()->setParent(nullptr);
            else
                delete item->widget();
        }
        delete item;
    }
    QString sourceName = selectedWidget->property("nickName").toString();
    int sourceId = selectedWidget->property("sourceId").toInt();
    QIcon sourceIcon = qvariant_cast<QIcon>(selectedWidget->property("sourceIcon"));

    sourceNameLabel->setText(sourceName);
    sourceIconLabel->setPixmap(sourceIcon.pixmap(20,20));

    VideoSource* source = sceneManager->currentScene()->findVideoSourceById(sourceId);
    switch (source->type()) {
    case VideoSourceType::Camera:
        createCameraTools(toolsWidget,source);
        break;
    case VideoSourceType::Desktop:
        createDesktopTools(toolsWidget,mainwindow,source);
        break;
    case VideoSourceType::Media:
        createFileTools(toolsWidget,mainwindow,source,sceneManager);
        break;
    case VideoSourceType::Text:
        createTextTools(toolsWidget,mainwindow,source);
        break;
    default:
        break;
    }
}

bool ComponentInitializer::isCachedControlBar(QWidget *widget)
{
    if(!widget) return false;
    ControlBar* controlbar = dynamic_cast<ControlBar*>(widget);
    if(!controlbar) return false;
    if(!controlBarCache_.contains(controlbar->currentSourceId())) return false;

    return true;
}


QString ComponentInitializer::requestSourceName(QWidget *mainwindow, SceneManager *sceneManager, const QString &title, const QString &label, const QString &defaultName)
{
    if (!sceneManager->currentScene()) {
        showMessageBox(mainwindow, "警告", "请先创建一个场景！",QMessageBox::Warning);
        return QString();
    }

    return showCreateDialog(title, label, defaultName, mainwindow);
}

QWidget *ComponentInitializer::getCurrentSourceItem() const
{
    return m_currentSourceItem;
}

QWidget *ComponentInitializer::getSourceItemById(int sourceId)
{
    if (m_sourceItemMap.contains(sourceId)) {
        return m_sourceItemMap[sourceId];
    }
    return nullptr;
}

void ComponentInitializer::setCurrentSourceItem(QWidget* widget)
{
    m_currentSourceItem = widget;

}

void ComponentInitializer::registerSourceItem(int sourceId, QWidget *widget)
{
    m_sourceItemMap[sourceId] = widget;
}

void ComponentInitializer::unregisterSourceItem(int sourceId)
{
    QWidget* item = m_sourceItemMap.take(sourceId);
    if (item) {
        item->deleteLater();
    }
}

void ComponentInitializer::clearSourceItem()
{
    // qDeleteAll(m_sourceItemMap);
    m_sourceItemMap.clear();
}

SelectionEventFilter *ComponentInitializer::sourceSelectionFilter() const
{
    return m_selectFilter;
}



