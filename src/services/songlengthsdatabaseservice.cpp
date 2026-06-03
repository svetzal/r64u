/**
 * @file songlengthsdatabaseservice.cpp
 * @brief Implementation of the SonglengthsDatabaseService service.
 */

#include "songlengthsdatabaseservice.h"

#include "cacheddownloadservice.h"
#include "core/songlengthsparser.h"

#include <QCryptographicHash>
#include <QFile>

SonglengthsDatabaseService::SonglengthsDatabaseService(IFileDownloaderService *downloader,
                                                       QObject *parent)
    : QObject(parent)
{
    manager_ = new CachedDownloadService(
        downloader, QStringLiteral("Songlengths.md5"), QUrl(QString::fromLatin1(DatabaseUrl)),
        [this](const QByteArray &data) -> int {
            parsedDb_ = songlengths::parseDatabase(data);
            return parsedDb_.durations.isEmpty() ? -1 : parsedDb_.durations.size();
        },
        this);

    connect(manager_, &CachedDownloadService::downloadProgress, this,
            &SonglengthsDatabaseService::downloadProgress);
    connect(manager_, &CachedDownloadService::downloadFinished, this,
            &SonglengthsDatabaseService::downloadFinished);
    connect(manager_, &CachedDownloadService::downloadFailed, this,
            &SonglengthsDatabaseService::downloadFailed);
    connect(manager_, &CachedDownloadService::loaded, this,
            &SonglengthsDatabaseService::databaseLoaded);
}

QString SonglengthsDatabaseService::cacheFilePath() const
{
    return manager_->cacheFilePath();
}

bool SonglengthsDatabaseService::hasCachedDatabase() const
{
    return manager_->hasCachedFile();
}

bool SonglengthsDatabaseService::loadFromCache()
{
    return manager_->loadFromCache();
}

QString SonglengthsDatabaseService::calculateMd5(const QByteArray &sidData)
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(sidData);
    return QString::fromLatin1(hash.result().toHex()).toLower();
}

SonglengthsDatabaseService::SongLengths
SonglengthsDatabaseService::lookup(const QString &md5Hash) const
{
    SongLengths result;
    QString hash = md5Hash.toLower();

    if (parsedDb_.durations.contains(hash)) {
        result.found = true;
        result.hvscPath = parsedDb_.md5ToPath.value(hash);
        result.durations = parsedDb_.durations.value(hash);
        result.formattedTimes = parsedDb_.formattedTimes.value(hash);
    }

    return result;
}

SonglengthsDatabaseService::SongLengths
SonglengthsDatabaseService::lookupByData(const QByteArray &sidData) const
{
    const QString md5 = calculateMd5(sidData);
    return lookup(md5);
}

int SonglengthsDatabaseService::getDuration(const QString &md5Hash, int subsong) const
{
    SongLengths lengths = lookup(md5Hash);
    if (!lengths.found || subsong < 1 || subsong > lengths.durations.size()) {
        return DefaultDurationSecs;
    }
    return lengths.durations.at(subsong - 1);  // Convert 1-indexed to 0-indexed
}

int SonglengthsDatabaseService::getDurationByData(const QByteArray &sidData, int subsong) const
{
    const QString md5 = calculateMd5(sidData);
    return getDuration(md5, subsong);
}

void SonglengthsDatabaseService::downloadDatabase()
{
    manager_->download();
}

bool SonglengthsDatabaseService::parseDatabaseData(const QByteArray &data)
{
    parsedDb_ = songlengths::parseDatabase(data);
    return !parsedDb_.durations.isEmpty();
}

bool SonglengthsDatabaseService::parseDatabaseFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    return parseDatabaseData(data);
}
