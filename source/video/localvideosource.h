#ifndef LOCALVIDEOSOURCE_H
#define LOCALVIDEOSOURCE_H

#include "source/video/videosource.h"
#include "source/audio/mediaaudiosource.h"
#include "component/filesettingdialog.h"

class LocalVideoSource : public VideoSource
{
public:
    LocalVideoSource(int sourceId,int sceneId,FileSetting setting);
    ~LocalVideoSource();

    int open() override;
    void close() override;
    AVFormatContext* getFormatContext() override;
    QString name() const override;
    VideoSourceType type() const override;
    double duration() const;

    int audioIndex() const;
    int videoIndex() const;
    QString filePath() const;
    void serSetting(const FileSetting& setting);
    FileSetting fileSetting() const;
    void setMediaASource(MediaAudioSource* audioSource);
    void setFormatContext(AVFormatContext* fmtCtx);
    void seek(int64_t us);
    MediaAudioSource* getAssociatedAudioSource();
    std::tuple<int, int, AVPixelFormat, AVRational> getVideoFilterParams() const;
private:
    QString m_filePath;
    FileSetting m_setting;
    AVFormatContext* localVideoFmtCtx_ = nullptr;
    MediaAudioSource* audioSource_ = nullptr;
    int audioIndex_ = -1;
    int videoIndex_ = -1;
};

#endif // LOCALVIDEOSOURCE_H
