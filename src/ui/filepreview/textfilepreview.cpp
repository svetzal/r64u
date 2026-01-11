#include "textfilepreview.h"

#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QStyleHints>
#include <QTextBlockFormat>
#include <QTextCursor>

bool TextFilePreview::canHandle(const QString &path) const
{
    return isTextFile(path) || isHtmlFile(path);
}

QWidget* TextFilePreview::createPreviewWidget(QWidget *parent)
{
    previewWidget_ = new QWidget(parent);
    auto *layout = new QVBoxLayout(previewWidget_);
    layout->setContentsMargins(0, 0, 0, 0);

    // File name label
    fileNameLabel_ = new QLabel(previewWidget_);
    QFont boldFont = fileNameLabel_->font();
    boldFont.setBold(true);
    boldFont.setPointSize(boldFont.pointSize() + 2);
    fileNameLabel_->setFont(boldFont);
    fileNameLabel_->setContentsMargins(0, 0, 0, 4);

    // Text browser
    textBrowser_ = new QTextBrowser(previewWidget_);
    textBrowser_->setReadOnly(true);

    layout->addWidget(fileNameLabel_);
    layout->addWidget(textBrowser_);

    return previewWidget_;
}

void TextFilePreview::showPreview(const QString &path, const QByteArray &data)
{
    QFileInfo fi(path);
    fileNameLabel_->setText(fi.fileName());

    isHtml_ = isHtmlFile(path);

    if (isHtml_) {
        textBrowser_->setOpenExternalLinks(true);
        textBrowser_->setHtml(QString::fromUtf8(data));
    } else {
        // Plain text file
        applyC64TextStyle();
        textBrowser_->setOpenExternalLinks(false);
        textBrowser_->setOpenLinks(false);
        textBrowser_->setPlainText(QString::fromUtf8(data));

        // Apply line height for better readability
        applyLineHeight(150);
    }
}

void TextFilePreview::showLoading(const QString &path)
{
    QFileInfo fi(path);
    fileNameLabel_->setText(fi.fileName());
    textBrowser_->setPlainText(tr("Loading..."));
}

void TextFilePreview::showError(const QString &error)
{
    if (isHtml_) {
        textBrowser_->setHtml(QString("<p style='color:red'>Error: %1</p>").arg(error));
    } else {
        textBrowser_->setPlainText(tr("Error: %1").arg(error));
    }
}

void TextFilePreview::clear()
{
    if (fileNameLabel_) {
        fileNameLabel_->clear();
    }
    if (textBrowser_) {
        textBrowser_->clear();
    }
}

bool TextFilePreview::isTextFile(const QString &path) const
{
    QString lower = path.toLower();
    return lower.endsWith(".cfg") ||
           lower.endsWith(".txt") ||
           lower.endsWith(".log") ||
           lower.endsWith(".ini") ||
           lower.endsWith(".md") ||
           lower.endsWith(".json") ||
           lower.endsWith(".xml");
}

bool TextFilePreview::isHtmlFile(const QString &path) const
{
    QString lower = path.toLower();
    return lower.endsWith(".html") || lower.endsWith(".htm");
}

void TextFilePreview::applyC64TextStyle()
{
    if (!textBrowser_) {
        return;
    }

    // Set C64 Pro Mono font
    QFont c64Font("C64 Pro Mono");
    c64Font.setStyleHint(QFont::Monospace);
    c64Font.setPointSize(12);
    textBrowser_->setFont(c64Font);

    // Determine color scheme based on system theme
    Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
    bool isDarkMode = (scheme == Qt::ColorScheme::Dark);

    // C64 color constants
    const QString c64Blue = "#4040E8";
    const QString c64LightBlue = "#887ECB";

    QString stylesheet;
    if (isDarkMode) {
        // Dark mode: blue text on black background
        stylesheet = QString(
            "QTextBrowser {"
            "  background-color: #000000;"
            "  color: %1;"
            "  border: 1px solid %1;"
            "  padding: 8px;"
            "}"
        ).arg(c64LightBlue);
    } else {
        // Light mode: white text on blue background (classic C64 look)
        stylesheet = QString(
            "QTextBrowser {"
            "  background-color: %1;"
            "  color: #FFFFFF;"
            "  border: 1px solid #2020A8;"
            "  padding: 8px;"
            "}"
        ).arg(c64Blue);
    }

    textBrowser_->setStyleSheet(stylesheet);
}

void TextFilePreview::applyLineHeight(int percentage)
{
    if (!textBrowser_) {
        return;
    }

    QTextBlockFormat blockFormat;
    blockFormat.setLineHeight(percentage, QTextBlockFormat::ProportionalHeight);
    QTextCursor cursor = textBrowser_->textCursor();
    cursor.select(QTextCursor::Document);
    cursor.mergeBlockFormat(blockFormat);
}
