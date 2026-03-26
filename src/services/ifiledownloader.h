#pragma once

#include <QObject>
#include <QUrl>

/**
 * @brief Gateway interface for HTTP file download operations.
 *
 * Abstracts single-file HTTP downloads behind a testable interface.
 * Only one download is active at a time; calling download() while a
 * download is in progress is a no-op.
 *
 * All implementations must emit exactly one of downloadFinished() or
 * downloadFailed() for every download() call.
 */
class IFileDownloader : public QObject
{
    Q_OBJECT

public:
    explicit IFileDownloader(QObject *parent = nullptr);
    ~IFileDownloader() override = default;

    /**
     * @brief Begin downloading from the given URL.
     *
     * If a download is already in progress this call is silently ignored.
     * @param url The URL to download from.
     */
    virtual void download(const QUrl &url) = 0;

    /**
     * @brief Cancel the current download, if any.
     *
     * Has no effect if no download is in progress.
     */
    virtual void cancel() = 0;

    /**
     * @brief Returns true if a download is currently in progress.
     */
    [[nodiscard]] virtual bool isDownloading() const = 0;

signals:
    /** @brief Emitted periodically as download data arrives. */
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

    /** @brief Emitted when the download completes successfully with all received bytes. */
    void downloadFinished(const QByteArray &data);

    /** @brief Emitted when the download fails with a human-readable error message. */
    void downloadFailed(const QString &error);
};
