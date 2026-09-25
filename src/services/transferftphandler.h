#ifndef TRANSFERFTPHANDLER_H
#define TRANSFERFTPHANDLER_H

#include "ftp/ftpentry.h"
#include "services/transferhandlerbase.h"

class TransferTimeoutManager;
class RemoteDirectoryCoordinator;
class RecursiveScanCoordinator;

/// Owns the FTP client signal connections and translates raw FTP events into
/// higher-level transfer-queue events.  TransferQueue connects to these signals
/// to update the model and drive queue processing.
///
/// The FTP client is shared with other components (remote browser, previews,
/// playlists, config loader), so every event is first matched against the
/// request the queue is waiting for; events for other requests are ignored.
class TransferFtpHandler : public TransferHandlerBase
{
    Q_OBJECT

public:
    explicit TransferFtpHandler(transfer::State &state, QObject *parent = nullptr);

    void setTimeoutManager(TransferTimeoutManager *manager);
    void setDirCreator(RemoteDirectoryCoordinator *creator);
    void setScanCoordinator(RecursiveScanCoordinator *coordinator);

signals:
    void operationCompleted(const QString &fileName);
    void deleteProgressUpdate(const QString &fileName, int current, int total);
    void processNextDeleteRequested();
    void completeBatchRequested(int batchId);

protected:
    void connectFtpSignals() override;

private slots:
    void onUploadProgress(const QString &file, qint64 sent, qint64 total);
    void onUploadFinished(const QString &localPath, const QString &remotePath);
    void onDownloadProgress(const QString &file, qint64 received, qint64 total);
    void onDownloadFinished(const QString &remotePath, const QString &localPath);
    void onFtpOperationFailed(IFtpClient::Operation operation, const QString &remotePath,
                              const QString &localPath, const QString &message);
    void onFtpDirectoryCreated(const QString &path);
    void onDirectoryListed(const QString &path, const QList<FtpEntry> &entries);
    void onFileRemoved(const QString &path);

private:
    /// True if the failed FTP request is one the transfer queue issued and still awaits.
    [[nodiscard]] bool isQueueRequest(IFtpClient::Operation operation, const QString &remotePath,
                                      const QString &localPath) const;
    /// Applies a failure of the queue's own request to the queue state.
    void handleQueueRequestFailure(const QString &message);
    void startTimeout();
    void stopTimeout();

    TransferTimeoutManager *timeoutManager_ = nullptr;
    RemoteDirectoryCoordinator *dirCreator_ = nullptr;
    RecursiveScanCoordinator *scanCoordinator_ = nullptr;
};

#endif  // TRANSFERFTPHANDLER_H
