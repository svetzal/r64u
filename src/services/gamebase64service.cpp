#include "gamebase64service.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QUuid>

// For gzip decompression
#include <zlib.h>

GameBase64Service::GameBase64Service(QObject *parent)
    : QObject(parent)
    , networkManager_(new QNetworkAccessManager(this))
    , connectionName_(QUuid::createUuid().toString())
{
    // Try to load from cache on startup
    if (hasCachedDatabase()) {
        loadFromCache();
    }
}

GameBase64Service::~GameBase64Service()
{
    closeDatabase();
}

bool GameBase64Service::hasCachedDatabase() const
{
    return QFile::exists(databaseCacheFilePath());
}

QString GameBase64Service::databaseCacheFilePath() const
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dataPath + "/" + DatabaseFilename;
}

void GameBase64Service::downloadDatabase()
{
    if (currentDownload_ != nullptr) {
        return; // Already downloading
    }

    // Ensure cache directory exists
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataPath);

    QUrl url{QString::fromLatin1(DatabaseUrl)};
    QNetworkRequest request{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                        QNetworkRequest::NoLessSafeRedirectPolicy);

    currentDownload_ = networkManager_->get(request);
    connect(currentDownload_, &QNetworkReply::downloadProgress,
            this, &GameBase64Service::onDownloadProgress);
    connect(currentDownload_, &QNetworkReply::finished,
            this, &GameBase64Service::onDownloadFinished);
}

void GameBase64Service::cancelDownload()
{
    if (currentDownload_ != nullptr) {
        currentDownload_->abort();
        currentDownload_->deleteLater();
        currentDownload_ = nullptr;
    }
}

void GameBase64Service::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    emit downloadProgress(bytesReceived, bytesTotal);
}

void GameBase64Service::onDownloadFinished()
{
    if (currentDownload_ == nullptr) {
        return;
    }

    QNetworkReply *reply = currentDownload_;
    currentDownload_ = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg = reply->errorString();
        reply->deleteLater();
        emit downloadFailed(errorMsg);
        return;
    }

    // Save compressed data to temp file
    QByteArray compressedData = reply->readAll();
    reply->deleteLater();

    if (compressedData.isEmpty()) {
        emit downloadFailed(tr("Downloaded file is empty"));
        return;
    }

    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString gzipPath = dataPath + "/gamebase64.db.gz";
    QString dbPath = databaseCacheFilePath();

    // Write compressed file
    QFile gzipFile(gzipPath);
    if (!gzipFile.open(QIODevice::WriteOnly)) {
        emit downloadFailed(tr("Failed to save compressed database: %1").arg(gzipFile.errorString()));
        return;
    }
    gzipFile.write(compressedData);
    gzipFile.close();

    // Decompress
    if (!decompressGzip(gzipPath, dbPath)) {
        QFile::remove(gzipPath);
        emit downloadFailed(tr("Failed to decompress database"));
        return;
    }

    // Clean up compressed file
    QFile::remove(gzipPath);

    // Load the database
    loadFromCache();

    if (databaseLoaded_) {
        emit downloadFinished(gameCount_);
    } else {
        emit downloadFailed(tr("Failed to load downloaded database"));
    }
}

bool GameBase64Service::decompressGzip(const QString &gzipPath, const QString &outputPath)
{
    gzFile gzFile = gzopen(gzipPath.toLocal8Bit().constData(), "rb");
    if (gzFile == nullptr) {
        return false;
    }

    QFile outFile(outputPath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        gzclose(gzFile);
        return false;
    }

    constexpr int bufferSize = 65536;
    char buffer[bufferSize];
    int bytesRead = 0;

    while ((bytesRead = gzread(gzFile, buffer, bufferSize)) > 0) {
        if (outFile.write(buffer, bytesRead) != bytesRead) {
            gzclose(gzFile);
            outFile.close();
            QFile::remove(outputPath);
            return false;
        }
    }

    bool success = (bytesRead == 0); // 0 means EOF, negative means error
    gzclose(gzFile);
    outFile.close();

    if (!success) {
        QFile::remove(outputPath);
    }

    return success;
}

void GameBase64Service::loadFromCache()
{
    QString dbPath = databaseCacheFilePath();
    if (!QFile::exists(dbPath)) {
        return;
    }

    openDatabase(dbPath);
}

void GameBase64Service::clearCache()
{
    closeDatabase();

    QString dbPath = databaseCacheFilePath();
    if (QFile::exists(dbPath)) {
        QFile::remove(dbPath);
    }

    emit databaseUnloaded();
}

void GameBase64Service::openDatabase(const QString &path)
{
    closeDatabase();

    database_ = QSqlDatabase::addDatabase("QSQLITE", connectionName_);
    database_.setDatabaseName(path);

    if (!database_.open()) {
        qWarning() << "Failed to open GameBase64 database:" << database_.lastError().text();
        return;
    }

    // Count games
    QSqlQuery countQuery(database_);
    if (countQuery.exec("SELECT COUNT(*) FROM Games") && countQuery.next()) {
        gameCount_ = countQuery.value(0).toInt();
    }

    // Build filename indices for fast lookup
    QSqlQuery indexQuery(database_);
    if (indexQuery.exec("SELECT GA_Id, Filename, SidFilename FROM Games")) {
        while (indexQuery.next()) {
            int gameId = indexQuery.value(0).toInt();
            QString filename = indexQuery.value(1).toString().toLower();
            QString sidFilename = indexQuery.value(2).toString().toLower();

            if (!filename.isEmpty()) {
                filenameToGameId_[filename] = gameId;
            }
            if (!sidFilename.isEmpty()) {
                sidFilenameToGameId_[sidFilename] = gameId;
            }
        }
    }

    databaseLoaded_ = true;
    emit databaseLoaded(gameCount_);
}

void GameBase64Service::closeDatabase()
{
    if (database_.isOpen()) {
        database_.close();
    }
    database_ = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName_);

    databaseLoaded_ = false;
    gameCount_ = 0;
    filenameToGameId_.clear();
    sidFilenameToGameId_.clear();
}

GameBase64Service::GameInfo GameBase64Service::lookupByGameId(int gameId) const
{
    GameInfo info;
    if (!databaseLoaded_) {
        return info;
    }

    QSqlQuery query(database_);
    query.prepare(R"(
        SELECT g.GA_Id, g.Name, g.Filename, g.ScrnshotFilename, g.SidFilename,
               g.Rating, g.PlayersFrom, g.PlayersTo, g.MemoText, g.Comment,
               p.Publisher, y.Year, ge.Genre, pg.Genre as ParentGenre,
               m.Musician, m.Grp
        FROM Games g
        LEFT JOIN Publishers p ON g.PU_Id = p.PU_Id
        LEFT JOIN Years y ON g.YE_Id = y.YE_Id
        LEFT JOIN Genres ge ON g.GE_Id = ge.GE_Id
        LEFT JOIN Genres pg ON ge.PG_Id = pg.GE_Id
        LEFT JOIN Musicians m ON g.MU_Id = m.MU_Id
        WHERE g.GA_Id = ?
    )");
    query.addBindValue(gameId);

    if (query.exec() && query.next()) {
        info = buildGameInfoFromQuery(query);
    }

    return info;
}

GameBase64Service::GameInfo GameBase64Service::lookupByName(const QString &name) const
{
    GameInfo info;
    if (!databaseLoaded_ || name.isEmpty()) {
        return info;
    }

    QSqlQuery query(database_);
    query.prepare(R"(
        SELECT g.GA_Id, g.Name, g.Filename, g.ScrnshotFilename, g.SidFilename,
               g.Rating, g.PlayersFrom, g.PlayersTo, g.MemoText, g.Comment,
               p.Publisher, y.Year, ge.Genre, pg.Genre as ParentGenre,
               m.Musician, m.Grp
        FROM Games g
        LEFT JOIN Publishers p ON g.PU_Id = p.PU_Id
        LEFT JOIN Years y ON g.YE_Id = y.YE_Id
        LEFT JOIN Genres ge ON g.GE_Id = ge.GE_Id
        LEFT JOIN Genres pg ON ge.PG_Id = pg.GE_Id
        LEFT JOIN Musicians m ON g.MU_Id = m.MU_Id
        WHERE LOWER(g.Name) = LOWER(?)
        LIMIT 1
    )");
    query.addBindValue(name);

    if (query.exec() && query.next()) {
        info = buildGameInfoFromQuery(query);
    }

    return info;
}

GameBase64Service::GameInfo GameBase64Service::lookupByFilename(const QString &filename) const
{
    GameInfo info;
    if (!databaseLoaded_ || filename.isEmpty()) {
        return info;
    }

    // Extract just the filename without path
    QString baseName = QFileInfo(filename).fileName().toLower();

    // Try index first
    auto it = filenameToGameId_.find(baseName);
    if (it != filenameToGameId_.end()) {
        return lookupByGameId(it.value());
    }

    // Fallback to LIKE query for partial matches
    QSqlQuery query(database_);
    query.prepare(R"(
        SELECT g.GA_Id, g.Name, g.Filename, g.ScrnshotFilename, g.SidFilename,
               g.Rating, g.PlayersFrom, g.PlayersTo, g.MemoText, g.Comment,
               p.Publisher, y.Year, ge.Genre, pg.Genre as ParentGenre,
               m.Musician, m.Grp
        FROM Games g
        LEFT JOIN Publishers p ON g.PU_Id = p.PU_Id
        LEFT JOIN Years y ON g.YE_Id = y.YE_Id
        LEFT JOIN Genres ge ON g.GE_Id = ge.GE_Id
        LEFT JOIN Genres pg ON ge.PG_Id = pg.GE_Id
        LEFT JOIN Musicians m ON g.MU_Id = m.MU_Id
        WHERE LOWER(g.Filename) LIKE ?
        LIMIT 1
    )");
    query.addBindValue("%" + baseName);

    if (query.exec() && query.next()) {
        info = buildGameInfoFromQuery(query);
    }

    return info;
}

GameBase64Service::GameInfo GameBase64Service::lookupBySidFilename(const QString &sidFilename) const
{
    GameInfo info;
    if (!databaseLoaded_ || sidFilename.isEmpty()) {
        return info;
    }

    // Extract just the filename without path
    QString baseName = QFileInfo(sidFilename).fileName().toLower();

    // Try index first
    auto it = sidFilenameToGameId_.find(baseName);
    if (it != sidFilenameToGameId_.end()) {
        return lookupByGameId(it.value());
    }

    // Fallback to LIKE query
    QSqlQuery query(database_);
    query.prepare(R"(
        SELECT g.GA_Id, g.Name, g.Filename, g.ScrnshotFilename, g.SidFilename,
               g.Rating, g.PlayersFrom, g.PlayersTo, g.MemoText, g.Comment,
               p.Publisher, y.Year, ge.Genre, pg.Genre as ParentGenre,
               m.Musician, m.Grp
        FROM Games g
        LEFT JOIN Publishers p ON g.PU_Id = p.PU_Id
        LEFT JOIN Years y ON g.YE_Id = y.YE_Id
        LEFT JOIN Genres ge ON g.GE_Id = ge.GE_Id
        LEFT JOIN Genres pg ON ge.PG_Id = pg.GE_Id
        LEFT JOIN Musicians m ON g.MU_Id = m.MU_Id
        WHERE LOWER(g.SidFilename) LIKE ?
        LIMIT 1
    )");
    query.addBindValue("%" + baseName);

    if (query.exec() && query.next()) {
        info = buildGameInfoFromQuery(query);
    }

    return info;
}

GameBase64Service::SearchResults GameBase64Service::searchByName(const QString &searchQuery, int maxResults) const
{
    SearchResults results;
    if (!databaseLoaded_ || searchQuery.isEmpty()) {
        results.success = true;
        return results;
    }

    QSqlQuery query(database_);
    query.prepare(R"(
        SELECT g.GA_Id, g.Name, g.Filename, g.ScrnshotFilename, g.SidFilename,
               g.Rating, g.PlayersFrom, g.PlayersTo, g.MemoText, g.Comment,
               p.Publisher, y.Year, ge.Genre, pg.Genre as ParentGenre,
               m.Musician, m.Grp
        FROM Games g
        LEFT JOIN Publishers p ON g.PU_Id = p.PU_Id
        LEFT JOIN Years y ON g.YE_Id = y.YE_Id
        LEFT JOIN Genres ge ON g.GE_Id = ge.GE_Id
        LEFT JOIN Genres pg ON ge.PG_Id = pg.GE_Id
        LEFT JOIN Musicians m ON g.MU_Id = m.MU_Id
        WHERE g.Name LIKE ?
        ORDER BY g.Name
        LIMIT ?
    )");
    query.addBindValue("%" + searchQuery + "%");
    query.addBindValue(maxResults);

    if (!query.exec()) {
        results.error = query.lastError().text();
        return results;
    }

    results.success = true;
    while (query.next()) {
        results.games.append(buildGameInfoFromQuery(query));
    }

    return results;
}

GameBase64Service::SearchResults GameBase64Service::searchByMusician(const QString &musician, int maxResults) const
{
    SearchResults results;
    if (!databaseLoaded_ || musician.isEmpty()) {
        results.success = true;
        return results;
    }

    QSqlQuery query(database_);
    query.prepare(R"(
        SELECT g.GA_Id, g.Name, g.Filename, g.ScrnshotFilename, g.SidFilename,
               g.Rating, g.PlayersFrom, g.PlayersTo, g.MemoText, g.Comment,
               p.Publisher, y.Year, ge.Genre, pg.Genre as ParentGenre,
               m.Musician, m.Grp
        FROM Games g
        LEFT JOIN Publishers p ON g.PU_Id = p.PU_Id
        LEFT JOIN Years y ON g.YE_Id = y.YE_Id
        LEFT JOIN Genres ge ON g.GE_Id = ge.GE_Id
        LEFT JOIN Genres pg ON ge.PG_Id = pg.GE_Id
        LEFT JOIN Musicians m ON g.MU_Id = m.MU_Id
        WHERE m.Musician LIKE ?
        ORDER BY g.Name
        LIMIT ?
    )");
    query.addBindValue("%" + musician + "%");
    query.addBindValue(maxResults);

    if (!query.exec()) {
        results.error = query.lastError().text();
        return results;
    }

    results.success = true;
    while (query.next()) {
        results.games.append(buildGameInfoFromQuery(query));
    }

    return results;
}

GameBase64Service::SearchResults GameBase64Service::searchByPublisher(const QString &publisher, int maxResults) const
{
    SearchResults results;
    if (!databaseLoaded_ || publisher.isEmpty()) {
        results.success = true;
        return results;
    }

    QSqlQuery query(database_);
    query.prepare(R"(
        SELECT g.GA_Id, g.Name, g.Filename, g.ScrnshotFilename, g.SidFilename,
               g.Rating, g.PlayersFrom, g.PlayersTo, g.MemoText, g.Comment,
               p.Publisher, y.Year, ge.Genre, pg.Genre as ParentGenre,
               m.Musician, m.Grp
        FROM Games g
        LEFT JOIN Publishers p ON g.PU_Id = p.PU_Id
        LEFT JOIN Years y ON g.YE_Id = y.YE_Id
        LEFT JOIN Genres ge ON g.GE_Id = ge.GE_Id
        LEFT JOIN Genres pg ON ge.PG_Id = pg.GE_Id
        LEFT JOIN Musicians m ON g.MU_Id = m.MU_Id
        WHERE p.Publisher LIKE ?
        ORDER BY g.Name
        LIMIT ?
    )");
    query.addBindValue("%" + publisher + "%");
    query.addBindValue(maxResults);

    if (!query.exec()) {
        results.error = query.lastError().text();
        return results;
    }

    results.success = true;
    while (query.next()) {
        results.games.append(buildGameInfoFromQuery(query));
    }

    return results;
}

GameBase64Service::GameInfo GameBase64Service::buildGameInfoFromQuery(const QSqlQuery &query) const
{
    GameInfo info;
    info.found = true;
    info.gameId = query.value("GA_Id").toInt();
    info.name = query.value("Name").toString();
    info.filename = query.value("Filename").toString();
    info.screenshotFilename = query.value("ScrnshotFilename").toString();
    info.sidFilename = query.value("SidFilename").toString();
    info.rating = query.value("Rating").toInt();
    info.playersFrom = query.value("PlayersFrom").toInt();
    info.playersTo = query.value("PlayersTo").toInt();
    info.memo = query.value("MemoText").toString();
    info.comment = query.value("Comment").toString();
    info.publisher = query.value("Publisher").toString();
    info.year = query.value("Year").toInt();
    info.genre = query.value("Genre").toString();
    info.parentGenre = query.value("ParentGenre").toString();
    info.musician = query.value("Musician").toString();
    info.musicianGroup = query.value("Grp").toString();

    return info;
}
