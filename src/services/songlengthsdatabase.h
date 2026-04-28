#ifndef SONGLENGTHSDATABASE_H
#define SONGLENGTHSDATABASE_H

#include "cacheddownloadmanager.h"
#include "songlengthsparser.h"

#include <QList>
#include <QObject>
#include <QString>

/**
 * @brief Manages the HVSC Songlengths database for SID duration lookup.
 *
 * The HVSC (High Voltage SID Collection) provides a database mapping
 * MD5 hashes of SID files to their song durations. This service:
 * - Downloads and caches the database locally
 * - Parses the Songlengths.md5 format
 * - Calculates MD5 hashes of SID file data
 * - Looks up durations for SID files
 */
class SonglengthsDatabase : public QObject
{
    Q_OBJECT

public:
    /// URL to download the Songlengths.md5 database
    static constexpr const char *DatabaseUrl =
        "https://www.hvsc.c64.org/download/C64Music/DOCUMENTS/Songlengths.md5";

    /// Default song duration in seconds when not in database
    static constexpr int DefaultDurationSecs = 180;

    /**
     * @brief Song duration information for a single SID file.
     */
    struct SongLengths
    {
        bool found = false;    ///< True if the SID was found in database
        QString hvscPath;      ///< HVSC path (e.g., "/MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid")
        QList<int> durations;  ///< Duration in seconds for each subsong
        QList<QString> formattedTimes;  ///< Formatted times (e.g., "3:57") for each subsong
    };

    explicit SonglengthsDatabase(IFileDownloader *downloader, QObject *parent = nullptr);
    ~SonglengthsDatabase() override = default;

    /**
     * @brief Checks if the database has been loaded.
     */
    [[nodiscard]] bool isLoaded() const { return !parsedDb_.durations.isEmpty(); }

    /**
     * @brief Returns the number of entries in the database.
     */
    [[nodiscard]] int entryCount() const { return parsedDb_.durations.size(); }

    /**
     * @brief Returns the path to the cached database file.
     */
    [[nodiscard]] QString cacheFilePath() const;

    /**
     * @brief Checks if a cached database exists.
     */
    [[nodiscard]] bool hasCachedDatabase() const;

    /**
     * @brief Loads the database from the local cache.
     * @return True if successful.
     */
    bool loadFromCache();

    /**
     * @brief Calculates the MD5 hash of SID file data.
     * @param sidData Complete SID file contents including header.
     * @return Lowercase hex MD5 hash string.
     */
    [[nodiscard]] static QString calculateMd5(const QByteArray &sidData);

    /**
     * @brief Looks up song lengths by MD5 hash.
     * @param md5Hash The MD5 hash (lowercase hex).
     * @return Song length information.
     */
    [[nodiscard]] SongLengths lookup(const QString &md5Hash) const;

    /**
     * @brief Looks up song lengths for SID file data.
     * @param sidData Complete SID file contents.
     * @return Song length information.
     */
    [[nodiscard]] SongLengths lookupByData(const QByteArray &sidData) const;

    /**
     * @brief Gets the duration for a specific subsong.
     * @param md5Hash The MD5 hash.
     * @param subsong Subsong number (1-indexed).
     * @return Duration in seconds, or DefaultDurationSecs if not found.
     */
    [[nodiscard]] int getDuration(const QString &md5Hash, int subsong) const;

    /**
     * @brief Gets the duration for a specific subsong from SID data.
     * @param sidData Complete SID file contents.
     * @param subsong Subsong number (1-indexed).
     * @return Duration in seconds, or DefaultDurationSecs if not found.
     */
    [[nodiscard]] int getDurationByData(const QByteArray &sidData, int subsong) const;

public slots:
    /**
     * @brief Downloads the database from HVSC.
     *
     * Emits downloadProgress() during download, then either
     * downloadFinished() or downloadFailed().
     */
    void downloadDatabase();

signals:
    /**
     * @brief Emitted during database download.
     * @param bytesReceived Bytes downloaded so far.
     * @param bytesTotal Total bytes to download (-1 if unknown).
     */
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

    /**
     * @brief Emitted when download and loading completes successfully.
     * @param entryCount Number of entries loaded.
     */
    void downloadFinished(int entryCount);

    /**
     * @brief Emitted when download or parsing fails.
     * @param error Error message.
     */
    void downloadFailed(const QString &error);

    /**
     * @brief Emitted when the database is loaded (from cache or download).
     */
    void databaseLoaded();

private:
    bool parseDatabaseData(const QByteArray &data);
    bool parseDatabaseFile(const QString &filePath);

    CachedDownloadManager *manager_;
    songlengths::ParsedDatabase parsedDb_;
};

#endif  // SONGLENGTHSDATABASE_H
