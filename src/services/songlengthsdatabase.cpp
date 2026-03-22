/**
 * @file songlengthsdatabase.cpp
 * @brief Implementation of the SonglengthsDatabase service.
 */

#include "songlengthsdatabase.h"

#include "songlengthsparser.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QNetworkReply>
#include <QStandardPaths>

SonglengthsDatabase::SonglengthsDatabase(QObject *parent)
    : QObject(parent), networkManager_(new QNetworkAccessManager(this))
{
    // Try to load from cache on startup
    if (hasCachedDatabase()) {
        loadFromCache();
    }
}

QString SonglengthsDatabase::cacheFilePath() const
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    return dataDir + "/Songlengths.md5";
}

bool SonglengthsDatabase::hasCachedDatabase() const
{
    return QFile::exists(cacheFilePath());
}

bool SonglengthsDatabase::loadFromCache()
{
    QString path = cacheFilePath();
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
    QString md5 = calculateMd5(sidData);
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
    QString md5 = calculateMd5(sidData);
    return getDuration(md5, subsong);
}

void SonglengthsDatabase::downloadDatabase()
{
    if (currentDownload_) {
        // Already downloading
        return;
    }

    QUrl url(DatabaseUrl);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "r64u/1.0");

    currentDownload_ = networkManager_->get(request);

    connect(currentDownload_, &QNetworkReply::downloadProgress, this,
            &SonglengthsDatabase::onDownloadProgress);
    connect(currentDownload_, &QNetworkReply::finished, this,
            &SonglengthsDatabase::onDownloadFinished);
}

void SonglengthsDatabase::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    emit downloadProgress(bytesReceived, bytesTotal);
}

void SonglengthsDatabase::onDownloadFinished()
{
    if (!currentDownload_) {
        return;
    }

    QNetworkReply *reply = currentDownload_;
    currentDownload_ = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        emit downloadFailed(reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    if (data.isEmpty()) {
        emit downloadFailed(tr("Downloaded file is empty"));
        return;
    }

    // Save to cache
    QFile cacheFile(cacheFilePath());
    if (cacheFile.open(QIODevice::WriteOnly)) {
        cacheFile.write(data);
        cacheFile.close();
    }

    // Parse the database
    if (parseDatabase(data)) {
        emit downloadFinished(parsedDb_.durations.size());
        emit databaseLoaded();
    } else {
        emit downloadFailed(tr("Failed to parse database"));
    }
}

bool SonglengthsDatabase::parseDatabase(const QByteArray &data)
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

    QByteArray data = file.readAll();
    file.close();

    return parseDatabase(data);
}
