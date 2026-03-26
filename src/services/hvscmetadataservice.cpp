/**
 * @file hvscmetadataservice.cpp
 * @brief Implementation of the HVSCMetadataService.
 */

#include "hvscmetadataservice.h"

#include "hvscparser.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>

HVSCMetadataService::HVSCMetadataService(IFileDownloader *stilDownloader,
                                         IFileDownloader *buglistDownloader, QObject *parent)
    : QObject(parent), stilDownloader_(stilDownloader), buglistDownloader_(buglistDownloader)
{
    connect(stilDownloader_, &IFileDownloader::downloadProgress, this,
            &HVSCMetadataService::onStilDownloaderProgress);
    connect(stilDownloader_, &IFileDownloader::downloadFinished, this,
            &HVSCMetadataService::onStilDownloaderFinished);
    connect(stilDownloader_, &IFileDownloader::downloadFailed, this,
            &HVSCMetadataService::onStilDownloaderFailed);

    connect(buglistDownloader_, &IFileDownloader::downloadProgress, this,
            &HVSCMetadataService::onBuglistDownloaderProgress);
    connect(buglistDownloader_, &IFileDownloader::downloadFinished, this,
            &HVSCMetadataService::onBuglistDownloaderFinished);
    connect(buglistDownloader_, &IFileDownloader::downloadFailed, this,
            &HVSCMetadataService::onBuglistDownloaderFailed);

    // Try to load from cache on startup
    if (hasCachedStil()) {
        loadStilFromCache();
    }
    if (hasCachedBuglist()) {
        loadBuglistFromCache();
    }
}

QString HVSCMetadataService::stilCacheFilePath() const
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    return dataDir + "/STIL.txt";
}

QString HVSCMetadataService::buglistCacheFilePath() const
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    return dataDir + "/BUGlist.txt";
}

bool HVSCMetadataService::hasCachedStil() const
{
    return QFile::exists(stilCacheFilePath());
}

bool HVSCMetadataService::hasCachedBuglist() const
{
    return QFile::exists(buglistCacheFilePath());
}

bool HVSCMetadataService::loadStilFromCache()
{
    QString path = stilCacheFilePath();
    if (!QFile::exists(path)) {
        return false;
    }

    if (parseStilFile(path)) {
        emit stilLoaded();
        return true;
    }
    return false;
}

bool HVSCMetadataService::loadBuglistFromCache()
{
    QString path = buglistCacheFilePath();
    if (!QFile::exists(path)) {
        return false;
    }

    if (parseBuglistFile(path)) {
        emit buglistLoaded();
        return true;
    }
    return false;
}

HVSCMetadataService::StilInfo HVSCMetadataService::lookupStil(const QString &hvscPath) const
{
    return hvsc::lookupStil(stilDatabase_, hvscPath);
}

HVSCMetadataService::BugInfo HVSCMetadataService::lookupBuglist(const QString &hvscPath) const
{
    return hvsc::lookupBuglist(buglistDatabase_, hvscPath);
}

void HVSCMetadataService::downloadStil()
{
    if (stilDownloader_->isDownloading()) {
        return;
    }

    stilDownloader_->download(QUrl(QString::fromLatin1(StilUrl)));
}

void HVSCMetadataService::downloadBuglist()
{
    if (buglistDownloader_->isDownloading()) {
        return;
    }

    buglistDownloader_->download(QUrl(QString::fromLatin1(BuglistUrl)));
}

void HVSCMetadataService::onStilDownloaderProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    emit stilDownloadProgress(bytesReceived, bytesTotal);
}

void HVSCMetadataService::onStilDownloaderFinished(const QByteArray &data)
{
    // Save to cache
    QFile cacheFile(stilCacheFilePath());
    if (cacheFile.open(QIODevice::WriteOnly)) {
        cacheFile.write(data);
        cacheFile.close();
    }

    // Parse the database
    if (parseStil(data)) {
        emit stilDownloadFinished(stilDatabase_.size());
        emit stilLoaded();
    } else {
        emit stilDownloadFailed(tr("Failed to parse STIL database"));
    }
}

void HVSCMetadataService::onStilDownloaderFailed(const QString &error)
{
    emit stilDownloadFailed(error);
}

void HVSCMetadataService::onBuglistDownloaderProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    emit buglistDownloadProgress(bytesReceived, bytesTotal);
}

void HVSCMetadataService::onBuglistDownloaderFinished(const QByteArray &data)
{
    // Save to cache
    QFile cacheFile(buglistCacheFilePath());
    if (cacheFile.open(QIODevice::WriteOnly)) {
        cacheFile.write(data);
        cacheFile.close();
    }

    // Parse the database
    if (parseBuglist(data)) {
        emit buglistDownloadFinished(buglistDatabase_.size());
        emit buglistLoaded();
    } else {
        emit buglistDownloadFailed(tr("Failed to parse BUGlist database"));
    }
}

void HVSCMetadataService::onBuglistDownloaderFailed(const QString &error)
{
    emit buglistDownloadFailed(error);
}

bool HVSCMetadataService::parseStil(const QByteArray &data)
{
    stilDatabase_ = hvsc::parseStilData(data);
    return !stilDatabase_.isEmpty();
}

bool HVSCMetadataService::parseStilFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    return parseStil(data);
}

bool HVSCMetadataService::parseBuglist(const QByteArray &data)
{
    buglistDatabase_ = hvsc::parseBuglistData(data);
    return !buglistDatabase_.isEmpty();
}

bool HVSCMetadataService::parseBuglistFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    return parseBuglist(data);
}
