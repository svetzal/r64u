#include "textfilepreview.h"

#include "services/fileactioncore.h"

#include <QFileInfo>

bool TextFilePreview::canHandle(const QString &path) const
{
    fileaction::PreviewContentType type = fileaction::detectPreviewType(path);
    return type == fileaction::PreviewContentType::Text ||
           type == fileaction::PreviewContentType::Html;
}

void TextFilePreview::showPreview(const QString &path, const QByteArray &data)
{
    QFileInfo fi(path);
    fileNameLabel_->setText(fi.fileName());

    isHtml_ = (fileaction::detectPreviewType(path) == fileaction::PreviewContentType::Html);

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
