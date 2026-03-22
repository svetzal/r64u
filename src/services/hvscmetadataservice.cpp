/**
 * @file hvscmetadataservice.cpp
 * @brief Implementation of the HVSCMetadataService.
 */

#include "hvscmetadataservice.h"

#include "hvscparser.h"

#include <QDir>
#include <QFile>
#include <QNetworkReply>
#include <QStandardPaths>

HVSCMetadataService::HVSCMetadataService(QObject *parent)
    : QObject(parent), networkManager_(new QNetworkAccessManager(this))
{
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
    if (stilDownload_) {
        // Already downloading
        return;
    }

    QUrl url(StilUrl);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "r64u/1.0");

    stilDownload_ = networkManager_->get(request);

    connect(stilDownload_, &QNetworkReply::downloadProgress, this,
            &HVSCMetadataService::onStilDownloadProgress);
    connect(stilDownload_, &QNetworkReply::finished, this,
            &HVSCMetadataService::onStilDownloadFinished);
}

void HVSCMetadataService::downloadBuglist()
{
    if (buglistDownload_) {
        // Already downloading
        return;
    }

    QUrl url(BuglistUrl);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "r64u/1.0");

    buglistDownload_ = networkManager_->get(request);

    connect(buglistDownload_, &QNetworkReply::downloadProgress, this,
            &HVSCMetadataService::onBuglistDownloadProgress);
    connect(buglistDownload_, &QNetworkReply::finished, this,
            &HVSCMetadataService::onBuglistDownloadFinished);
}

void HVSCMetadataService::onStilDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    emit stilDownloadProgress(bytesReceived, bytesTotal);
}

void HVSCMetadataService::onStilDownloadFinished()
{
    if (!stilDownload_) {
        return;
    }

    QNetworkReply *reply = stilDownload_;
    stilDownload_ = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        emit stilDownloadFailed(reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    if (data.isEmpty()) {
        emit stilDownloadFailed(tr("Downloaded file is empty"));
        return;
    }

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

void HVSCMetadataService::onBuglistDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    emit buglistDownloadProgress(bytesReceived, bytesTotal);
}

void HVSCMetadataService::onBuglistDownloadFinished()
{
    if (!buglistDownload_) {
        return;
    }

    QNetworkReply *reply = buglistDownload_;
    buglistDownload_ = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        emit buglistDownloadFailed(reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    if (data.isEmpty()) {
        emit buglistDownloadFailed(tr("Downloaded file is empty"));
        return;
    }

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
