#ifndef TRANSFERFTPHANDLER_H
#define TRANSFERFTPHANDLER_H

#include "services/ftpentry.h"
#include "services/iftpclient.h"
#include "services/transfercore.h"

#include <QObject>
#include <QPointer>

class TransferTimeoutManager;
class RemoteDirectoryCreator;
class RecursiveScanCoordinator;

/// Owns the 8 FTP client signal connections and translates raw FTP events into
/// higher-level transfer-queue events.  TransferQueue connects to these signals
/// to update the model and drive queue processing.
class TransferFtpHandler : public QObject
{
    Q_OBJECT

public:
    explicit TransferFtpHandler(transfer::State &state, QObject *parent = nullptr);

    void setFtpClient(IFtpClient *client);
    void setTimeoutManager(TransferTimeoutManager *manager);
    void setDirCreator(RemoteDirectoryCreator *creator);
    void setScanCoordinator(RecursiveScanCoordinator *coordinator);

signals:
    /// Row whose display data changed; receiver should emit dataChanged(index(row), index(row)).
    void itemDataChanged(int row);
    void operationCompleted(const QString &fileName);
    void operationFailed(const QString &fileName, const QString &error);
    void queueChanged();
    void deleteProgressUpdate(const QString &fileName, int current, int total);
    void scheduleProcessNextRequested();
    void processNextDeleteRequested();
    void completeBatchRequested(int batchId);
    void batchProgressRequested(int batchId, bool isComplete, bool includeFailed);

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
    void markCurrentComplete(transfer::TransferItem::Status status);
    void startTimeout();
    void stopTimeout();

    transfer::State &state_;
    QPointer<IFtpClient> ftpClient_;
    TransferTimeoutManager *timeoutManager_ = nullptr;
    RemoteDirectoryCreator *dirCreator_ = nullptr;
    RecursiveScanCoordinator *scanCoordinator_ = nullptr;
};

#endif  // TRANSFERFTPHANDLER_H
