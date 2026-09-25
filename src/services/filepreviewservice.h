#ifndef FILEPREVIEWSERVICE_H
#define FILEPREVIEWSERVICE_H

#include "ierroremitter.h"
#include "iftpclient.h"  // Full include needed for QPointer

#include <QByteArray>
#include <QPointer>
#include <QString>

/**
 * @brief Service for downloading file content for preview purposes.
 *
 * FilePreviewService encapsulates the download-preview workflow, decoupling
 * UI widgets from direct FTP client signal handling. Benefits include:
 * - UI widgets can be tested in isolation
 * - Preview logic is centralized
 * - Easier to add caching or retry logic in the future
 *
 * @par Example usage:
 * @code
 * FilePreviewService *preview = new FilePreviewService(ftpClient, this);
 *
 * connect(preview, &FilePreviewService::previewReady,
 *         this, &MyWidget::onPreviewReady);
 * connect(preview, &FilePreviewService::previewFailed,
 *         this, &MyWidget::onPreviewFailed);
 *
 * preview->requestPreview("/path/to/file.txt");
 * @endcode
 */
class FilePreviewService : public IErrorEmitter
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a file preview service.
     * @param ftpClient The FTP client to use for downloads (not owned).
     * @param parent Optional parent QObject for memory management.
     */
    explicit FilePreviewService(IFtpClient *ftpClient, QObject *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~FilePreviewService() override;

    /**
     * @brief Requests a file preview.
     * @param remotePath Path of the file to preview.
     *
     * Downloads the file content to memory. Emits previewReady() on success
     * or previewFailed() on failure.
     */
    void requestPreview(const QString &remotePath);

    /**
     * @brief Cancels any pending preview request.
     *
     * Aborts the download only if it is the one in flight on the shared client,
     * never another component's transfer.
     */
    void cancelRequest();

    /**
     * @brief Returns the path of the currently pending request, if any.
     * @return The pending path, or empty string if no request is pending.
     */
    [[nodiscard]] QString pendingPath() const { return pendingPath_; }

    /**
     * @brief Checks if a preview request is in progress.
     * @return True if a request is pending.
     */
    [[nodiscard]] bool isLoading() const { return !pendingPath_.isEmpty(); }

signals:
    /**
     * @brief Emitted when preview content is successfully downloaded.
     * @param path The remote file path.
     * @param data The file contents.
     */
    void previewReady(const QString &path, const QByteArray &data);

    /**
     * @brief Emitted when a preview request fails.
     * @param path The remote file path.
     * @param error Human-readable error description.
     */
    void previewFailed(const QString &path, const QString &error);

    /**
     * @brief Emitted when a preview request is started.
     * @param path The remote file path being loaded.
     */
    void previewStarted(const QString &path);

private slots:
    void onDownloadToMemoryFinished(const QString &remotePath, const QByteArray &data);
    void onFtpOperationFailed(IFtpClient::Operation operation, const QString &remotePath,
                              const QString &localPath, const QString &message);
    void onFtpError(const QString &message);
    void onFtpDisconnected();

private:
    /// Ends the pending request, if any, with previewFailed(@p message).
    void failPendingPreview(const QString &message);

    QPointer<IFtpClient> ftpClient_;
    QString pendingPath_;
};

#endif  // FILEPREVIEWSERVICE_H
