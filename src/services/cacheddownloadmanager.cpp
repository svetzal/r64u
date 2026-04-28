/**
 * @file cacheddownloadmanager.cpp
 * @brief Implementation of CachedDownloadManager.
 */

#include "cacheddownloadmanager.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>

CachedDownloadManager::CachedDownloadManager(IFileDownloader *downloader,
                                             const QString &cacheFilename, const QUrl &downloadUrl,
                                             ParseCallback parseCallback, QObject *parent)
    : QObject(parent), downloader_(downloader), cacheFilename_(cacheFilename),
      downloadUrl_(downloadUrl), parseCallback_(std::move(parseCallback))
{
    connect(downloader_, &IFileDownloader::downloadProgress, this,
            &CachedDownloadManager::onDownloaderProgress);
    connect(downloader_, &IFileDownloader::downloadFinished, this,
            &CachedDownloadManager::onDownloaderFinished);
    connect(downloader_, &IFileDownloader::downloadFailed, this,
            &CachedDownloadManager::onDownloaderFailed);

    if (hasCachedFile()) {
        loadFromFile(cacheFilePath());
    }
}

QString CachedDownloadManager::cacheFilePath() const
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    return dataDir + "/" + cacheFilename_;
}

bool CachedDownloadManager::hasCachedFile() const
{
    return QFile::exists(cacheFilePath());
}

void CachedDownloadManager::download()
{
    if (downloader_->isDownloading()) {
        return;
    }
    downloader_->download(downloadUrl_);
}

void CachedDownloadManager::cancelDownload()
{
    downloader_->cancel();
}

void CachedDownloadManager::onDownloaderProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    emit downloadProgress(bytesReceived, bytesTotal);
}

void CachedDownloadManager::onDownloaderFinished(const QByteArray &data)
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

void CachedDownloadManager::onDownloaderFailed(const QString &error)
{
    emit downloadFailed(error);
}

bool CachedDownloadManager::loadFromFile(const QString &filePath)
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
