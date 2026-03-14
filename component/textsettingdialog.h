#ifndef TEXTSETTINGDIALOG_H
#define TEXTSETTINGDIALOG_H

#include <QDialog>
#include <QFont>
#include <QLabel>
namespace Ui {
class TextSettingDialog;
}

// 文本转换枚举
enum class TextTransform {
    None,           // 无转换（默认）
    UpperCase,      // 全部大写（如 "hello" → "HELLO"）
    LowerCase,      // 全部小写（如 "HELLO" → "hello"）
    Capitalize      // 首字母大写（如 "hello world" → "Hello World"）
};

// 水平对齐枚举
enum class HorizontalAlignment {
    Left,           // 左对齐（默认）
    Right,          // 右对齐
    Center,         // 居中对齐
    Justify         // 两端对齐（仅多行文本有效）
};

// 垂直对齐枚举
enum class VerticalAlignment {
    Top,            // 顶部对齐
    Middle,         // 垂直居中（默认）
    Bottom          // 底部对齐
};

// 文本设置结构体
struct TextSetting {
    QString nickName;
    QString text = "预览文本（请输入内容）";                // 文本内容（默认空）
    QFont font = QFont("Microsoft YaHei", 72);          // 字体（默认：微软雅黑，12号字）
    TextTransform textTransform = TextTransform::None;  // 文本转换（默认无转换）
    QString color = "#ffffff";                          // 文本颜色（默认黑色，原#ffffff是白色，修正逻辑）
    QString bgColor = "#000000";                        // 背景颜色（默认白色）
    qreal textOpacity = 1.0;                            // 文本透明度（0.0=完全透明，1.0=不透明）
    qreal bgOpacity = 0.0;                              // 背景透明度
    HorizontalAlignment horizontalAlign = HorizontalAlignment::Left;  // 水平对齐（默认左对齐）
    VerticalAlignment verticalAlign = VerticalAlignment::Middle;      // 垂直对齐（默认居中）

    bool operator==(const TextSetting& other) const {
        // 1. 比较 QString 类型（直接用 ==）
        if (this->nickName != other.nickName) return false;
        if (this->text != other.text) return false;
        if (this->color != other.color) return false;
        if (this->bgColor != other.bgColor) return false;

        // 2. 比较 QFont 类型（Qt 已重载 ==，会比较字体名、大小、样式等）
        if (this->font != other.font) return false;

        // 3. 比较枚举类型（直接用 ==，枚举底层是整数）
        if (this->textTransform != other.textTransform) return false;
        if (this->horizontalAlign != other.horizontalAlign) return false;
        if (this->verticalAlign != other.verticalAlign) return false;

        // 4. 比较浮点数（qreal）：用 qFuzzyCompare 处理精度问题
        if (!qFuzzyCompare(this->textOpacity, other.textOpacity)) return false;
        if (!qFuzzyCompare(this->bgOpacity, other.bgOpacity)) return false;

        // 所有成员都相等，返回 true
        return true;
    }

    bool operator!=(const TextSetting& other) const {
        return !(*this == other);
    }
};

class TextSettingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TextSettingDialog(const QString& title, QWidget *parent = nullptr);
    ~TextSettingDialog();

    void init();                                  // 初始化UI（如滑块范围、标题）
    void setInitialSetting(const TextSetting& setting);  // 设置初始值
    TextSetting getTextSetting();                 // 获取最终设置

private slots:
    void on_chooseFontBtn_clicked();              // 选择字体按钮
    void on_chooseColorBtn_clicked();             // 选择文本颜色按钮
    void on_chooseBgColorBtn_clicked();           // 选择背景颜色按钮
    void on_textEdit_textChanged();
    void on_transformCBBOX_currentIndexChanged(int index);
    void on_colorTransparencySlider_valueChanged(int value);
    void on_bgColorTransparencySlider_valueChanged(int value);
    void on_horizonAlignmentCBBox_currentIndexChanged(int index);
    void on_verticalAlignmentCBBox_currentIndexChanged(int index);

private:
    TextTransform getTextTransform(int index);
    HorizontalAlignment getHorizontalAlignment(int index);
    VerticalAlignment getVerticalAlignment(int index);

    int getTextTransformIndex(TextTransform transform);
    int getHorizontalAlignmentIndex(HorizontalAlignment align);
    int getVerticalAlignmentIndex(VerticalAlignment align);

    void updateTextPreview();

private:
    Ui::TextSettingDialog *ui;
    QString title_;         // 对话框标题
    TextSetting setting_;   // 保存当前设置
};

#endif // TEXTSETTINGDIALOG_H
