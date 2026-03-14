#include "source/video/camerasource.h"
#include "component/camerasettingdialog.h"

CameraSource::CameraSource(int sourceId,int sceneId,CameraSetting setting)
    : VideoSource(sourceId,sceneId), m_setting(setting)
{

}

CameraSource::~CameraSource()
{
    // if (cameraFmtCtx_) {
    //     avformat_close_input(&cameraFmtCtx_);
    // }
    close();
}

int CameraSource::open()
{

    AVDictionary* opts = nullptr;
    const AVInputFormat* inputFmt = av_find_input_format(getPlatformDevicePrefix().toStdString().c_str());

    av_dict_set(&opts,"video_size",m_setting.resolution.toStdString().c_str(),0);
    av_dict_set(&opts,"framerate",QString::number(m_setting.frameRate).toStdString().c_str(),0);

    QString devicePath = "video=" + m_setting.deviceName;

    if (avformat_open_input(&cameraFmtCtx_, devicePath.toStdString().c_str(),inputFmt, &opts) < 0) {
        qWarning("无法打开摄像头输入: %s\n m_resluotion: %s\n m_frameRate: %s", devicePath.toStdString().c_str(),m_resoluotion.toStdString().c_str(),QString::number(m_frameRate).toStdString().c_str());
        return -1;
    }
    qWarning("摄像头输入: %s\n m_resluotion: %s\n m_frameRate: %s", devicePath.toStdString().c_str(),m_setting.resolution.toStdString().c_str(),QString::number(m_setting.frameRate).toStdString().c_str());

    int ret = avformat_find_stream_info(cameraFmtCtx_,nullptr);
    if(ret < 0){
        avformat_close_input(&cameraFmtCtx_);
        return ret;
    }

    return 0;
}

void CameraSource::close()
{
    if (cameraFmtCtx_) {
        avformat_close_input(&cameraFmtCtx_);
        cameraFmtCtx_ = nullptr;
    }
}

AVFormatContext *CameraSource::getFormatContext()
{
    return cameraFmtCtx_;
}

QString CameraSource::name() const { return m_setting.nickName; }
VideoSourceType CameraSource::type() const { return VideoSourceType::Camera; }

void CameraSource::serSetting(const CameraSetting &setting)
{
    m_setting = setting;
}

CameraSetting CameraSource::cameraSetting() const
{
    return m_setting;
}

void CameraSource::setFramerate(int framerate)
{
    m_frameRate = framerate;
}
void CameraSource::setResolution(QString resolution)
{
    m_resoluotion = resolution;
}

QString CameraSource::getPlatformDevicePrefix() const
{
#ifdef Q_OS_WINDOWS
    return "dshow"; // DirectShow
#elif defined(Q_OS_MACOS)
    return "avfoundation"; // AVFoundation
#else
    return "v4l2"; // Video4Linux2
#endif
}

QString CameraSource::deviceName() const { return m_deviceName; }

