// #ifndef DESKTOPSOURCE_H
// #define DESKTOPSOURCE_H

// #include "videosource.h"
// #include "component/screensettingdialog.h"
// #include "dxgi/dxgidesktopduplicator.h"

// class DesktopSource : public VideoSource
// {
// public:
//     DesktopSource(int id, ScreenSetting setting);

//     int open() override;
//     AVFormatContext* getFormatContext() override;

//     int sourceId() const override;
//     QString name() const override;
//     SourceType type() const override;


//     void setPriority(int) override;
//     int priority() const override;

//     // void setGeometry(const QRectF&) override;
//     // QRectF geometry() const override;

//     void setOpacity(float) override;
//     float opacity() const override;

//     void active() override;
//     void unactive() override;
//     bool isActive() const override;

//     // 桌面画面参数
//     ScreenSetting screenSetting() const;
//     void setFramerate(int framerate);
//     void setResolution(QString resolution);
//     void setOffset(QString offsetX,QString offsetY);
//     void setCaptureType(DesktopCaptureType type);
//     int screenIndex() const;
//     DxgiDesktopDuplicator* dxgiDuplicator();

// private:
//     int m_id;
//     int m_screenIndex;
//     bool m_visible = true;
//     bool m_locked = false;
//     int m_priority = 0;
//     float m_opacity = 1.0f;
//     bool m_active = true;

//     int m_frameRate = 30;
//     QString m_nickName;
//     QString m_resulotion = "1960x1080";
//     QString m_offsetX;
//     QString m_offsetY;

//     ScreenSetting m_setting;

//     AVFormatContext* desktopFmtCtx_ = nullptr;
//     QString inputFmt = "gdigrab";
//     std::unique_ptr<DxgiDesktopDuplicator> dxgiDuplicator_;

// };

// #endif // DESKTOPSOURCE_H
#ifndef DESKTOPSOURCE_H
#define DESKTOPSOURCE_H

#include "source/video/videosource.h"
#include "component/screensettingdialog.h"
#include "dxgi/dxgidesktopduplicator.h"

struct DisplayInfo {
    int adapterIndex;   // 所属适配器索引
    int outputIndex;    // 显示器在适配器中的索引
    QString deviceName; // 设备名称（如 "\\.\DISPLAY1"）
};

class DesktopSource : public VideoSource
{
public:
    DesktopSource(int sourceId,int sceneId,ScreenSetting setting);
    ~DesktopSource();

    int open() override;
    void close() override;
    AVFormatContext* getFormatContext() override;
    QString name() const override;
    VideoSourceType type() const override;

    // 桌面画面参数
    void serSetting(const ScreenSetting& setting);
    ScreenSetting screenSetting() const;
    void setFramerate(int framerate);
    void setResolution(QString resolution);
    void setOffset(QString offsetX,QString offsetY);
    void setCaptureType(DesktopCaptureType type);
    int screenIndex() const;
    DxgiDesktopDuplicator* dxgiDuplicator();
    std::vector<DisplayInfo> enumerateAllDisplays();

private:
    int m_screenIndex;
    int m_frameRate = 30;
    QString m_nickName;
    QString m_resulotion = "1960x1080";
    QString m_offsetX;
    QString m_offsetY;

    ScreenSetting m_setting;

    AVFormatContext* desktopFmtCtx_ = nullptr;
    QString inputFmt = "gdigrab";
    std::unique_ptr<DxgiDesktopDuplicator> dxgiDuplicator_;

};

#endif // DESKTOPSOURCE_H
