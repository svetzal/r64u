#ifndef TRANSFERQUEUE_H
#define TRANSFERQUEUE_H

#include <QAbstractListModel>
#include <QQueue>
#include <QString>
#include <QSet>

#include "services/ftpentry.h"  // For FtpEntry definition (needed by Qt MOC)

class C64UFtpClient;

enum class OperationType { Upload, Download, Delete };

enum class OverwriteResponse { Overwrite, OverwriteAll, Skip, Cancel };

struct TransferItem {
    enum class Status { Pending, InProgress, Completed, Failed };

    QString localPath;   // Empty for delete operations
    QString remotePath;
    OperationType operationType;
    Status status = Status::Pending;
    qint64 bytesTransferred = 0;
    qint64 totalBytes = 0;
    QString errorMessage;
    bool isDirectory = false;  // For delete operations
};

class TransferQueue : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        LocalPathRole = Qt::UserRole + 1,
        RemotePathRole,
        OperationTypeRole,
        StatusRole,
        ProgressRole,
        BytesTransferredRole,
        TotalBytesRole,
        ErrorMessageRole,
        FileNameRole
    };

    explicit TransferQueue(QObject *parent = nullptr);

    void setFtpClient(C64UFtpClient *client);

    // Queue operations
    void enqueueUpload(const QString &localPath, const QString &remotePath);
    void enqueueDownload(const QString &remotePath, const QString &localPath);

    // Recursive operations
    void enqueueRecursiveUpload(const QString &localDir, const QString &remoteDir);
    void enqueueRecursiveDownload(const QString &remoteDir, const QString &localDir);

    // Delete operations
    void enqueueDelete(const QString &remotePath, bool isDirectory);
    void enqueueRecursiveDelete(const QString &remotePath);

    void clear();
    void removeCompleted();
    void cancelAll();

    [[nodiscard]] int pendingCount() const;
    [[nodiscard]] int activeCount() const;
    [[nodiscard]] bool isProcessing() const { return processing_; }
    [[nodiscard]] bool isProcessingDelete() const { return processingDelete_; }
    [[nodiscard]] bool isScanning() const { return !pendingScans_.isEmpty() || !pendingDeleteScans_.isEmpty(); }
    [[nodiscard]] bool isScanningForDelete() const { return !pendingDeleteScans_.isEmpty(); }
    [[nodiscard]] int deleteProgress() const { return totalDeleteItems_ > 0 ? deletedCount_ : 0; }
    [[nodiscard]] int deleteTotalCount() const { return totalDeleteItems_; }

    // QAbstractListModel interface
    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    // Overwrite handling
    void respondToOverwrite(OverwriteResponse response);
    void setAutoOverwrite(bool autoOverwrite) { overwriteAll_ = autoOverwrite; }

signals:
    void operationStarted(const QString &fileName, OperationType type);
    void operationCompleted(const QString &fileName);
    void operationFailed(const QString &fileName, const QString &error);
    void allOperationsCompleted();
    void operationsCancelled();
    void queueChanged();
    void deleteProgressUpdate(const QString &fileName, int current, int total);
    void overwriteConfirmationNeeded(const QString &fileName, OperationType type);

private slots:
    void onUploadProgress(const QString &file, qint64 sent, qint64 total);
    void onUploadFinished(const QString &localPath, const QString &remotePath);
    void onDownloadProgress(const QString &file, qint64 received, qint64 total);
    void onDownloadFinished(const QString &remotePath, const QString &localPath);
    void onFtpError(const QString &message);
    void onDirectoryCreated(const QString &path);
    void onDirectoryListed(const QString &path, const QList<FtpEntry> &entries);
    void onFileRemoved(const QString &path);

private:
    void processNext();
    [[nodiscard]] int findItemIndex(const QString &localPath, const QString &remotePath) const;
    void processRecursiveUpload(const QString &localDir, const QString &remoteDir);
    void processPendingDirectoryCreation();
    void processNextDelete();
    void onDirectoryListedForDelete(const QString &path, const QList<FtpEntry> &entries);

    C64UFtpClient *ftpClient_ = nullptr;
    QList<TransferItem> items_;
    bool processing_ = false;
    int currentIndex_ = -1;

    // Recursive download state
    struct PendingScan {
        QString remotePath;
        QString localBasePath;
    };
    QQueue<PendingScan> pendingScans_;
    QSet<QString> requestedListings_;  // Track paths we've explicitly requested
    QString recursiveLocalBase_;
    QString recursiveRemoteBase_;
    bool scanningDirectories_ = false;  // True while scanning dirs for recursive download

    // Recursive upload state
    struct PendingMkdir {
        QString remotePath;
        QString localDir;
        QString remoteBase;
    };
    QQueue<PendingMkdir> pendingMkdirs_;
    bool creatingDirectory_ = false;

    // Recursive delete state
    struct PendingDeleteScan {
        QString remotePath;
    };
    QQueue<PendingDeleteScan> pendingDeleteScans_;
    QSet<QString> requestedDeleteListings_;  // Track paths we've requested for delete scanning

    struct DeleteItem {
        QString path;
        bool isDirectory;
    };
    QList<DeleteItem> deleteQueue_;  // Ordered: files first, then dirs deepest-first
    int currentDeleteIndex_ = 0;
    int totalDeleteItems_ = 0;
    int deletedCount_ = 0;
    bool processingDelete_ = false;

    // Overwrite confirmation state
    bool waitingForOverwriteResponse_ = false;
    bool overwriteAll_ = false;
};

#endif // TRANSFERQUEUE_H
