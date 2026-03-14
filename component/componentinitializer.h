#ifndef COMPONENTINITIALIZER_H
#define COMPONENTINITIALIZER_H
#include <QObject>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QListWidget>
#include <QMenu>
#include <QStatusBar>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QWidget>
#include <QFrame>
#include <QMessageBox>
#include <QGraphicsDropShadowEffect>
#include "controlbar.h"
class SelectionEventFilter;
class SceneManager;
class VideoSource;
class ComponentInitializer : public QObject
{
    Q_OBJECT
public:
    static ComponentInitializer* getInstance();
    static void destroyInstance();

    void init(QWidget* mainwindow,SceneManager* sceneManager);

    // 创建场景弹窗
    QString showCreateSceneDialog(QWidget* mainwindow = nullptr);
    QString showCreateDialog(const QString& title,const QString& labelName,const QString& placeHolder,QWidget* mainwindow = nullptr);
    bool showMessageBox(QWidget* parent,
                        const QString& title,
                        const QString& text,
                        QMessageBox::Icon icon = QMessageBox::Information,
                        bool showCancelBtn = false);
    // 创建输入源Item
    QWidget* createSourceItem(QWidget* mainwindow,VideoSource* newSource, const QIcon& icon);
    // 输入源卡片弹窗
    QFrame* createIconPopup(QWidget* parent, const QList<QPushButton*>& buttons);
    // 输入源按钮
    QPushButton* createIconButton(const QString& text, QWidget* parent = nullptr,const QIcon& icon = QIcon());
    QString requestSourceName(QWidget* mainwindow,SceneManager* sceneManager,const QString& title,const QString& label,const QString& defaultName);

    void setCurrentSourceItem(QWidget* widget);
    void registerSourceItem(int sourceId, QWidget* widget);
    void unregisterSourceItem(int sourceId);
    void clearSourceItem();
    QWidget* getCurrentSourceItem() const;
    QWidget* getSourceItemById(int sourceId);
    SelectionEventFilter* sourceSelectionFilter() const;

    void clearToolsWidget(QWidget* mainwindow);
    void clearWidget(QWidget* widget);
    void removeSourceItem(QWidget* sourceWidget, QWidget* targetItem);
    void removeControlBarCaches(int sourceId);
    void clear();

private:
    ComponentInitializer() = default;
    ComponentInitializer(const ComponentInitializer&) = delete;
    ComponentInitializer& operator=(const ComponentInitializer&) = delete;
    ~ComponentInitializer();
    void initStatusBar(QWidget* mainwindow);
    void initSceneListWidget(QWidget* mainwindow);
    void initFilter(QWidget* mainwindow,SceneManager* sceneManager);

    QLabel* createLabel(QWidget* mainwindow,const QString& text,const QString& styleSheet = "");
    QPushButton* createButton(QWidget* mainwindow,const QString& Path);
    void createCameraTools(QWidget *toolsWidget,VideoSource* cameraSource);
    void createDesktopTools(QWidget *toolsWidget,QWidget *mainwindow,VideoSource* desktopSource);
    void createFileTools(QWidget *toolsWidget,QWidget *mainwindow,VideoSource* fileSource,SceneManager* sceneManager);
    void createTextTools(QWidget *toolsWidget,QWidget *mainwindow,VideoSource* textSource);
    void on_widgetSelected(QWidget* selectedWidget,QWidget* mainwindow,SceneManager* sceneManager);
    bool isCachedControlBar(QWidget* widget);

private:
    static ComponentInitializer* instance_;
    int sceneIndex = 2;
    SelectionEventFilter* m_selectFilter;
    QWidget* m_currentSourceItem;
    QMap<int, QWidget*> m_sourceItemMap;
    QMap<int, ControlBar*> controlBarCache_;

};

#endif // COMPONENTINITIALIZER_H
