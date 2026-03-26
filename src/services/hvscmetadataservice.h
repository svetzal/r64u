/**
 * @file hvscmetadataservice.h
 * @brief Service for managing HVSC STIL and BUGlist metadata.
 *
 * Downloads, caches, and queries the HVSC STIL.txt and BUGlist.txt files
 * to provide tune commentary, cover information, and bug warnings.
 */

#ifndef HVSCMETADATASERVICE_H
#define HVSCMETADATASERVICE_H

#include "hvscparser.h"
#include "ifiledownloader.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

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
    static constexpr const char *StilUrl =
        "https://www.hvsc.c64.org/download/C64Music/DOCUMENTS/STIL.txt";

    /// URL to download BUGlist.txt
    static constexpr const char *BuglistUrl =
        "https://www.hvsc.c64.org/download/C64Music/DOCUMENTS/BUGlist.txt";

    // Type aliases for backward compatibility — types are defined in hvscparser.h
    using CoverInfo = hvsc::CoverInfo;
    using SubtuneEntry = hvsc::SubtuneEntry;
    using StilInfo = hvsc::StilInfo;
    using BugEntry = hvsc::BugEntry;
    using BugInfo = hvsc::BugInfo;

    explicit HVSCMetadataService(IFileDownloader *stilDownloader,
                                 IFileDownloader *buglistDownloader, QObject *parent = nullptr);
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
    void onStilDownloaderProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onStilDownloaderFinished(const QByteArray &data);
    void onStilDownloaderFailed(const QString &error);

    void onBuglistDownloaderProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onBuglistDownloaderFinished(const QByteArray &data);
    void onBuglistDownloaderFailed(const QString &error);

private:
    bool parseStil(const QByteArray &data);
    bool parseStilFile(const QString &filePath);
    bool parseBuglist(const QByteArray &data);
    bool parseBuglistFile(const QString &filePath);

    IFileDownloader *stilDownloader_ = nullptr;
    IFileDownloader *buglistDownloader_ = nullptr;

    // STIL database: HVSC path -> entries
    QHash<QString, QList<hvsc::SubtuneEntry>> stilDatabase_;

    // BUGlist database: HVSC path -> bug entries
    QHash<QString, QList<hvsc::BugEntry>> buglistDatabase_;
};

#endif  // HVSCMETADATASERVICE_H
