#include "defaultfilepreview.h"

#include <QFileInfo>
#include <QFont>

bool DefaultFilePreview::canHandle(const QString &/*path*/) const
{
    // Default strategy handles everything (fallback)
    return true;
}

QWidget* DefaultFilePreview::createPreviewWidget(QWidget *parent)
{
    previewWidget_ = new QWidget(parent);
    auto *layout = new QVBoxLayout(previewWidget_);
    layout->setAlignment(Qt::AlignTop);

    // File name label
    fileNameLabel_ = new QLabel(previewWidget_);
    fileNameLabel_->setWordWrap(true);
    QFont boldFont = fileNameLabel_->font();
    boldFont.setBold(true);
    boldFont.setPointSize(boldFont.pointSize() + 2);
    fileNameLabel_->setFont(boldFont);

    // File size label
    fileSizeLabel_ = new QLabel(previewWidget_);

    // File type label
    fileTypeLabel_ = new QLabel(previewWidget_);

    // Status label (for loading/errors)
    statusLabel_ = new QLabel(previewWidget_);
    statusLabel_->setAlignment(Qt::AlignCenter);
    statusLabel_->setStyleSheet("color: gray;");
    statusLabel_->hide();

    layout->addWidget(fileNameLabel_);
    layout->addSpacing(8);
    layout->addWidget(fileSizeLabel_);
    layout->addWidget(fileTypeLabel_);
    layout->addStretch();
    layout->addWidget(statusLabel_);

    return previewWidget_;
}

void DefaultFilePreview::showPreview(const QString &/*path*/, const QByteArray &/*data*/)
{
    // Default preview only shows metadata, not content
    // Metadata should be set via setFileDetails()
    if (statusLabel_) {
        statusLabel_->hide();
    }
}

void DefaultFilePreview::showLoading(const QString &path)
{
    QFileInfo fi(path);
    fileNameLabel_->setText(fi.fileName());
    statusLabel_->setText(tr("Loading..."));
    statusLabel_->show();
}

void DefaultFilePreview::showError(const QString &error)
{
    if (statusLabel_) {
        statusLabel_->setText(tr("Error: %1").arg(error));
        statusLabel_->show();
    }
}

void DefaultFilePreview::clear()
{
    if (fileNameLabel_) {
        fileNameLabel_->clear();
    }
    if (fileSizeLabel_) {
        fileSizeLabel_->clear();
    }
    if (fileTypeLabel_) {
        fileTypeLabel_->clear();
    }
    if (statusLabel_) {
        statusLabel_->hide();
    }
}

void DefaultFilePreview::setFileDetails(const QString &path, qint64 size, const QString &type)
{
    QFileInfo fi(path);
    fileNameLabel_->setText(fi.fileName());

    QString sizeStr;
    if (size < 1024) {
        sizeStr = tr("Size: %1 bytes").arg(size);
    } else if (size < 1024 * 1024) {
        sizeStr = tr("Size: %1 KB").arg(size / 1024.0, 0, 'f', 1);
    } else {
        sizeStr = tr("Size: %1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 2);
    }
    fileSizeLabel_->setText(sizeStr);
    fileTypeLabel_->setText(tr("Type: %1").arg(type));

    if (statusLabel_) {
        statusLabel_->hide();
    }
}
