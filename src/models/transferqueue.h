#ifndef TRANSFERQUEUE_H
#define TRANSFERQUEUE_H

#include "services/ftpentry.h"
#include "services/iftpclient.h"
#include "services/transfercore.h"

#include <QAbstractListModel>
#include <QPointer>
#include <QQueue>
#include <QSet>
#include <QString>
#include <QTimer>

#include <functional>

using transfer::BatchProgress;
using transfer::ConfirmationContext;
using transfer::DeleteItem;
using transfer::FolderExistsResponse;
using transfer::OperationType;
using transfer::OverwriteResponse;
using transfer::PendingFolderOp;
using transfer::PendingMkdir;
using transfer::PendingScan;
using transfer::QueueState;
using transfer::queueStateToString;
using transfer::TransferBatch;
using transfer::TransferItem;

class TransferQueue : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        LocalPathRole = static_cast<int>(transfer::ItemRole::LocalPath),
        RemotePathRole = static_cast<int>(transfer::ItemRole::RemotePath),
        OperationTypeRole = static_cast<int>(transfer::ItemRole::OperationType),
        StatusRole = static_cast<int>(transfer::ItemRole::Status),
        ProgressRole = static_cast<int>(transfer::ItemRole::Progress),
        BytesTransferredRole = static_cast<int>(transfer::ItemRole::BytesTransferred),
        TotalBytesRole = static_cast<int>(transfer::ItemRole::TotalBytes),
        ErrorMessageRole = static_cast<int>(transfer::ItemRole::ErrorMessage),
        FileNameRole = static_cast<int>(transfer::ItemRole::FileName)
    };

    explicit TransferQueue(QObject *parent = nullptr);
    ~TransferQueue() override;

    void setFtpClient(IFtpClient *client);

    // Queue operations - public API remains the same
    void enqueueUpload(const QString &localPath, const QString &remotePath, int targetBatchId = -1);
    void enqueueDownload(const QString &remotePath, const QString &localPath,
                         int targetBatchId = -1);

    // Recursive operations
    void enqueueRecursiveUpload(const QString &localDir, const QString &remoteDir);
    void enqueueRecursiveDownload(const QString &remoteDir, const QString &localDir);

    // Delete operations
    void enqueueDelete(const QString &remotePath, bool isDirectory);
    void enqueueRecursiveDelete(const QString &remotePath);

    void clear();
    void removeCompleted();
    void cancelAll();
    void cancelBatch(int batchId);

    [[nodiscard]] int pendingCount() const;
    [[nodiscard]] int activeCount() const;
    [[nodiscard]] int activeAndPendingCount() const;
    [[nodiscard]] bool isProcessing() const
    {
        return state_.queueState == QueueState::Transferring;
    }
    [[nodiscard]] bool isProcessingDelete() const
    {
        return state_.queueState == QueueState::Deleting;
    }
    [[nodiscard]] bool isScanning() const { return state_.queueState == QueueState::Scanning; }
    [[nodiscard]] bool isScanningForDelete() const;
    [[nodiscard]] bool isCreatingDirectories() const
    {
        return state_.queueState == QueueState::CreatingDirectories;
    }
    [[nodiscard]] int deleteProgress() const { return state_.deletedCount; }
    [[nodiscard]] int deleteTotalCount() const { return state_.deleteQueue.size(); }

    // Batch operations
    [[nodiscard]] BatchProgress activeBatchProgress() const;
    [[nodiscard]] BatchProgress batchProgress(int batchId) const;
    [[nodiscard]] QList<int> allBatchIds() const;
    [[nodiscard]] bool hasActiveBatch() const;
    [[nodiscard]] int queuedBatchCount() const;

    // Duplicate detection
    [[nodiscard]] bool isPathBeingTransferred(const QString &path, OperationType type) const;

    // QAbstractListModel interface
    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index,
                                int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    // Confirmation handling
    void respondToOverwrite(OverwriteResponse response);
    void setAutoOverwrite(bool autoOverwrite) { state_.overwriteAll = autoOverwrite; }

    void respondToFolderExists(FolderExistsResponse response);
    void setAutoMerge(bool autoMerge) { state_.autoMerge = autoMerge; }

    // For testing
    void flushEventQueue();

signals:
    void operationStarted(const QString &fileName, OperationType type);
    void operationCompleted(const QString &fileName);
    void operationFailed(const QString &fileName, const QString &error);
    void allOperationsCompleted();
    void operationsCancelled();
    void queueChanged();
    void deleteProgressUpdate(const QString &fileName, int current, int total);
    void overwriteConfirmationNeeded(const QString &fileName, OperationType type);
    void folderExistsConfirmationNeeded(const QStringList &folderNames);

    // Batch signals
    void batchStarted(int batchId);
    void batchProgressUpdate(int batchId, int completed, int total);
    void batchCompleted(int batchId);

    // Status messages
    void statusMessage(const QString &message, int timeout);

    // Scanning progress signals
    void scanningStarted(const QString &folderName, OperationType type);
    void scanningProgress(int directoriesScanned, int directoriesRemaining, int filesDiscovered);
    void directoryCreationProgress(int created, int total);

private slots:
    void onUploadProgress(const QString &file, qint64 sent, qint64 total);
    void onUploadFinished(const QString &localPath, const QString &remotePath);
    void onDownloadProgress(const QString &file, qint64 received, qint64 total);
    void onDownloadFinished(const QString &remotePath, const QString &localPath);
    void onFtpError(const QString &message);
    void onDirectoryCreated(const QString &path);
    void onDirectoryListed(const QString &path, const QList<FtpEntry> &entries);
    void onFileRemoved(const QString &path);
    void onDebounceTimeout();

private:
    // Core processing - single entry point
    void processNext();
    void scheduleProcessNext();
    void processEventQueue();

    // State machine
    void transitionTo(QueueState newState);

    // Folder operations (unified for upload/download)
    void startFolderOperation(const PendingFolderOp &op);
    void onFolderOperationComplete();
    void checkFolderConfirmation();

    // Scanning
    void startScan(const QString &remotePath, const QString &localBase, const QString &remoteBase,
                   int batchId);
    void handleDirectoryListing(const QString &path, const QList<FtpEntry> &entries);
    void handleDirectoryListingForDelete(const QString &path, const QList<FtpEntry> &entries);
    void handleDirectoryListingForFolderCheck(const QString &path, const QList<FtpEntry> &entries);
    void handleDirectoryListingForUploadCheck(const QString &path, const QList<FtpEntry> &entries);
    void finishScanning();

    // Directory creation (uploads)
    void createNextDirectory();
    void queueDirectoriesForUpload(const QString &localDir, const QString &remoteDir);
    void finishDirectoryCreation();

    // File operations
    void startNextTransfer();
    void markCurrentComplete(TransferItem::Status status);
    void processNextDelete();

    // Batch management
    int createBatch(OperationType type, const QString &description, const QString &folderName,
                    const QString &sourcePath = QString());
    void activateNextBatch();
    void completeBatch(int batchId);
    void purgeBatch(int batchId);
    [[nodiscard]] TransferBatch *findBatch(int batchId);
    [[nodiscard]] const TransferBatch *findBatch(int batchId) const;
    [[nodiscard]] TransferBatch *activeBatch();
    [[nodiscard]] int findItemIndex(const QString &localPath, const QString &remotePath) const;

    // Batch progress helper
    void emitBatchProgressAndComplete(int batchId, bool batchIsComplete,
                                      bool includeFailed = false);

    // Recursive operation shared logic
    void enqueueRecursiveOperation(OperationType type, const QString &sourcePath,
                                   const QString &destPath);

    // Timeout handling
    QTimer *operationTimeoutTimer_ = nullptr;
    static constexpr int OperationTimeoutMs = 300000;  // 5 minutes
    void startOperationTimeout();
    void stopOperationTimeout();
    void onOperationTimeout();

    // I/O boundary members (not part of pure state)
    QPointer<IFtpClient> ftpClient_;

    // Debounce timer for collecting items
    QTimer *debounceTimer_ = nullptr;
    static constexpr int DebounceMs = 50;

    // Event queue for deferred processing
    QQueue<std::function<void()>> eventQueue_;
    bool processingEvents_ = false;
    bool eventProcessingScheduled_ = false;

    // All mutable queue state (pure-function compatible)
    transfer::State state_;
};

#endif  // TRANSFERQUEUE_H
