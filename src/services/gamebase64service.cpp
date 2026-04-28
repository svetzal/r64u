#include "gamebase64service.h"

#include "cacheddownloadmanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

// For gzip decompression
#include <zlib.h>

GameBase64Service::GameBase64Service(IFileDownloader *downloader, QObject *parent)
    : QObject(parent), connectionName_(QUuid::createUuid().toString())
{
    manager_ = new CachedDownloadManager(
        downloader, QStringLiteral("gamebase64.db.gz"), QUrl(QString::fromLatin1(DatabaseUrl)),
        [this](const QByteArray & /*data*/) -> int {
            // The gz file has already been saved to manager_->cacheFilePath() by the
            // CachedDownloadManager before this callback is invoked.  Decompress it
            // to the database path, then remove the temporary gz file.
            const QString gzipPath = manager_->cacheFilePath();
            const QString dbPath = databaseCacheFilePath();

            if (!decompressGzip(gzipPath, dbPath)) {
                QFile::remove(gzipPath);
                return -1;
            }

            QFile::remove(gzipPath);

            openDatabase(dbPath);
            return databaseLoaded_ ? gameCount_ : -1;
        },
        this);

    connect(manager_, &CachedDownloadManager::downloadProgress, this,
            &GameBase64Service::downloadProgress);
    connect(manager_, &CachedDownloadManager::downloadFinished, this,
            &GameBase64Service::downloadFinished);
    connect(manager_, &CachedDownloadManager::downloadFailed, this,
            &GameBase64Service::downloadFailed);
    // manager_->loaded() is not forwarded here: GameBase64Service emits databaseLoaded(int)
    // from openDatabase(), which is invoked inside the parse callback above.

    // Try to load from cache on startup (the decompressed .db, not the .gz)
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
    manager_->download();
}

void GameBase64Service::cancelDownload()
{
    manager_->cancelDownload();
}

bool GameBase64Service::decompressGzip(const QString &gzipPath, const QString &outputPath)
{
    gzFile gzFileHandle = gzopen(gzipPath.toLocal8Bit().constData(), "rb");
    if (gzFileHandle == nullptr) {
        return false;
    }

    QFile outFile(outputPath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        gzclose(gzFileHandle);
        return false;
    }

    constexpr int bufferSize = 65536;
    char buffer[bufferSize];
    int bytesRead = 0;

    while ((bytesRead = gzread(gzFileHandle, buffer, bufferSize)) > 0) {
        if (outFile.write(buffer, bytesRead) != bytesRead) {
            gzclose(gzFileHandle);
            outFile.close();
            QFile::remove(outputPath);
            return false;
        }
    }

    bool success = (bytesRead == 0);  // 0 means EOF, negative means error
    gzclose(gzFileHandle);
    outFile.close();

    if (!success) {
        QFile::remove(outputPath);
    }

    return success;
}

void GameBase64Service::loadFromCache()
{
    const QString dbPath = databaseCacheFilePath();
    if (!QFile::exists(dbPath)) {
        return;
    }

    openDatabase(dbPath);
}

void GameBase64Service::clearCache()
{
    closeDatabase();

    const QString dbPath = databaseCacheFilePath();
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
    // Note: GameBase64 stores full paths like "Games\C\Commando.d64"
    // We extract just the base filename for matching
    QSqlQuery indexQuery(database_);
    if (indexQuery.exec("SELECT GA_Id, Filename, SidFilename FROM Games")) {
        while (indexQuery.next()) {
            int gameId = indexQuery.value(0).toInt();
            QString filename = indexQuery.value(1).toString();
            QString sidFilename = indexQuery.value(2).toString();

            if (!filename.isEmpty()) {
                // Extract base filename and normalize to lowercase
                QString baseName = QFileInfo(filename).fileName().toLower();
                if (!baseName.isEmpty()) {
                    filenameToGameId_[baseName] = gameId;
                }
            }
            if (!sidFilename.isEmpty()) {
                // Extract base filename and normalize to lowercase
                QString baseName = QFileInfo(sidFilename).fileName().toLower();
                if (!baseName.isEmpty()) {
                    sidFilenameToGameId_[baseName] = gameId;
                }
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

GameBase64Service::SearchResults GameBase64Service::searchByName(const QString &searchQuery,
                                                                 int maxResults) const
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

GameBase64Service::SearchResults GameBase64Service::searchByMusician(const QString &musician,
                                                                     int maxResults) const
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

GameBase64Service::SearchResults GameBase64Service::searchByPublisher(const QString &publisher,
                                                                      int maxResults) const
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
