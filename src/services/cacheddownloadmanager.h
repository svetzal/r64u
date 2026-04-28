#pragma once

#include "ifiledownloader.h"

#include <QObject>
#include <QString>
#include <QUrl>

#include <functional>

/**
 * @brief Manages the download-cache-parse lifecycle for a single file.
 *
 * Wraps IFileDownloader to provide: cache path resolution, guard against
 * concurrent downloads, save-to-cache on success, parse callback invocation,
 * and signal forwarding.
 *
 * On construction, if a cached file exists, the parse callback is invoked
 * immediately and loaded() is emitted.
 *
 * The ParseCallback receives the raw bytes and returns an entry count (>= 0)
 * on success or -1 on failure.
 */
class CachedDownloadManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Callback invoked after data is received (from network or cache).
     *
     * The callback must parse @p data, update whatever state it manages, and
     * return the number of entries parsed (>= 0) on success or -1 on failure.
     */
    using ParseCallback = std::function<int(const QByteArray &data)>;

    /**
     * @brief Constructs the manager and wires downloader signals.
     *
     * If a cached file already exists the parse callback is invoked
     * immediately; on success loaded() is emitted.
     *
     * @param downloader    Injected downloader gateway (not owned).
     * @param cacheFilename Filename stored under QStandardPaths::AppDataLocation.
     * @param downloadUrl   URL to fetch when download() is called.
     * @param parseCallback Called with raw bytes; returns entry count or -1.
     * @param parent        Qt parent object.
     */
    explicit CachedDownloadManager(IFileDownloader *downloader, const QString &cacheFilename,
                                   const QUrl &downloadUrl, ParseCallback parseCallback,
                                   QObject *parent = nullptr);

    ~CachedDownloadManager() override = default;

    /**
     * @brief Returns the full path where the cached file is stored.
     *
     * Creates the parent directory if it does not yet exist.
     */
    [[nodiscard]] QString cacheFilePath() const;

    /**
     * @brief Returns true if the cached file exists on disk.
     */
    [[nodiscard]] bool hasCachedFile() const;

    /**
     * @brief Starts a download from the configured URL.
     *
     * A no-op if a download is already in progress.
     */
    void download();

    /**
     * @brief Cancels the current download, if any.
     */
    void cancelDownload();

signals:
    /** @brief Forwarded from the underlying IFileDownloader. */
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

    /**
     * @brief Emitted after a successful download + parse.
     * @param entryCount Number of entries returned by the parse callback.
     */
    void downloadFinished(int entryCount);

    /** @brief Emitted when a download or parse step fails. */
    void downloadFailed(const QString &error);

    /** @brief Emitted whenever data has been parsed successfully (download or cache). */
    void loaded();

private slots:
    void onDownloaderProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloaderFinished(const QByteArray &data);
    void onDownloaderFailed(const QString &error);

private:
    /** @brief Reads @p filePath, invokes the parse callback, emits loaded() on success. */
    bool loadFromFile(const QString &filePath);

    IFileDownloader *downloader_;
    QString cacheFilename_;
    QUrl downloadUrl_;
    ParseCallback parseCallback_;
};
