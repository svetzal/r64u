#include "diskimagepreview.h"

#include "services/diskimagereader.h"

#include <QFileInfo>

bool DiskImagePreview::canHandle(const QString &path) const
{
    return DiskImageReader::isDiskImage(path);
}

void DiskImagePreview::showPreview(const QString &path, const QByteArray &data)
{
    DiskImageReader::DiskDirectory dir = DiskImageReader::parse(data, path);

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
