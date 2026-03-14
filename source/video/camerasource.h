// #ifndef CAMERASOURCE_H
// #define CAMERASOURCE_H

// #include "videosource.h"
// #include "component/camerasettingdialog.h"
// class CameraSource : public VideoSource
// {
// public:
//     CameraSource(int id, const QString& deviceName,const QString& sourceName,const int frameRate = 30,const QString& resolution = "1280x720",const QString& videoFormat = "");
//     CameraSource(int id, CameraSetting setting);
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

//     // 摄像头参数
//     CameraSetting cameraSetting() const;

//     void setFramerate(int framerate);
//     void setResolution(QString resolution);
//     QString getPlatformDevicePrefix() const;

//     QString deviceName() const;

// private:
//     int m_id;
//     QString m_deviceName;
//     QString m_SourceName;
//     bool m_visible = true;
//     bool m_locked = false;
//     int m_priority = 0;
//     float m_opacity = 1.0f;
//     bool m_active = true;

//     int m_frameRate;
//     int m_cwidth = 1960;
//     int m_cheight = 1080;
//     QString m_resoluotion;
//     QString m_videoFormat;

//     CameraSetting m_setting;
//     AVFormatContext* cameraFmtCtx_ = nullptr;

// };

// #endif // CAMERASOURCE_H
#ifndef CAMERASOURCE_H
#define CAMERASOURCE_H

#include "source/video/videosource.h"
#include "component/camerasettingdialog.h"
class CameraSource : public VideoSource
{
public:
    CameraSource(int sourceId,int sceneId, CameraSetting setting);
    ~CameraSource();

    int open() override;
    void close() override;
    AVFormatContext* getFormatContext() override;
    QString name() const override;
    VideoSourceType type() const override;

    // 摄像头参数
    void serSetting(const CameraSetting& setting);
    CameraSetting cameraSetting() const;

    void setFramerate(int framerate);
    void setResolution(QString resolution);
    QString getPlatformDevicePrefix() const;

    QString deviceName() const;

private:
    QString m_deviceName;
    QString m_SourceName;

    int m_frameRate;
    int m_cwidth = 1960;
    int m_cheight = 1080;
    QString m_resoluotion;
    QString m_videoFormat;

    CameraSetting m_setting;
    AVFormatContext* cameraFmtCtx_ = nullptr;

};

#endif // CAMERASOURCE_H
