#include "textsettingdialog.h"
#include "textsettingdialog.h"
#include "ui_textsettingdialog.h"
#include <QColorDialog>
#include <QFontDialog>
#include <QTextEdit>
#include <QDebug>

TextSettingDialog::TextSettingDialog(const QString& title, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::TextSettingDialog)
    , title_(title)
{
    ui->setupUi(this);
    init();
}

TextSettingDialog::~TextSettingDialog()
{
    delete ui;
}

void TextSettingDialog::init()
{
    this->setWindowTitle(title_);

    ui->colorTransparencySlider->setRange(0, 100);
    ui->bgColorTransparencySlider->setRange(0, 100);
    ui->colorTransparencySlider->setValue(100);
    ui->bgColorTransparencySlider->setValue(0);

    ui->fontNameLabel->setFont(setting_.font);
    ui->fontNameLabel->setText(QString("%1 %2pt")
                                   .arg(setting_.font.family())
                                   .arg(setting_.font.pointSize()));

    ui->previewLabel->setWordWrap(true);
    ui->previewLabel->setMinimumHeight(80);
    ui->previewLabel->setAlignment(Qt::AlignCenter);
    ui->previewLabel->setStyleSheet("border: 1px solid #eee;");

    updateTextPreview();
}

void TextSettingDialog::setInitialSetting(const TextSetting &setting)
{
    setting_ = setting;

    ui->textEdit->setText(setting.text);
    // ui->textEdit->setPlaceholderText("请输入文本...");

    // 字体初始化
    ui->fontNameLabel->setFont(setting.font);
    ui->fontNameLabel->setText(QString("%1 %2pt %3%4")
                                   .arg(setting.font.family())
                                   .arg(setting.font.pointSize())
                                   .arg(setting.font.bold() ? "粗体 " : "")
                                   .arg(setting.font.italic() ? "斜体" : ""));

    // 文本颜色初始化
    QColor textColor(setting.color);
    if (!textColor.isValid()) {
        textColor = Qt::black; // 默认黑色（避免无背景色）
        setting_.color = "#ffffff"; // 更新无效颜色为默认值
    }
    ui->colorLabel->setStyleSheet(QString("background-color: %1; border: 1px solid #ccc;")
                                      .arg(textColor.name()));
    ui->colorLabel->setText(textColor.name());

    // 背景颜色初始化
    QColor bgColor(setting.bgColor);
    if (!bgColor.isValid()) {
        bgColor = Qt::white; // 默认白色
        setting_.bgColor = "#000000";
    }
    ui->bgColorLabel->setStyleSheet(QString("background-color: %1; border: 1px solid #ccc;")
                                        .arg(bgColor.name()));
    ui->bgColorLabel->setText(bgColor.name());

    // 透明度滑块（转换：qreal 0.0-1.0 → int 0-100）
    int textOpacityInt = qRound(setting.textOpacity * 100);
    int bgOpacityInt = qRound(setting.bgOpacity * 100);
    ui->colorTransparencySlider->setValue(textOpacityInt);
    ui->bgColorTransparencySlider->setValue(bgOpacityInt);

    // 文本转换下拉框
    ui->transformCBBOX->setCurrentIndex(getTextTransformIndex(setting.textTransform));

    // 对齐方式下拉框
    ui->horizonAlignmentCBBox->setCurrentIndex(getHorizontalAlignmentIndex(setting.horizontalAlign));
    ui->verticalAlignmentCBBox->setCurrentIndex(getVerticalAlignmentIndex(setting.verticalAlign));

}

TextSetting TextSettingDialog::getTextSetting()
{
    // 1. 文本内容（去除首尾空格，避免空白导致的显示问题）
    setting_.text = ui->textEdit->toPlainText();

    // 2. 字体（如果注释了字体赋值，需要取消注释，否则字体为默认值）
    // setting_.font = ui->fontNameLabel->font(); // 关键：确保字体从UI正确获取

    // 3. 文本颜色（记录原始字符串和解析后的QColor）
    setting_.color = ui->colorLabel->text();
    QColor textColor(setting_.color);

    // 4. 背景颜色
    setting_.bgColor = ui->bgColorLabel->text();
    QColor bgColor(setting_.bgColor);

    // 5. 文本转换（大写/小写等）
    setting_.textTransform = getTextTransform(ui->transformCBBOX->currentIndex());

    // 6. 文本透明度（0.0~1.0）
    setting_.textOpacity = ui->colorTransparencySlider->value() / 100.0;

    // 7. 背景透明度（0.0~1.0）
    setting_.bgOpacity = ui->bgColorTransparencySlider->value() / 100.0;

    // 8. 对齐方式
    setting_.horizontalAlign = getHorizontalAlignment(ui->horizonAlignmentCBBox->currentIndex());
    setting_.verticalAlign = getVerticalAlignment(ui->verticalAlignmentCBBox->currentIndex());

    qDebug() << "\n===== TextSetting 调试信息 =====";
    qDebug() << "1. 文本内容: " << (setting_.text.isEmpty() ? "空文本" : setting_.text);
    qDebug() << "2. 字体: 家族=" << setting_.font.family()
             << ", 大小=" << setting_.font.pointSize() << "pt"
             << ", 粗体=" << setting_.font.bold()
             << ", 斜体=" << setting_.font.italic();
    qDebug() << "3. 文本颜色: 原始字符串=" << setting_.color
             << ", 有效=" << textColor.isValid()
             << ", RGBA=(" << textColor.red() << "," << textColor.green() << "," << textColor.blue() << "," << textColor.alpha() << ")";
    qDebug() << "4. 背景颜色: 原始字符串=" << setting_.bgColor
             << ", 有效=" << bgColor.isValid()
             << ", RGBA=(" << bgColor.red() << "," << bgColor.green() << "," << bgColor.blue() << "," << bgColor.alpha() << ")";
    qDebug() << "5. 文本透明度: " << setting_.textOpacity
             << "(滑块值=" << ui->colorTransparencySlider->value() << ")";
    qDebug() << "6. 背景透明度: " << setting_.bgOpacity
             << "(滑块值=" << ui->bgColorTransparencySlider->value() << ")";
    qDebug() << "7. 文本转换: " << static_cast<int>(setting_.textTransform);
    qDebug() << "8. 水平对齐: " << static_cast<int>(setting_.horizontalAlign);
    qDebug() << "9. 垂直对齐: " << static_cast<int>(setting_.verticalAlign);
    qDebug() << "=============================\n";
    return setting_;
}

// 选择字体：打开字体对话框，同步到UI和setting_
void TextSettingDialog::on_chooseFontBtn_clicked()
{
    bool isConfirmed;
    QFont selectedFont = QFontDialog::getFont(
        &isConfirmed,
        setting_.font,
        this,
        "选择文本字体"
        );

    if (isConfirmed) {
        setting_.font = selectedFont;

        qDebug() << "[选择的字体] 家族:" << selectedFont.family()
                 << "点大小:" << selectedFont.pointSize()
                 << "像素大小:" << selectedFont.pixelSize();
        ui->fontNameLabel->setFont(selectedFont);
        ui->fontNameLabel->setText(QString("%1 %2pt %3%4")
                                       .arg(selectedFont.family())
                                       .arg(selectedFont.pointSize())
                                       .arg(selectedFont.bold() ? "粗体 " : "")
                                       .arg(selectedFont.italic() ? "斜体" : ""));
        updateTextPreview();
    }
}
void TextSettingDialog::on_chooseColorBtn_clicked()
{
    QColor selectedColor = QColorDialog::getColor(
        QColor(setting_.color),
        this,
        "选择文本颜色"
        );

    if (selectedColor.isValid()) {
        setting_.color = selectedColor.name();
        ui->colorLabel->setStyleSheet(QString("background-color: %1;")
                                          .arg(selectedColor.name()));
        ui->colorLabel->setText(selectedColor.name());
        updateTextPreview();
    }
}

void TextSettingDialog::on_chooseBgColorBtn_clicked()
{
    QColor selectedColor = QColorDialog::getColor(
        QColor(setting_.bgColor),
        this,
        "选择背景颜色"
        );

    if (selectedColor.isValid()) {
        setting_.bgColor = selectedColor.name();
        ui->bgColorLabel->setStyleSheet(QString("background-color: %1;")
                                            .arg(selectedColor.name()));
        ui->bgColorLabel->setText(selectedColor.name());
        updateTextPreview();
    }
}

TextTransform TextSettingDialog::getTextTransform(int index)
{
    switch (index) {
    case 0: return TextTransform::None;
    case 1: return TextTransform::UpperCase;
    case 2: return TextTransform::LowerCase;
    case 3: return TextTransform::Capitalize;
    default: return TextTransform::None;
    }
}

HorizontalAlignment TextSettingDialog::getHorizontalAlignment(int index)
{
    switch (index) {
    case 0: return HorizontalAlignment::Left;
    case 1: return HorizontalAlignment::Right;
    case 2: return HorizontalAlignment::Center;
    case 3: return HorizontalAlignment::Justify;
    default: return HorizontalAlignment::Left;
    }
}

VerticalAlignment TextSettingDialog::getVerticalAlignment(int index)
{
    switch (index) {
    case 0: return VerticalAlignment::Top;
    case 1: return VerticalAlignment::Middle;
    case 2: return VerticalAlignment::Bottom;
    default: return VerticalAlignment::Top;
    }
}

int TextSettingDialog::getTextTransformIndex(TextTransform transform)
{
    switch (transform) {
    case TextTransform::None: return 0;
    case TextTransform::UpperCase: return 1;
    case TextTransform::LowerCase: return 2;
    case TextTransform::Capitalize: return 3;
    default: return 0;
    }
}

int TextSettingDialog::getHorizontalAlignmentIndex(HorizontalAlignment align)
{
    switch (align) {
    case HorizontalAlignment::Left: return 0;
    case HorizontalAlignment::Right: return 1;
    case HorizontalAlignment::Center: return 2;
    case HorizontalAlignment::Justify: return 3;
    default: return 0;
    }
}

int TextSettingDialog::getVerticalAlignmentIndex(VerticalAlignment align)
{
    switch (align) {
    case VerticalAlignment::Top: return 0;
    case VerticalAlignment::Middle: return 1;
    case VerticalAlignment::Bottom: return 2;
    default: return 1; // 默认垂直居中（符合结构体默认值）
    }
}

void TextSettingDialog::updateTextPreview()
{
    QString previewText = setting_.text;

    switch (setting_.textTransform) {
    case TextTransform::UpperCase:
        previewText = previewText.toUpper();
        break;
    case TextTransform::LowerCase:
        previewText = previewText.toLower();
        break;
    case TextTransform::Capitalize:
        // 首字母大写（处理每个单词的首字母）
        previewText = previewText.toLower(); // 先全部小写，避免原文本大小写混乱
        for (int i = 0; i < previewText.size(); ++i) {
            if (i == 0 || previewText.at(i-1).isSpace()) { // 首字符或空格后首字符
                previewText[i] = previewText.at(i).toUpper();
            }
        }
        break;
    case TextTransform::None:
    default:
        break;
    }

    if (previewText.isEmpty()) {
        previewText = "预览文本（请输入内容）";
    }

    ui->previewLabel->setText(previewText);
    ui->previewLabel->setFont(setting_.font);

    QColor textColor(setting_.color);
    textColor.setAlphaF(setting_.textOpacity);
    ui->previewLabel->setStyleSheet(ui->previewLabel->styleSheet() +
                                QString("color: %1;").arg(textColor.name(QColor::HexArgb))); // HexArgb 支持透明度

    QColor bgColor(setting_.bgColor);
    bgColor.setAlphaF(setting_.bgOpacity);
    ui->previewLabel->setAutoFillBackground(true);
    QPalette palette = ui->previewLabel->palette();
    palette.setColor(QPalette::Window, bgColor);
    ui->previewLabel->setPalette(palette);

    Qt::Alignment alignment = Qt::AlignLeft; // 默认左对齐
    // 转换水平对齐
    switch (setting_.horizontalAlign) {
    case HorizontalAlignment::Left: alignment |= Qt::AlignLeft; break;
    case HorizontalAlignment::Right: alignment |= Qt::AlignRight; break;
    case HorizontalAlignment::Center: alignment |= Qt::AlignHCenter; break;
    case HorizontalAlignment::Justify: alignment |= Qt::AlignJustify; break;
    }
    // 转换垂直对齐
    switch (setting_.verticalAlign) {
    case VerticalAlignment::Top: alignment |= Qt::AlignTop; break;
    case VerticalAlignment::Middle: alignment |= Qt::AlignVCenter; break;
    case VerticalAlignment::Bottom: alignment |= Qt::AlignBottom; break;
    }
    ui->previewLabel->setAlignment(alignment);
}

void TextSettingDialog::on_textEdit_textChanged()
{
    setting_.text = ui->textEdit->toPlainText();
    updateTextPreview();
}


void TextSettingDialog::on_transformCBBOX_currentIndexChanged(int index)
{
    setting_.textTransform = getTextTransform(index);
    updateTextPreview();
}


void TextSettingDialog::on_colorTransparencySlider_valueChanged(int value)
{
    ui->colorPercentLabel->setText(QString("%1%").arg(value));
    setting_.textOpacity = ui->colorTransparencySlider->value() / 100.0;
    updateTextPreview();
}


void TextSettingDialog::on_bgColorTransparencySlider_valueChanged(int value)
{
    ui->bgColorPercentLabel->setText(QString("%1%").arg(value));
    setting_.bgOpacity = ui->bgColorTransparencySlider->value() / 100.0;
    updateTextPreview();
}


void TextSettingDialog::on_horizonAlignmentCBBox_currentIndexChanged(int index)
{
    setting_.horizontalAlign = getHorizontalAlignment(index);
    updateTextPreview();
}


void TextSettingDialog::on_verticalAlignmentCBBox_currentIndexChanged(int index)
{
    setting_.verticalAlign = getVerticalAlignment(index);
    updateTextPreview();
}

