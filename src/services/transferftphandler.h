#ifndef TRANSFERFTPHANDLER_H
#define TRANSFERFTPHANDLER_H

#include "ftp/ftpentry.h"
#include "services/transferhandlerbase.h"

class TransferTimeoutManager;
class RemoteDirectoryCoordinator;
class RecursiveScanCoordinator;

/// Owns the 8 FTP client signal connections and translates raw FTP events into
/// higher-level transfer-queue events.  TransferQueue connects to these signals
/// to update the model and drive queue processing.
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
    void onFtpError(const QString &message);
    void onFtpDirectoryCreated(const QString &path);
    void onDirectoryListed(const QString &path, const QList<FtpEntry> &entries);
    void onFileRemoved(const QString &path);

private:
    void startTimeout();
    void stopTimeout();

    TransferTimeoutManager *timeoutManager_ = nullptr;
    RemoteDirectoryCoordinator *dirCreator_ = nullptr;
    RecursiveScanCoordinator *scanCoordinator_ = nullptr;
};

#endif  // TRANSFERFTPHANDLER_H
