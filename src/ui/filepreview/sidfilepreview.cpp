#include "sidfilepreview.h"

#include "services/sidfileparser.h"

#include <QFileInfo>

bool SidFilePreview::canHandle(const QString &path) const
{
    return SidFileParser::isSidFile(path);
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
