#ifndef TRANSFERQUEUE_H
#define TRANSFERQUEUE_H

#include "transferorchestrator.h"

#include "services/transfercore.h"

#include <QAbstractListModel>
#include <QString>

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
    ~TransferQueue() override = default;

    void setFtpClient(IFtpClient *client);

    void setLocalFileSystem(ILocalFileSystem *fs);

    // Queue operations
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
    [[nodiscard]] bool isProcessing() const { return orchestrator_->isProcessing(); }
    [[nodiscard]] bool isProcessingDelete() const { return orchestrator_->isProcessingDelete(); }
    [[nodiscard]] bool isScanning() const { return orchestrator_->isScanning(); }
    [[nodiscard]] bool isScanningForDelete() const;
    [[nodiscard]] bool isCreatingDirectories() const
    {
        return orchestrator_->isCreatingDirectories();
    }
    [[nodiscard]] int deleteProgress() const { return orchestrator_->deleteProgress(); }
    [[nodiscard]] int deleteTotalCount() const { return orchestrator_->deleteTotalCount(); }

    // Batch operations (delegated to orchestrator)
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
    void setAutoOverwrite(bool autoOverwrite) { orchestrator_->setAutoOverwrite(autoOverwrite); }

    void respondToFolderExists(FolderExistsResponse response);
    void setAutoMerge(bool autoMerge) { orchestrator_->setAutoMerge(autoMerge); }

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

private:
    TransferOrchestrator *orchestrator_ = nullptr;
};

#endif  // TRANSFERQUEUE_H
