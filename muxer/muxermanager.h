#ifndef MUXERMANAGER_H
#define MUXERMANAGER_H
#include <QObject>
#include <QMap>
#include <QString>
#include <mutex>
#include "muxer/muxer.h"



class MuxerManager : public QObject
{
    Q_OBJECT

public:
    explicit MuxerManager(QObject* parent = nullptr);
    ~MuxerManager();

    // 添加输出：录制或推流
    bool addOutput(MuxerType type, const QString& url,const QString& format);

    void removeOutput(MuxerType type);

    // 初始化所有 Muxer（写入 header）
    bool startAll();

    // 写入尾部并关闭
    void stopAll();

    // 添加编码器通道（video/audio）给每个 Muxer
    bool addStream(AVCodecContext* codecCtx, AVMediaType type);


    // 写入封装 AVPacket 到所有 Muxer
    void writePacket(AVPacket* pkt, AVMediaType type);

    // 检查是否有任何 Muxer 正在工作
    bool isActive() const;

private:
    struct MuxerTarget {
        MuxerType type;
        QString url;
        Muxer* muxer = nullptr;
    };

    QVector<MuxerTarget> muxers_;
    mutable std::mutex mutex_;
};

#endif // MUXERMANAGER_H
