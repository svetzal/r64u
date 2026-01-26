/**
 * @file hvscmetadataservice.h
 * @brief Service for managing HVSC STIL and BUGlist metadata.
 *
 * Downloads, caches, and queries the HVSC STIL.txt and BUGlist.txt files
 * to provide tune commentary, cover information, and bug warnings.
 */

#ifndef HVSCMETADATASERVICE_H
#define HVSCMETADATASERVICE_H

#include <QObject>
#include <QString>
#include <QHash>
#include <QList>
#include <QNetworkAccessManager>

/**
 * @brief Manages HVSC STIL and BUGlist databases for SID metadata lookup.
 *
 * The HVSC (High Voltage SID Collection) provides metadata files:
 * - STIL.txt: SID Tune Information List (comments, cover info, history)
 * - BUGlist.txt: Known bugs and playback issues
 *
 * This service downloads, caches, and parses these files for lookup.
 */
class HVSCMetadataService : public QObject
{
    Q_OBJECT

public:
    /// URL to download STIL.txt
    static constexpr const char* StilUrl =
        "https://www.hvsc.c64.org/download/C64Music/DOCUMENTS/STIL.txt";

    /// URL to download BUGlist.txt
    static constexpr const char* BuglistUrl =
        "https://www.hvsc.c64.org/download/C64Music/DOCUMENTS/BUGlist.txt";

    /**
     * @brief Information about a cover/sample in a tune.
     */
    struct CoverInfo {
        QString title;      ///< Original song title
        QString artist;     ///< Original artist
        QString timestamp;  ///< Optional timestamp (e.g., "1:05" or "1:05-2:30")
    };

    /**
     * @brief STIL entry for a single subtune.
     */
    struct SubtuneEntry {
        int subtune = 0;            ///< Subtune number (0 = whole file)
        QString name;               ///< Tune name
        QString author;             ///< Tune author (if different from SID header)
        QString comment;            ///< Commentary/history
        QList<CoverInfo> covers;    ///< Cover/sample information
    };

    /**
     * @brief Complete STIL information for a SID file.
     */
    struct StilInfo {
        bool found = false;             ///< True if entry exists in STIL
        QString path;                   ///< HVSC path
        QList<SubtuneEntry> entries;    ///< Entries for file and/or subtunes
    };

    /**
     * @brief Bug report for a SID file.
     */
    struct BugEntry {
        int subtune = 0;        ///< Subtune number (0 = whole file)
        QString description;    ///< Bug description
    };

    /**
     * @brief Complete bug information for a SID file.
     */
    struct BugInfo {
        bool found = false;         ///< True if entry exists in BUGlist
        QString path;               ///< HVSC path
        QList<BugEntry> entries;    ///< Bug entries for file and/or subtunes
    };

    explicit HVSCMetadataService(QObject *parent = nullptr);
    ~HVSCMetadataService() override = default;

    /**
     * @brief Checks if STIL database has been loaded.
     */
    [[nodiscard]] bool isStilLoaded() const { return !stilDatabase_.isEmpty(); }

    /**
     * @brief Checks if BUGlist database has been loaded.
     */
    [[nodiscard]] bool isBuglistLoaded() const { return !buglistDatabase_.isEmpty(); }

    /**
     * @brief Returns the number of entries in STIL database.
     */
    [[nodiscard]] int stilEntryCount() const { return stilDatabase_.size(); }

    /**
     * @brief Returns the number of entries in BUGlist database.
     */
    [[nodiscard]] int buglistEntryCount() const { return buglistDatabase_.size(); }

    /**
     * @brief Returns the path to the cached STIL file.
     */
    [[nodiscard]] QString stilCacheFilePath() const;

    /**
     * @brief Returns the path to the cached BUGlist file.
     */
    [[nodiscard]] QString buglistCacheFilePath() const;

    /**
     * @brief Checks if cached STIL file exists.
     */
    [[nodiscard]] bool hasCachedStil() const;

    /**
     * @brief Checks if cached BUGlist file exists.
     */
    [[nodiscard]] bool hasCachedBuglist() const;

    /**
     * @brief Loads STIL from local cache.
     * @return True if successful.
     */
    bool loadStilFromCache();

    /**
     * @brief Loads BUGlist from local cache.
     * @return True if successful.
     */
    bool loadBuglistFromCache();

    /**
     * @brief Looks up STIL information by HVSC path.
     * @param hvscPath Path relative to HVSC root (e.g., "/MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid")
     * @return STIL information.
     */
    [[nodiscard]] StilInfo lookupStil(const QString &hvscPath) const;

    /**
     * @brief Looks up bug information by HVSC path.
     * @param hvscPath Path relative to HVSC root.
     * @return Bug information.
     */
    [[nodiscard]] BugInfo lookupBuglist(const QString &hvscPath) const;

public slots:
    /**
     * @brief Downloads the STIL database from HVSC.
     *
     * Emits stilDownloadProgress() during download, then either
     * stilDownloadFinished() or stilDownloadFailed().
     */
    void downloadStil();

    /**
     * @brief Downloads the BUGlist database from HVSC.
     *
     * Emits buglistDownloadProgress() during download, then either
     * buglistDownloadFinished() or buglistDownloadFailed().
     */
    void downloadBuglist();

signals:
    // STIL signals
    void stilDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void stilDownloadFinished(int entryCount);
    void stilDownloadFailed(const QString &error);
    void stilLoaded();

    // BUGlist signals
    void buglistDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void buglistDownloadFinished(int entryCount);
    void buglistDownloadFailed(const QString &error);
    void buglistLoaded();

private slots:
    void onStilDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onStilDownloadFinished();
    void onBuglistDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onBuglistDownloadFinished();

private:
    bool parseStil(const QByteArray &data);
    bool parseStilFile(const QString &filePath);
    bool parseBuglist(const QByteArray &data);
    bool parseBuglistFile(const QString &filePath);

    // Parse a single STIL/BUGlist entry block (lines between file paths)
    static QList<SubtuneEntry> parseStilEntry(const QStringList &lines);
    static QList<BugEntry> parseBugEntry(const QStringList &lines);

    QNetworkAccessManager *networkManager_ = nullptr;
    QNetworkReply *stilDownload_ = nullptr;
    QNetworkReply *buglistDownload_ = nullptr;

    // STIL database: HVSC path -> entries
    QHash<QString, QList<SubtuneEntry>> stilDatabase_;

    // BUGlist database: HVSC path -> bug entries
    QHash<QString, QList<BugEntry>> buglistDatabase_;
};

#endif // HVSCMETADATASERVICE_H
