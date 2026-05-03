/**
 * @file cacheddownloadservice.cpp
 * @brief Implementation of CachedDownloadService.
 */

#include "cacheddownloadservice.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>

CachedDownloadService::CachedDownloadService(IFileDownloader *downloader,
                                             const QString &cacheFilename, const QUrl &downloadUrl,
                                             ParseCallback parseCallback, QObject *parent)
    : QObject(parent), downloader_(downloader), cacheFilename_(cacheFilename),
      downloadUrl_(downloadUrl), parseCallback_(std::move(parseCallback))
{
    connect(downloader_, &IFileDownloader::downloadProgress, this,
            &CachedDownloadService::onDownloaderProgress);
    connect(downloader_, &IFileDownloader::downloadFinished, this,
            &CachedDownloadService::onDownloaderFinished);
    connect(downloader_, &IFileDownloader::downloadFailed, this,
            &CachedDownloadService::onDownloaderFailed);

    if (hasCachedFile()) {
        loadFromFile(cacheFilePath());
    }
}

QString CachedDownloadService::cacheFilePath() const
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    return dataDir + "/" + cacheFilename_;
}

bool CachedDownloadService::hasCachedFile() const
{
    return QFile::exists(cacheFilePath());
}

void CachedDownloadService::download()
{
    if (downloader_->isDownloading()) {
        return;
    }
    downloader_->download(downloadUrl_);
}

void CachedDownloadService::cancelDownload()
{
    downloader_->cancel();
}

bool CachedDownloadService::loadFromCache()
{
    if (!hasCachedFile()) {
        return false;
    }
    return loadFromFile(cacheFilePath());
}

void CachedDownloadService::onDownloaderProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    emit downloadProgress(bytesReceived, bytesTotal);
}

void CachedDownloadService::onDownloaderFinished(const QByteArray &data)
{
    // Persist to cache before parsing so the data is available on next startup
    // even if parsing fails.
    QFile cacheFile(cacheFilePath());
    if (cacheFile.open(QIODevice::WriteOnly)) {
        cacheFile.write(data);
        cacheFile.close();
    }

    const int count = parseCallback_(data);
    if (count >= 0) {
        emit downloadFinished(count);
        emit loaded();
    } else {
        emit downloadFailed(tr("Failed to parse downloaded data"));
    }
}

void CachedDownloadService::onDownloaderFailed(const QString &error)
{
    emit downloadFailed(error);
}

bool CachedDownloadService::loadFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    const int count = parseCallback_(data);
    if (count >= 0) {
        emit loaded();
        return true;
    }
    return false;
}
