/**
 * @file songlengthsdatabase.cpp
 * @brief Implementation of the SonglengthsDatabase service.
 */

#include "songlengthsdatabase.h"

#include "cacheddownloadmanager.h"
#include "songlengthsparser.h"

#include <QCryptographicHash>
#include <QFile>

SonglengthsDatabase::SonglengthsDatabase(IFileDownloader *downloader, QObject *parent)
    : QObject(parent)
{
    manager_ = new CachedDownloadManager(
        downloader, QStringLiteral("Songlengths.md5"), QUrl(QString::fromLatin1(DatabaseUrl)),
        [this](const QByteArray &data) -> int {
            parsedDb_ = songlengths::parseDatabase(data);
            return parsedDb_.durations.isEmpty() ? -1 : parsedDb_.durations.size();
        },
        this);

    connect(manager_, &CachedDownloadManager::downloadProgress, this,
            &SonglengthsDatabase::downloadProgress);
    connect(manager_, &CachedDownloadManager::downloadFinished, this,
            &SonglengthsDatabase::downloadFinished);
    connect(manager_, &CachedDownloadManager::downloadFailed, this,
            &SonglengthsDatabase::downloadFailed);
    connect(manager_, &CachedDownloadManager::loaded, this, &SonglengthsDatabase::databaseLoaded);
}

QString SonglengthsDatabase::cacheFilePath() const
{
    return manager_->cacheFilePath();
}

bool SonglengthsDatabase::hasCachedDatabase() const
{
    return manager_->hasCachedFile();
}

bool SonglengthsDatabase::loadFromCache()
{
    const QString path = cacheFilePath();
    if (!QFile::exists(path)) {
        return false;
    }

    if (parseDatabaseFile(path)) {
        emit databaseLoaded();
        return true;
    }
    return false;
}

QString SonglengthsDatabase::calculateMd5(const QByteArray &sidData)
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(sidData);
    return QString::fromLatin1(hash.result().toHex()).toLower();
}

SonglengthsDatabase::SongLengths SonglengthsDatabase::lookup(const QString &md5Hash) const
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

SonglengthsDatabase::SongLengths SonglengthsDatabase::lookupByData(const QByteArray &sidData) const
{
    const QString md5 = calculateMd5(sidData);
    return lookup(md5);
}

int SonglengthsDatabase::getDuration(const QString &md5Hash, int subsong) const
{
    SongLengths lengths = lookup(md5Hash);
    if (!lengths.found || subsong < 1 || subsong > lengths.durations.size()) {
        return DefaultDurationSecs;
    }
    return lengths.durations.at(subsong - 1);  // Convert 1-indexed to 0-indexed
}

int SonglengthsDatabase::getDurationByData(const QByteArray &sidData, int subsong) const
{
    const QString md5 = calculateMd5(sidData);
    return getDuration(md5, subsong);
}

void SonglengthsDatabase::downloadDatabase()
{
    manager_->download();
}

bool SonglengthsDatabase::parseDatabaseData(const QByteArray &data)
{
    parsedDb_ = songlengths::parseDatabase(data);
    return !parsedDb_.durations.isEmpty();
}

bool SonglengthsDatabase::parseDatabaseFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    return parseDatabaseData(data);
}
