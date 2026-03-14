#include "muxermanager.h"
#include <QDebug>

MuxerManager::MuxerManager(QObject* parent)
    : QObject(parent)
{
}

MuxerManager::~MuxerManager()
{
    stopAll();
    for (auto& t : muxers_) {
        delete t.muxer;
    }
    muxers_.clear();
}

bool MuxerManager::addOutput(MuxerType type, const QString& url,const QString& format)
{
    std::lock_guard<std::mutex> lock(mutex_);
    bool isExist = false;
    for (const auto& m : muxers_) {
        if (m.url == url) {
            qWarning() << "Output already exists for url:" << url;
            isExist = true;
            break;
        }
    }

    if(!isExist)
    {
        Muxer* muxer = new Muxer();
        bool ok = muxer->init(url, type,format);
        if (!ok) {
            delete muxer;
            return false;
        }

        muxers_.append({type, url, muxer});
    }

    return true;
}

void MuxerManager::removeOutput(MuxerType type)
{
    std::lock_guard<std::mutex> lock(mutex_); // 线程安全锁

    // 从后往前遍历直接删除，避免QVector索引偏移
    for (int i = muxers_.size() - 1; i >= 0; --i) {
        if (muxers_[i].type == type) {
            // 释放资源
            muxers_[i].muxer->writeTrailer();
            delete muxers_[i].muxer;
            // 从向量中删除
            muxers_.remove(i);
        }
    }
}

bool MuxerManager::startAll()
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& target : muxers_) {
        if (!target.muxer->writeHeader()) {
            qWarning() << "Failed to write header for:" << target.url;
            return false;
        }
    }
    return true;
}

void MuxerManager::stopAll()
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& target : muxers_) {
        target.muxer->writeTrailer();
        delete target.muxer; // 停止后删除muxer
    }
    muxers_.clear(); // 清空列表，允许下次重新添加
}

bool MuxerManager::addStream(AVCodecContext* codecCtx, AVMediaType type)
{
    std::lock_guard<std::mutex> lock(mutex_);
    bool ok = true;
    for (auto& target : muxers_) {
        ok &= target.muxer->addStream(codecCtx, type);
    }
    return ok;
}

void MuxerManager::writePacket(AVPacket* pkt, AVMediaType type)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& target : muxers_) {
        if(!target.muxer->writePacket(pkt, type))
            // qDebug() << "type " << type << "writePacket sucessfully!";
            qDebug() << "type " << type << "writePacket failed!";
    }
}

bool MuxerManager::isActive() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return !muxers_.isEmpty();
}
