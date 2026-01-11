#include "diskimagepreview.h"

#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QStyleHints>

bool DiskImagePreview::canHandle(const QString &path) const
{
    return DiskImageReader::isDiskImage(path);
}

QWidget* DiskImagePreview::createPreviewWidget(QWidget *parent)
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

    // Text browser for directory listing
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

void DiskImagePreview::showPreview(const QString &path, const QByteArray &data)
{
    DiskImageReader reader;
    DiskImageReader::DiskDirectory dir = reader.parse(data, path);

    if (dir.format == DiskImageReader::Format::Unknown) {
        showError(tr("Unable to parse disk image"));
        return;
    }

    QString listing = DiskImageReader::formatDirectoryListing(dir);

    QFileInfo fi(path);
    fileNameLabel_->setText(fi.fileName());
    textBrowser_->setPlainText(listing);

    // Note: No extra line height for disk directories - PETSCII graphics
    // require characters to touch vertically with no gaps
}

void DiskImagePreview::showLoading(const QString &path)
{
    QFileInfo fi(path);
    fileNameLabel_->setText(fi.fileName());
    textBrowser_->setPlainText(tr("Loading disk directory..."));
}

void DiskImagePreview::showError(const QString &error)
{
    if (textBrowser_) {
        textBrowser_->setPlainText(tr("Error: %1").arg(error));
    }
}

void DiskImagePreview::clear()
{
    if (fileNameLabel_) {
        fileNameLabel_->clear();
    }
    if (textBrowser_) {
        textBrowser_->clear();
    }
}

void DiskImagePreview::applyC64TextStyle()
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
