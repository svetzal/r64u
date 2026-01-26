/**
 * @file hvscmetadataservice.cpp
 * @brief Implementation of the HVSCMetadataService.
 */

#include "hvscmetadataservice.h"

#include <QDir>
#include <QFile>
#include <QNetworkReply>
#include <QStandardPaths>
#include <QTextStream>
#include <QRegularExpression>

HVSCMetadataService::HVSCMetadataService(QObject *parent)
    : QObject(parent)
    , networkManager_(new QNetworkAccessManager(this))
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
    StilInfo result;
    QString path = hvscPath;

    // Normalize path: ensure it starts with / and uses forward slashes
    path = path.replace('\\', '/');
    if (!path.startsWith('/')) {
        path = '/' + path;
    }

    if (stilDatabase_.contains(path)) {
        result.found = true;
        result.path = path;
        result.entries = stilDatabase_.value(path);
    }

    return result;
}

HVSCMetadataService::BugInfo HVSCMetadataService::lookupBuglist(const QString &hvscPath) const
{
    BugInfo result;
    QString path = hvscPath;

    // Normalize path
    path = path.replace('\\', '/');
    if (!path.startsWith('/')) {
        path = '/' + path;
    }

    if (buglistDatabase_.contains(path)) {
        result.found = true;
        result.path = path;
        result.entries = buglistDatabase_.value(path);
    }

    return result;
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

    connect(stilDownload_, &QNetworkReply::downloadProgress,
            this, &HVSCMetadataService::onStilDownloadProgress);
    connect(stilDownload_, &QNetworkReply::finished,
            this, &HVSCMetadataService::onStilDownloadFinished);
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

    connect(buglistDownload_, &QNetworkReply::downloadProgress,
            this, &HVSCMetadataService::onBuglistDownloadProgress);
    connect(buglistDownload_, &QNetworkReply::finished,
            this, &HVSCMetadataService::onBuglistDownloadFinished);
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
    stilDatabase_.clear();

    QTextStream stream(data);
    stream.setEncoding(QStringConverter::Encoding::Latin1);

    // Regex for file path entries (start with / and end with .sid)
    static const QRegularExpression pathRegex(R"(^(/[^\s]+\.sid)$)", QRegularExpression::CaseInsensitiveOption);

    // Regex for subtune markers (#N)
    static const QRegularExpression subtuneRegex(R"(^\(#(\d+)\)$)");

    QString currentPath;
    QStringList currentBlock;

    while (!stream.atEnd()) {
        QString line = stream.readLine();

        // Skip section headers (lines starting with ###)
        if (line.startsWith("###")) {
            continue;
        }

        // Check for file path
        QRegularExpressionMatch pathMatch = pathRegex.match(line.trimmed());
        if (pathMatch.hasMatch()) {
            // Save previous entry
            if (!currentPath.isEmpty() && !currentBlock.isEmpty()) {
                stilDatabase_.insert(currentPath, parseStilEntry(currentBlock));
            }

            // Start new entry
            currentPath = pathMatch.captured(1);
            currentBlock.clear();
            continue;
        }

        // Accumulate lines for current entry
        if (!currentPath.isEmpty() && !line.trimmed().isEmpty()) {
            currentBlock.append(line);
        }
    }

    // Don't forget the last entry
    if (!currentPath.isEmpty() && !currentBlock.isEmpty()) {
        stilDatabase_.insert(currentPath, parseStilEntry(currentBlock));
    }

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
    buglistDatabase_.clear();

    QTextStream stream(data);
    stream.setEncoding(QStringConverter::Encoding::Latin1);

    // Regex for file path entries
    static const QRegularExpression pathRegex(R"(^(/[^\s]+\.sid)$)", QRegularExpression::CaseInsensitiveOption);

    QString currentPath;
    QStringList currentBlock;

    while (!stream.atEnd()) {
        QString line = stream.readLine();

        // Skip section headers
        if (line.startsWith("###")) {
            continue;
        }

        // Check for file path
        QRegularExpressionMatch pathMatch = pathRegex.match(line.trimmed());
        if (pathMatch.hasMatch()) {
            // Save previous entry
            if (!currentPath.isEmpty() && !currentBlock.isEmpty()) {
                buglistDatabase_.insert(currentPath, parseBugEntry(currentBlock));
            }

            // Start new entry
            currentPath = pathMatch.captured(1);
            currentBlock.clear();
            continue;
        }

        // Accumulate lines for current entry
        if (!currentPath.isEmpty() && !line.trimmed().isEmpty()) {
            currentBlock.append(line);
        }
    }

    // Don't forget the last entry
    if (!currentPath.isEmpty() && !currentBlock.isEmpty()) {
        buglistDatabase_.insert(currentPath, parseBugEntry(currentBlock));
    }

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

QList<HVSCMetadataService::SubtuneEntry> HVSCMetadataService::parseStilEntry(const QStringList &lines)
{
    QList<SubtuneEntry> entries;
    SubtuneEntry current;

    // Field patterns (with their leading spaces)
    static const QRegularExpression nameRegex(R"(^   NAME: (.+)$)");
    static const QRegularExpression authorRegex(R"(^ AUTHOR: (.+)$)");
    static const QRegularExpression titleRegex(R"(^  TITLE: (.+)$)");
    static const QRegularExpression artistRegex(R"(^ ARTIST: (.+)$)");
    static const QRegularExpression commentRegex(R"(^COMMENT: (.+)$)");
    static const QRegularExpression subtuneRegex(R"(^\(#(\d+)\)$)");
    static const QRegularExpression continuationRegex(R"(^         (.+)$)");  // 9 spaces

    QString *currentMultiline = nullptr;  // Points to field being continued
    CoverInfo *currentCover = nullptr;

    for (const QString &line : lines) {
        // Check for subtune marker
        QRegularExpressionMatch subtuneMatch = subtuneRegex.match(line.trimmed());
        if (subtuneMatch.hasMatch()) {
            // Save current entry if it has content
            if (current.subtune > 0 || !current.comment.isEmpty() ||
                !current.name.isEmpty() || !current.covers.isEmpty()) {
                entries.append(current);
            }
            current = SubtuneEntry();
            current.subtune = subtuneMatch.captured(1).toInt();
            currentMultiline = nullptr;
            currentCover = nullptr;
            continue;
        }

        // Check for NAME field
        QRegularExpressionMatch nameMatch = nameRegex.match(line);
        if (nameMatch.hasMatch()) {
            current.name = nameMatch.captured(1).trimmed();
            currentMultiline = &current.name;
            currentCover = nullptr;
            continue;
        }

        // Check for AUTHOR field
        QRegularExpressionMatch authorMatch = authorRegex.match(line);
        if (authorMatch.hasMatch()) {
            current.author = authorMatch.captured(1).trimmed();
            currentMultiline = &current.author;
            currentCover = nullptr;
            continue;
        }

        // Check for TITLE field (cover info)
        QRegularExpressionMatch titleMatch = titleRegex.match(line);
        if (titleMatch.hasMatch()) {
            CoverInfo cover;
            QString titleStr = titleMatch.captured(1).trimmed();

            // Check for timestamp in parentheses at end
            static const QRegularExpression timestampRegex(R"(\((\d+:\d{2}(?:-\d+:\d{2})?)\)$)");
            QRegularExpressionMatch tsMatch = timestampRegex.match(titleStr);
            if (tsMatch.hasMatch()) {
                cover.timestamp = tsMatch.captured(1);
                titleStr = titleStr.left(tsMatch.capturedStart()).trimmed();
            }

            cover.title = titleStr;
            current.covers.append(cover);
            currentCover = &current.covers.last();
            currentMultiline = &currentCover->title;
            continue;
        }

        // Check for ARTIST field (cover artist)
        QRegularExpressionMatch artistMatch = artistRegex.match(line);
        if (artistMatch.hasMatch()) {
            if (currentCover != nullptr) {
                currentCover->artist = artistMatch.captured(1).trimmed();
                currentMultiline = &currentCover->artist;
            }
            continue;
        }

        // Check for COMMENT field
        QRegularExpressionMatch commentMatch = commentRegex.match(line);
        if (commentMatch.hasMatch()) {
            if (!current.comment.isEmpty()) {
                current.comment += "\n";
            }
            current.comment += commentMatch.captured(1).trimmed();
            currentMultiline = &current.comment;
            currentCover = nullptr;
            continue;
        }

        // Check for continuation line (9 spaces)
        QRegularExpressionMatch contMatch = continuationRegex.match(line);
        if (contMatch.hasMatch() && currentMultiline != nullptr) {
            *currentMultiline += " " + contMatch.captured(1).trimmed();
            continue;
        }
    }

    // Don't forget the last entry
    if (current.subtune > 0 || !current.comment.isEmpty() ||
        !current.name.isEmpty() || !current.covers.isEmpty()) {
        entries.append(current);
    }

    return entries;
}

QList<HVSCMetadataService::BugEntry> HVSCMetadataService::parseBugEntry(const QStringList &lines)
{
    QList<BugEntry> entries;
    BugEntry current;

    // Field patterns
    static const QRegularExpression bugRegex(R"(^BUG: (.+)$)");
    static const QRegularExpression subtuneRegex(R"(^\(#(\d+)\)$)");
    static const QRegularExpression continuationRegex(R"(^     (.+)$)");  // 5 spaces for BUG continuation

    QString *currentMultiline = nullptr;

    for (const QString &line : lines) {
        // Check for subtune marker
        QRegularExpressionMatch subtuneMatch = subtuneRegex.match(line.trimmed());
        if (subtuneMatch.hasMatch()) {
            // Save current entry if it has content
            if (!current.description.isEmpty()) {
                entries.append(current);
            }
            current = BugEntry();
            current.subtune = subtuneMatch.captured(1).toInt();
            currentMultiline = nullptr;
            continue;
        }

        // Check for BUG field
        QRegularExpressionMatch bugMatch = bugRegex.match(line);
        if (bugMatch.hasMatch()) {
            if (!current.description.isEmpty()) {
                current.description += "\n";
            }
            current.description += bugMatch.captured(1).trimmed();
            currentMultiline = &current.description;
            continue;
        }

        // Check for continuation line
        QRegularExpressionMatch contMatch = continuationRegex.match(line);
        if (contMatch.hasMatch() && currentMultiline != nullptr) {
            *currentMultiline += " " + contMatch.captured(1).trimmed();
            continue;
        }
    }

    // Don't forget the last entry
    if (!current.description.isEmpty()) {
        entries.append(current);
    }

    return entries;
}
