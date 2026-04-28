/**
 * @file hvscmetadataservice.cpp
 * @brief Implementation of the HVSCMetadataService.
 */

#include "hvscmetadataservice.h"

#include "cacheddownloadmanager.h"
#include "hvscparser.h"

#include <QFile>

HVSCMetadataService::HVSCMetadataService(IFileDownloader *stilDownloader,
                                         IFileDownloader *buglistDownloader, QObject *parent)
    : QObject(parent)
{
    stilManager_ = new CachedDownloadManager(
        stilDownloader, QStringLiteral("STIL.txt"), QUrl(QString::fromLatin1(StilUrl)),
        [this](const QByteArray &data) -> int {
            stilDatabase_ = hvsc::parseStilData(data);
            return stilDatabase_.isEmpty() ? -1 : stilDatabase_.size();
        },
        this);

    buglistManager_ = new CachedDownloadManager(
        buglistDownloader, QStringLiteral("BUGlist.txt"), QUrl(QString::fromLatin1(BuglistUrl)),
        [this](const QByteArray &data) -> int {
            buglistDatabase_ = hvsc::parseBuglistData(data);
            return buglistDatabase_.isEmpty() ? -1 : buglistDatabase_.size();
        },
        this);

    connect(stilManager_, &CachedDownloadManager::downloadProgress, this,
            &HVSCMetadataService::stilDownloadProgress);
    connect(stilManager_, &CachedDownloadManager::downloadFinished, this,
            &HVSCMetadataService::stilDownloadFinished);
    connect(stilManager_, &CachedDownloadManager::downloadFailed, this,
            &HVSCMetadataService::stilDownloadFailed);
    connect(stilManager_, &CachedDownloadManager::loaded, this, &HVSCMetadataService::stilLoaded);

    connect(buglistManager_, &CachedDownloadManager::downloadProgress, this,
            &HVSCMetadataService::buglistDownloadProgress);
    connect(buglistManager_, &CachedDownloadManager::downloadFinished, this,
            &HVSCMetadataService::buglistDownloadFinished);
    connect(buglistManager_, &CachedDownloadManager::downloadFailed, this,
            &HVSCMetadataService::buglistDownloadFailed);
    connect(buglistManager_, &CachedDownloadManager::loaded, this,
            &HVSCMetadataService::buglistLoaded);
}

QString HVSCMetadataService::stilCacheFilePath() const
{
    return stilManager_->cacheFilePath();
}

QString HVSCMetadataService::buglistCacheFilePath() const
{
    return buglistManager_->cacheFilePath();
}

bool HVSCMetadataService::hasCachedStil() const
{
    return stilManager_->hasCachedFile();
}

bool HVSCMetadataService::hasCachedBuglist() const
{
    return buglistManager_->hasCachedFile();
}

bool HVSCMetadataService::loadStilFromCache()
{
    const QString path = stilCacheFilePath();
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
    const QString path = buglistCacheFilePath();
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
    stilManager_->download();
}

void HVSCMetadataService::downloadBuglist()
{
    buglistManager_->download();
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

    const QByteArray data = file.readAll();
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

    const QByteArray data = file.readAll();
    file.close();

    return parseBuglist(data);
}
