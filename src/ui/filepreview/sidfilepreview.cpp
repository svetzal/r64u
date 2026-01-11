#include "sidfilepreview.h"

#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QStyleHints>
#include <QTextBlockFormat>
#include <QTextCursor>

bool SidFilePreview::canHandle(const QString &path) const
{
    return SidFileParser::isSidFile(path);
}

QWidget* SidFilePreview::createPreviewWidget(QWidget *parent)
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

    // Text browser for SID details
    textBrowser_ = new QTextBrowser(previewWidget_);
    textBrowser_->setReadOnly(true);
    textBrowser_->setOpenExternalLinks(false);
    textBrowser_->setOpenLinks(false);

    // Apply C64 styling
    applyC64TextStyle();

    layout->addWidget(fileNameLabel_);
    layout->addWidget(textBrowser_);

    return previewWidget_;
}

void SidFilePreview::showPreview(const QString &path, const QByteArray &data)
{
    SidFileParser::SidInfo info = SidFileParser::parse(data);

    if (!info.valid) {
        showError(tr("Unable to parse SID file"));
        return;
    }

    QString details = SidFileParser::formatForDisplay(info);

    QFileInfo fi(path);
    fileNameLabel_->setText(fi.fileName());
    textBrowser_->setPlainText(details);

    // Apply line height for better readability
    applyLineHeight(140);
}

void SidFilePreview::showLoading(const QString &path)
{
    QFileInfo fi(path);
    fileNameLabel_->setText(fi.fileName());
    textBrowser_->setPlainText(tr("Loading SID info..."));
}

void SidFilePreview::showError(const QString &error)
{
    if (textBrowser_) {
        textBrowser_->setPlainText(tr("Error: %1").arg(error));
    }
}

void SidFilePreview::clear()
{
    if (fileNameLabel_) {
        fileNameLabel_->clear();
    }
    if (textBrowser_) {
        textBrowser_->clear();
    }
}

void SidFilePreview::applyC64TextStyle()
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

void SidFilePreview::applyLineHeight(int percentage)
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
