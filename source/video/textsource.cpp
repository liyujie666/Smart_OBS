#include "textsource.h"

TextSource::TextSource(int sourceId,int sceneId,TextSetting textSetting, QObject* parent)
    :VideoSource(sourceId,sceneId),textSetting_(textSetting)
{
    setFrameSize(calculateTextSize());
}

TextSource::~TextSource()
{

}

int TextSource::open()
{
    if (isOpened_) return 0; // 避免重复打开
    isOpened_ = true;
    qDebug() << "TextSource opened (sourceId:" << m_sourceId << ")";
    return 0;
}

void TextSource::close()
{
    if (!isOpened_) return;
    isOpened_ = false;
    qDebug() << "TextSource closed (sourceId:" << m_sourceId << ")";
}

AVFormatContext *TextSource::getFormatContext()
{
    return nullptr;
}

QString TextSource::name() const
{
    return textSetting_.nickName;
}

VideoSourceType TextSource::type() const
{
    return VideoSourceType::Text;
}

void TextSource::setTextSetting(const TextSetting& newSetting)
{
    // 对比配置是否真的变化
    if (newSetting != textSetting_)
    {
        textSetting_ = newSetting;
        setFrameSize(calculateTextSize());
        emit textSettingChanged(textSetting_, frameSize_);
    }
}

QSize TextSource::calculateTextSize() const
{
    if (textSetting_.text.isEmpty())
        return QSize(100, 50);

    QFontMetrics fm(textSetting_.font);
    // 处理包含\n的多行文本
    QStringList lines = textSetting_.text.split('\n');
    int maxWidth = 0;
    int totalHeight = 0;
    int lineSpacing = fm.lineSpacing(); // 获取行间距

    // 计算每行的宽度和总高度
    for (const QString& line : lines) {
        int lineWidth = fm.horizontalAdvance(line);
        maxWidth = qMax(maxWidth, lineWidth);
        totalHeight += lineSpacing; // 累加每行高度（包含行间距）
    }

    // 减去最后一行多余的行间距（因为最后一行不需要再加间距）
    if (!lines.isEmpty()) {
        totalHeight -= (lineSpacing - fm.height());
    }

    // 增加更充足的内边距（左右10px，上下12px）
    return QSize(maxWidth + 20, totalHeight + 24);
}
