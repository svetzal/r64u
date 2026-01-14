#ifndef TRANSFERQUEUE_H
#define TRANSFERQUEUE_H

#include <QAbstractListModel>
#include <QPointer>
#include <QQueue>
#include <QString>
#include <QSet>
#include <QTimer>
#include <functional>

#include "services/ftpentry.h"  // For FtpEntry definition (needed by Qt MOC)
#include "services/iftpclient.h"  // Full include needed for QPointer

enum class OperationType { Upload, Download, Delete };

enum class OverwriteResponse { Overwrite, OverwriteAll, Skip, Cancel };

enum class FolderExistsResponse { Merge, Replace, Cancel };

/**
 * @brief State machine states for TransferQueue.
 *
 * This enum represents the high-level state of the transfer queue.
 * It runs parallel to the boolean flags during the transition period,
 * with assertions validating consistency.
 */
enum class QueueState {
    Idle,                    ///< No operations - ready for new work
    Scanning,                ///< Scanning remote directories for recursive ops
    CreatingDirectories,     ///< Creating directories for upload
    CheckingExists,          ///< Checking file/folder existence before transfer
    AwaitingConfirmation,    ///< Waiting for user response (overwrite/folder exists)
    Transferring,            ///< Active file transfer in progress
    Deleting,                ///< Active delete operation in progress
};

/// @brief Convert QueueState to string for debugging
[[nodiscard]] inline const char* queueStateToString(QueueState state) {
    switch (state) {
        case QueueState::Idle: return "Idle";
        case QueueState::Scanning: return "Scanning";
        case QueueState::CreatingDirectories: return "CreatingDirectories";
        case QueueState::CheckingExists: return "CheckingExists";
        case QueueState::AwaitingConfirmation: return "AwaitingConfirmation";
        case QueueState::Transferring: return "Transferring";
        case QueueState::Deleting: return "Deleting";
    }
    return "Unknown";
}

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
    bool overwriteConfirmed = false;  // User confirmed overwrite for this file
    int batchId = -1;  // Links item to its parent batch
};

/**
 * @brief A batch groups related transfer items from a single user action.
 *
 * When the user clicks "Download" with 5 files selected, those 5 files
 * form one batch. When the batch completes, it's purged from the queue.
 */
struct TransferBatch {
    int batchId = 0;
    QString description;
    OperationType operationType = OperationType::Download;
    QString sourcePath;  // Root path being operated on (for duplicate detection)
    QList<TransferItem> items;

    // Progress tracking
    int completedCount = 0;
    int failedCount = 0;

    // Batch state
    bool isActive = false;

    // Computed properties
    [[nodiscard]] int totalCount() const { return items.size(); }
    [[nodiscard]] int pendingCount() const { return totalCount() - completedCount - failedCount; }
    [[nodiscard]] bool isComplete() const { return pendingCount() == 0; }
};

/**
 * @brief Progress information for the active batch.
 */
struct BatchProgress {
    int batchId = -1;
    QString description;
    OperationType operationType = OperationType::Download;
    int totalItems = 0;
    int completedItems = 0;
    int failedItems = 0;
    bool isScanning = false;
    bool isCreatingDirectories = false;
    bool isProcessingDelete = false;
    int deleteProgress = 0;
    int deleteTotalCount = 0;

    // Scanning progress details
    QString scanningFolder;           ///< Name of folder being scanned
    int directoriesScanned = 0;       ///< Number of directories scanned so far
    int directoriesRemaining = 0;     ///< Number of directories left to scan
    int filesDiscovered = 0;          ///< Number of files found during scanning

    // Directory creation progress (for uploads)
    int directoriesCreated = 0;       ///< Number of directories created so far
    int directoriesToCreate = 0;      ///< Total directories to create

    [[nodiscard]] bool isValid() const { return batchId >= 0; }
    [[nodiscard]] int pendingItems() const { return totalItems - completedItems - failedItems; }
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
    ~TransferQueue() override;

    void setFtpClient(IFtpClient *client);

    // Queue operations
    void enqueueUpload(const QString &localPath, const QString &remotePath, int targetBatchId = -1);
    void enqueueDownload(const QString &remotePath, const QString &localPath, int targetBatchId = -1);

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
    [[nodiscard]] bool isProcessing() const { return state_ == QueueState::Transferring || processing_; }
    [[nodiscard]] bool isProcessingDelete() const { return state_ == QueueState::Deleting; }
    [[nodiscard]] bool isScanning() const { return state_ == QueueState::Scanning || !pendingScans_.isEmpty() || !pendingDeleteScans_.isEmpty(); }
    [[nodiscard]] bool isScanningForDelete() const { return !pendingDeleteScans_.isEmpty(); }
    [[nodiscard]] bool isCreatingDirectories() const { return state_ == QueueState::CreatingDirectories || !pendingMkdirs_.isEmpty(); }
    [[nodiscard]] int deleteProgress() const { return totalDeleteItems_ > 0 ? deletedCount_ : 0; }
    [[nodiscard]] int deleteTotalCount() const { return totalDeleteItems_; }

    // Batch operations
    [[nodiscard]] BatchProgress activeBatchProgress() const;
    [[nodiscard]] BatchProgress batchProgress(int batchId) const;
    [[nodiscard]] QList<int> allBatchIds() const;
    [[nodiscard]] bool hasActiveBatch() const;
    [[nodiscard]] int queuedBatchCount() const;

    // Duplicate detection - checks if a path already has an active/pending operation
    [[nodiscard]] bool isPathBeingTransferred(const QString &path, OperationType type) const;

    // QAbstractListModel interface
    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    // Overwrite handling
    void respondToOverwrite(OverwriteResponse response);
    void setAutoOverwrite(bool autoOverwrite) { overwriteAll_ = autoOverwrite; }

    // Folder exists handling
    void respondToFolderExists(FolderExistsResponse response);
    void setAutoMerge(bool autoMerge) { autoMerge_ = autoMerge; }

    // For testing: immediately process all pending events
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

    // Status messages (for user feedback)
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

private:
    void processNext();
    void scheduleProcessNext();  // Defers processNext() to prevent re-entrancy
    void processEventQueue();    // Processes pending events
    [[nodiscard]] int findItemIndex(const QString &localPath, const QString &remotePath) const;
    void processRecursiveUpload(const QString &localDir, const QString &remoteDir);
    void processPendingDirectoryCreation();
    void processNextDelete();
    void onDirectoryListedForDelete(const QString &path, const QList<FtpEntry> &entries);
    void onDirectoryListedForFolderCheck(const QString &path, const QList<FtpEntry> &entries);
    void onDirectoryListedForUploadCheck(const QString &path, const QList<FtpEntry> &entries);
    void startRecursiveUpload();  // Actually starts the upload after confirmation
    void startNextFolderUpload();  // Start uploading the next folder from foldersToUpload_
    void onFolderUploadComplete();  // Called when a folder upload finishes

    // Batch management helpers
    int createBatch(OperationType type, const QString &description, const QString &sourcePath = QString());
    void activateNextBatch();
    void completeBatch(int batchId);
    void purgeBatch(int batchId);
    [[nodiscard]] TransferBatch* findBatch(int batchId);
    [[nodiscard]] const TransferBatch* findBatch(int batchId) const;

    QPointer<IFtpClient> ftpClient_;
    QList<TransferItem> items_;
    bool processing_ = false;
    int currentIndex_ = -1;

    // Batch management
    QList<TransferBatch> batches_;
    int nextBatchId_ = 1;
    int activeBatchIndex_ = -1;  // Index into batches_, -1 if no active batch

    // Recursive download state
    struct PendingScan {
        QString remotePath;
        QString localBasePath;
        int batchId = -1;  // Links scan to its batch (for multi-batch support)
    };
    QQueue<PendingScan> pendingScans_;
    QSet<QString> requestedListings_;  // Track paths we've explicitly requested
    QString recursiveLocalBase_;
    QString recursiveRemoteBase_;
    bool scanningDirectories_ = false;  // True while scanning dirs for recursive download

    // Scanning progress tracking
    QString scanningFolderName_;       // Name of folder being scanned (for display)
    int directoriesScanned_ = 0;       // Count of directories processed
    int filesDiscovered_ = 0;          // Count of files found during scan

    // Recursive upload state
    struct PendingMkdir {
        QString remotePath;
        QString localDir;
        QString remoteBase;
    };
    QQueue<PendingMkdir> pendingMkdirs_;
    bool creatingDirectory_ = false;
    QSet<QString> requestedFolderCheckListings_;  // Track paths for folder existence check
    int directoriesCreated_ = 0;       // Count of directories created during upload
    int totalDirectoriesToCreate_ = 0; // Total directories to create for current upload

    // Recursive delete state
    struct PendingDeleteScan {
        QString remotePath;
    };
    QQueue<PendingDeleteScan> pendingDeleteScans_;
    QSet<QString> requestedDeleteListings_;  // Track paths we've requested for delete scanning
    QString recursiveDeleteBase_;  // Base path for recursive delete (for duplicate detection)

    struct DeleteItem {
        QString path;
        bool isDirectory;
    };
    QList<DeleteItem> deleteQueue_;  // Ordered: files first, then dirs deepest-first
    int currentDeleteIndex_ = 0;
    int totalDeleteItems_ = 0;
    int deletedCount_ = 0;
    bool processingDelete_ = false;

    // Confirmation context - consolidates what confirmation we're waiting for
    struct ConfirmationContext {
        enum Type { None, FileOverwrite, FolderExists };
        Type type = None;
        OperationType operationType = OperationType::Download;
        int itemIndex = -1;  // For file overwrites, the index of the item being confirmed

        void clear() {
            type = None;
            operationType = OperationType::Download;
            itemIndex = -1;
        }
        [[nodiscard]] bool isActive() const { return type != None; }
    };
    ConfirmationContext pendingConfirmation_;

    // Legacy flags - kept during transition period
    bool waitingForOverwriteResponse_ = false;
    bool overwriteAll_ = false;

    // Upload file existence check state
    bool checkingUploadFileExists_ = false;
    QSet<QString> requestedUploadFileCheckListings_;  // Track paths we've requested for upload file checks

    // Folder exists confirmation state
    struct PendingFolderUpload {
        QString localDir;
        QString remoteDir;
        QString targetDir;  // The actual target path (remoteDir + folderName)
        bool exists = false;  // Set after checking if folder exists on remote
        int batchId = -1;  // Batch ID for this folder upload (assigned when upload starts)
    };
    QQueue<PendingFolderUpload> pendingFolderUploads_;  // Queue of folders waiting to check/upload
    QList<PendingFolderUpload> foldersToUpload_;  // Folders ready to upload (after confirmation)
    PendingFolderUpload currentFolderUpload_;  // The folder currently being uploaded
    bool folderUploadInProgress_ = false;  // True while a folder is being uploaded
    bool replaceExistingFolders_ = false;  // True if user chose Replace (delete before upload)
    bool checkingFolderExists_ = false;
    bool waitingForFolderExistsResponse_ = false;
    bool autoMerge_ = false;

    // Compound operation state (delete + upload for Replace)
    struct CompoundOperation {
        enum Phase { None, Deleting, Uploading };
        Phase phase = None;

        void clear() { phase = None; }
        [[nodiscard]] bool isActive() const { return phase != None; }
    };
    CompoundOperation compoundOp_;

    // Legacy flag - kept during transition period
    bool pendingUploadAfterDelete_ = false;

    // Operation timeout for stalled transfers
    QTimer *operationTimeoutTimer_ = nullptr;
    static constexpr int OperationTimeoutMs = 300000;  // 5 minutes
    void startOperationTimeout();
    void stopOperationTimeout();
    void onOperationTimeout();

    // Event queue for deferred processing (prevents re-entrancy)
    QQueue<std::function<void()>> eventQueue_;
    bool processingEvents_ = false;  // Re-entrancy guard
    bool eventProcessingScheduled_ = false;  // Prevents multiple timer posts

    // State machine (runs parallel to boolean flags during transition)
    QueueState state_ = QueueState::Idle;
    void transitionTo(QueueState newState);
    void validateStateConsistency() const;  // Debug: asserts state matches flags
};

#endif // TRANSFERQUEUE_H
