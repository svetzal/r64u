#ifndef TRANSFERORCHESTRATOR_H
#define TRANSFERORCHESTRATOR_H

#include "batchmanager.h"
#include "transfereventprocessor.h"
#include "transferftphandler.h"
#include "transfertimeoutmanager.h"

#include "ftp/folderoperationcoordinator.h"
#include "ftp/recursivescancoordinator.h"
#include "ftp/remotedirectorycreator.h"
#include "services/ftpentry.h"
#include "services/iftpclient.h"
#include "services/ilocalfilesystem.h"
#include "services/transfercore.h"

#include <QObject>
#include <QPointer>
#include <QString>

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

/**
 * @brief Orchestrates transfer queue state, FTP dispatch, and folder operations.
 *
 * TransferOrchestrator manages the lifecycle of file transfers: enqueuing uploads,
 * downloads, and deletes; coordinating recursive folder scans and directory creation;
 * handling overwrite and folder-exists confirmations; and delegating actual FTP I/O
 * to TransferFtpHandler. Model notifications are routed back to the owning
 * QAbstractListModel via the ModelCallbacks struct so the orchestrator itself has
 * no dependency on Qt's model infrastructure.
 */
class TransferOrchestrator : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Callbacks the owning model registers so the orchestrator can drive
     *        QAbstractListModel notifications without depending on it directly.
     */
    struct ModelCallbacks
    {
        std::function<void(int first, int last)> beginInsertRows;
        std::function<void()> endInsertRows;
        std::function<void(int row)> dataChangedRow;
        std::function<void()> beginResetModel;
        std::function<void()> endResetModel;
        std::function<void(int first, int last)> beginRemoveRows;
        std::function<void()> endRemoveRows;
    };

    explicit TransferOrchestrator(QObject *parent = nullptr);
    ~TransferOrchestrator() override;

    void setModelCallbacks(const ModelCallbacks &callbacks);

    void setFtpClient(IFtpClient *client);
    void setLocalFileSystem(ILocalFileSystem *fs);

    void enqueueUpload(const QString &localPath, const QString &remotePath, int targetBatchId = -1);
    void enqueueDownload(const QString &remotePath, const QString &localPath,
                         int targetBatchId = -1);
    void enqueueRecursiveUpload(const QString &localDir, const QString &remoteDir);
    void enqueueRecursiveDownload(const QString &remoteDir, const QString &localDir);
    void enqueueDelete(const QString &remotePath, bool isDirectory);
    void enqueueRecursiveDelete(const QString &remotePath);

    void clear();
    void cancelAll();
    void cancelBatch(int batchId);

    void respondToOverwrite(OverwriteResponse response);
    void setAutoOverwrite(bool autoOverwrite) { state_.overwriteAll = autoOverwrite; }
    void respondToFolderExists(FolderExistsResponse response);
    void setAutoMerge(bool autoMerge) { state_.autoMerge = autoMerge; }

    void flushEventQueue();

    [[nodiscard]] const transfer::State &state() const { return state_; }

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

    [[nodiscard]] BatchProgress activeBatchProgress() const;
    [[nodiscard]] BatchProgress batchProgress(int batchId) const;
    [[nodiscard]] QList<int> allBatchIds() const;
    [[nodiscard]] bool hasActiveBatch() const;
    [[nodiscard]] int queuedBatchCount() const;

    [[nodiscard]] bool isPathBeingTransferred(const QString &path, OperationType type) const;

    [[nodiscard]] TransferBatch *findBatch(int batchId);
    [[nodiscard]] const TransferBatch *findBatch(int batchId) const;

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

    void batchStarted(int batchId);
    void batchProgressUpdate(int batchId, int completed, int total);
    void batchCompleted(int batchId);

    void statusMessage(const QString &message, int timeout);

    void scanningStarted(const QString &folderName, OperationType type);
    void scanningProgress(int directoriesScanned, int directoriesRemaining, int filesDiscovered);
    void directoryCreationProgress(int created, int total);

private slots:
    void onOperationTimeout();

private:
    void processNext();
    void scheduleProcessNext();
    void transitionTo(QueueState newState);

    void startOperationTimeout();
    void stopOperationTimeout();

    [[nodiscard]] int findBatchIndex(int batchId) const;
    void activateAndSchedule(int batchIdx);

    int createBatch(OperationType type, const QString &description, const QString &folderName,
                    const QString &sourcePath = QString());
    void activateNextBatch();
    void completeBatch(int batchId);
    void purgeBatch(int batchId);
    [[nodiscard]] TransferBatch *activeBatch();
    [[nodiscard]] int findItemIndex(const QString &localPath, const QString &remotePath) const;
    void emitBatchProgressAndComplete(int batchId, bool batchIsComplete,
                                      bool includeFailed = false);

    void finishDirectoryCreation();
    void processNextDelete();
    void markCurrentComplete(TransferItem::Status status);

    QPointer<IFtpClient> ftpClient_;
    ILocalFileSystem *localFs_ = nullptr;

    transfer::State state_;

    ModelCallbacks modelCallbacks_;

    BatchManager *batchManager_ = nullptr;
    TransferTimeoutManager *timeoutManager_ = nullptr;
    TransferEventProcessor *eventProcessor_ = nullptr;

    RecursiveScanCoordinator *scanCoordinator_ = nullptr;
    RemoteDirectoryCreator *dirCreator_ = nullptr;
    FolderOperationCoordinator *folderCoordinator_ = nullptr;
    TransferFtpHandler *ftpHandler_ = nullptr;
};

#endif  // TRANSFERORCHESTRATOR_H
