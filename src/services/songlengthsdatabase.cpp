/**
 * @file songlengthsdatabase.cpp
 * @brief Implementation of the SonglengthsDatabase service.
 */

#include "songlengthsdatabase.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QNetworkReply>
#include <QStandardPaths>
#include <QTextStream>
#include <QRegularExpression>

SonglengthsDatabase::SonglengthsDatabase(QObject *parent)
    : QObject(parent)
    , networkManager_(new QNetworkAccessManager(this))
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

    if (database_.contains(hash)) {
        result.found = true;
        result.durations = database_.value(hash);
        result.formattedTimes = formattedTimes_.value(hash);
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

    connect(currentDownload_, &QNetworkReply::downloadProgress,
            this, &SonglengthsDatabase::onDownloadProgress);
    connect(currentDownload_, &QNetworkReply::finished,
            this, &SonglengthsDatabase::onDownloadFinished);
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
        emit downloadFinished(database_.size());
        emit databaseLoaded();
    } else {
        emit downloadFailed(tr("Failed to parse database"));
    }
}

bool SonglengthsDatabase::parseDatabase(const QByteArray &data)
{
    database_.clear();
    formattedTimes_.clear();

    QTextStream stream(data);
    stream.setEncoding(QStringConverter::Encoding::Latin1);

    // Regular expression to match database entries
    // Format: <32-char hex MD5>=<time> <time> ...
    static const QRegularExpression entryRegex(
        R"(^([0-9a-fA-F]{32})=(.+)$)"
    );

    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();

        // Skip empty lines, comments, and section headers
        if (line.isEmpty() || line.startsWith(';') || line.startsWith('[')) {
            continue;
        }

        QRegularExpressionMatch match = entryRegex.match(line);
        if (match.hasMatch()) {
            QString md5 = match.captured(1).toLower();
            QString timesStr = match.captured(2);

            QList<int> durations = parseTimeList(timesStr);
            if (!durations.isEmpty()) {
                database_.insert(md5, durations);

                // Store formatted times too
                QList<QString> formatted;
                QStringList parts = timesStr.split(' ', Qt::SkipEmptyParts);
                for (const QString &part : parts) {
                    // Remove milliseconds for display
                    QString clean = part;
                    int dotPos = clean.indexOf('.');
                    if (dotPos >= 0) {
                        clean = clean.left(dotPos);
                    }
                    formatted.append(clean);
                }
                formattedTimes_.insert(md5, formatted);
            }
        }
    }

    return !database_.isEmpty();
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

QList<int> SonglengthsDatabase::parseTimeList(const QString &timeStr)
{
    QList<int> durations;
    QStringList parts = timeStr.split(' ', Qt::SkipEmptyParts);

    for (const QString &part : parts) {
        int seconds = parseTime(part);
        if (seconds > 0) {
            durations.append(seconds);
        }
    }

    return durations;
}

int SonglengthsDatabase::parseTime(const QString &time)
{
    // Format: mm:ss or mm:ss.SSS
    static const QRegularExpression timeRegex(R"(^(\d+):(\d{2})(?:\.(\d{1,3}))?$)");

    QRegularExpressionMatch match = timeRegex.match(time);
    if (!match.hasMatch()) {
        return 0;
    }

    int minutes = match.captured(1).toInt();
    int seconds = match.captured(2).toInt();
    // We ignore milliseconds for now

    return (minutes * 60) + seconds;
}
