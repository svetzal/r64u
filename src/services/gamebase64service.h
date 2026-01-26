#ifndef GAMEBASE64SERVICE_H
#define GAMEBASE64SERVICE_H

#include <QObject>
#include <QHash>
#include <QSqlDatabase>
#include <QNetworkAccessManager>
#include <QNetworkReply>

/**
 * @brief Service for accessing GameBase64 metadata database
 *
 * GameBase64 contains metadata for ~29,000 C64 games including:
 * - Game names, publishers, release years
 * - Genre classifications
 * - Musicians/composers
 * - Ratings and player counts
 * - Screenshot filenames
 * - Associated SID music filenames
 *
 * The database is downloaded from twinbirds.com as a gzipped SQLite file
 * and cached locally for offline use.
 */
class GameBase64Service : public QObject
{
    Q_OBJECT

public:
    /// Download URL for the GameBase64 SQLite database (gzipped)
    static constexpr const char* DatabaseUrl =
        "http://www.twinbirds.com/gamebase64browser/GBC_v18.sqlitedb.gz";

    /// Expected filename after decompression
    static constexpr const char* DatabaseFilename = "gamebase64.db";

    /**
     * @brief Information about a game from GameBase64
     */
    struct GameInfo {
        bool found = false;
        int gameId = 0;
        QString name;
        QString publisher;
        int year = 0;
        QString genre;
        QString parentGenre;
        QString musician;
        QString musicianGroup;
        QString filename;           ///< D64/CRT filename
        QString screenshotFilename;
        QString sidFilename;        ///< Associated SID music file
        int rating = 0;             ///< 0-10 rating
        int playersFrom = 1;
        int playersTo = 1;
        QString memo;               ///< Game description/memo
        QString comment;            ///< Additional comments
    };

    /**
     * @brief Search results containing multiple games
     */
    struct SearchResults {
        bool success = false;
        QString error;
        QList<GameInfo> games;
    };

    explicit GameBase64Service(QObject *parent = nullptr);
    ~GameBase64Service() override;

    // Database state
    [[nodiscard]] bool isLoaded() const { return databaseLoaded_; }
    [[nodiscard]] int gameCount() const { return gameCount_; }
    [[nodiscard]] bool hasCachedDatabase() const;
    [[nodiscard]] QString databaseCacheFilePath() const;

    // Lookup methods
    [[nodiscard]] GameInfo lookupByGameId(int gameId) const;
    [[nodiscard]] GameInfo lookupByName(const QString &name) const;
    [[nodiscard]] GameInfo lookupByFilename(const QString &filename) const;
    [[nodiscard]] GameInfo lookupBySidFilename(const QString &sidFilename) const;

    // Search methods (return multiple results)
    [[nodiscard]] SearchResults searchByName(const QString &query, int maxResults = 50) const;
    [[nodiscard]] SearchResults searchByMusician(const QString &musician, int maxResults = 50) const;
    [[nodiscard]] SearchResults searchByPublisher(const QString &publisher, int maxResults = 50) const;

    // Database management
    void downloadDatabase();
    void cancelDownload();
    void loadFromCache();
    void clearCache();

signals:
    /// Emitted during database download with progress info
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

    /// Emitted when download completes successfully
    void downloadFinished(int gameCount);

    /// Emitted when download fails
    void downloadFailed(const QString &error);

    /// Emitted when database is loaded and ready for queries
    void databaseLoaded(int gameCount);

    /// Emitted when database is unloaded/cleared
    void databaseUnloaded();

private slots:
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();

private:
    void openDatabase(const QString &path);
    void closeDatabase();
    [[nodiscard]] GameInfo buildGameInfoFromQuery(const QSqlQuery &query) const;
    [[nodiscard]] static bool decompressGzip(const QString &gzipPath, const QString &outputPath);

    QNetworkAccessManager *networkManager_ = nullptr;
    QNetworkReply *currentDownload_ = nullptr;
    QSqlDatabase database_;
    QString connectionName_;

    bool databaseLoaded_ = false;
    int gameCount_ = 0;

    // Indices for fast lookup
    QHash<QString, int> filenameToGameId_;
    QHash<QString, int> sidFilenameToGameId_;
};

#endif // GAMEBASE64SERVICE_H
