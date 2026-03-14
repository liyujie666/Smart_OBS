#ifndef TEXTSOURCE_H
#define TEXTSOURCE_H
#include "component/textsettingdialog.h"
#include "videosource.h"
#include <QObject>
class TextSource : public VideoSource
{
    Q_OBJECT
public:
    explicit TextSource(int sourceId,int sceneId,TextSetting textSetting, QObject* parent = nullptr);
    ~TextSource();

    int open() override;
    void close() override;
    AVFormatContext* getFormatContext() override;
    QString name() const override;
    VideoSourceType type() const override;

    TextSetting textSetting() const { return textSetting_; }
    void setTextSetting(const TextSetting& newSetting);

    QSize frameSize() const { return frameSize_; }
    void setFrameSize(const QSize& size) { frameSize_ = size; }
    QSize calculateTextSize() const;

signals:
    void textSettingChanged(const TextSetting& newSetting, const QSize& frameSize);
private:
    TextSetting textSetting_;
    QSize frameSize_{800, 400};// 文本帧默认尺寸（宽800，高400）
    bool isOpened_{false};     // 标记源是否已打开
};

#endif // TEXTSOURCE_H
