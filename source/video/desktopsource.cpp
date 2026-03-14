#include "source/video/desktopsource.h"
#include "component/screensettingdialog.h"
#include <QDebug>
DesktopSource::DesktopSource(int sourceId,int sceneId, ScreenSetting setting)
    : VideoSource(sourceId,sceneId), m_setting(setting)
{

}

DesktopSource::~DesktopSource()
{
    // if (desktopFmtCtx_) {
    //     avformat_close_input(&desktopFmtCtx_);
    // }
}

int DesktopSource::open()
{
    if(m_setting.captureType == DesktopCaptureType::DXGI)
    {
        dxgiDuplicator_  = std::make_unique<DxgiDesktopDuplicator>();
        auto allDisplays = enumerateAllDisplays();
        if(allDisplays.size() >= 1)
        {
            int adapterIdx = allDisplays[m_setting.screenIndex].adapterIndex;
            int outputIdx = allDisplays[m_setting.screenIndex].outputIndex;
            if (!dxgiDuplicator_->initialize(adapterIdx,outputIdx)) {
                qWarning() << "DXGI 初始化失败";
                return -1;
            }
            return 0;
        }

    }
    else
    {
        AVDictionary *opts = nullptr;
        const AVInputFormat *videoInputFmt = av_find_input_format(inputFmt.toStdString().c_str());
        av_dict_set(&opts, "framerate", "30", 0);
        av_dict_set(&opts, "video_size", m_setting.resolution.toStdString().c_str(), 0);
        av_dict_set(&opts, "offset_x", QString::number(m_setting.offset_X).toStdString().c_str(), 0);
        av_dict_set(&opts, "offset_y", QString::number(m_setting.offset_Y).toStdString().c_str(), 0);
        // av_dict_set(&opts, "use_wallclock_as_timestamps", "0", 0);
        // av_dict_set(&opts, "rtbufsize", "100M", 0);

        if(avformat_open_input(&desktopFmtCtx_,"desktop",videoInputFmt,&opts) < 0)
        {
            qDebug() << "desktop avformat_open_input failed!";
            return -1;
        }

        int ret = avformat_find_stream_info(desktopFmtCtx_,nullptr);
        if(ret < 0){
            avformat_close_input(&desktopFmtCtx_);
            return ret;
        }
        return 0;
    }

}

void DesktopSource::close()
{
    if (desktopFmtCtx_) {
        avformat_close_input(&desktopFmtCtx_);
        desktopFmtCtx_ = nullptr;
    }
}


AVFormatContext *DesktopSource::getFormatContext()
{
    return desktopFmtCtx_;
}

QString DesktopSource::name() const { return m_setting.nickName; }
VideoSourceType DesktopSource::type() const { return VideoSourceType::Desktop; }

void DesktopSource::serSetting(const ScreenSetting &setting)
{
    m_setting = setting;
}

ScreenSetting DesktopSource::screenSetting() const
{
    return m_setting;
}


void DesktopSource::setFramerate(int framerate)
{
    m_frameRate = framerate;
}
void DesktopSource::setResolution(QString resolution)
{
    m_resulotion = resolution;
}

void DesktopSource::setOffset(QString offsetX, QString offsetY)
{
    m_offsetX = offsetX;
    m_offsetY = offsetY;


}

void DesktopSource::setCaptureType(DesktopCaptureType type)
{
    // captureType_ = type;
}

int DesktopSource::screenIndex() const { return m_screenIndex; }

DxgiDesktopDuplicator* DesktopSource::dxgiDuplicator()
{
    return dxgiDuplicator_.get();
}


std::vector<DisplayInfo> DesktopSource::enumerateAllDisplays() {
    std::vector<DisplayInfo> displays;
    ComPtr<IDXGIFactory1> factory;

    // 创建DXGI工厂
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(factory.GetAddressOf())))) {
        qWarning("CreateDXGIFactory1 failed");
        return displays;
    }

    // 枚举所有适配器（GPU）
    for (UINT adapterIndex = 0; ; adapterIndex++) {
        ComPtr<IDXGIAdapter1> adapter;
        HRESULT hr = factory->EnumAdapters1(adapterIndex, &adapter);
        if (hr == DXGI_ERROR_NOT_FOUND) break; // 没有更多适配器
        if (FAILED(hr)) continue;

        // 枚举当前适配器下的所有显示器
        for (UINT outputIndex = 0; ; outputIndex++) {
            ComPtr<IDXGIOutput> output;
            hr = adapter->EnumOutputs(outputIndex, &output);
            if (hr == DXGI_ERROR_NOT_FOUND) break; // 没有更多显示器
            if (FAILED(hr)) continue;

            // 获取显示器名称
            DXGI_OUTPUT_DESC desc;
            if (SUCCEEDED(output->GetDesc(&desc))) {
                DisplayInfo info;
                info.adapterIndex = adapterIndex;
                info.outputIndex = outputIndex;
                info.deviceName = QString::fromWCharArray(desc.DeviceName);
                displays.push_back(info);
                qDebug() << "找到显示器：" << info.deviceName
                         << " (adapter:" << info.adapterIndex
                         << ", output:" << info.outputIndex << ")";
            }
        }
    }
    return displays;
}
