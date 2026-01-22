#ifndef TRANSFERQUEUE_H
#define TRANSFERQUEUE_H

#include <QAbstractListModel>
#include <QPointer>
#include <QQueue>
#include <QString>
#include <QSet>
#include <QTimer>
#include <functional>

#include "services/ftpentry.h"
#include "services/iftpclient.h"

enum class OperationType { Upload, Download, Delete };

enum class OverwriteResponse { Overwrite, OverwriteAll, Skip, Cancel };

enum class FolderExistsResponse { Merge, Replace, Cancel };

/**
 * @brief State machine states for TransferQueue.
 *
 * This enum is the SINGLE source of truth for queue state.
 * No parallel boolean flags - the state machine controls everything.
 */
enum class QueueState {
    Idle,                    ///< No operations - ready for new work
    CollectingItems,         ///< Debouncing user selections (50ms)
    AwaitingFolderConfirm,   ///< Waiting for Merge/Replace dialog
    Scanning,                ///< Scanning directories for recursive ops
    AwaitingFileConfirm,     ///< Waiting for Overwrite/Skip dialog
    CreatingDirectories,     ///< Creating remote dirs (upload only)
    Transferring,            ///< Active file transfer in progress
    Deleting,                ///< Active delete operation in progress
    BatchComplete            ///< Batch finished, transitioning to next
};

/// @brief Convert QueueState to string for debugging
[[nodiscard]] inline const char* queueStateToString(QueueState state) {
    switch (state) {
        case QueueState::Idle: return "Idle";
        case QueueState::CollectingItems: return "CollectingItems";
        case QueueState::AwaitingFolderConfirm: return "AwaitingFolderConfirm";
        case QueueState::Scanning: return "Scanning";
        case QueueState::AwaitingFileConfirm: return "AwaitingFileConfirm";
        case QueueState::CreatingDirectories: return "CreatingDirectories";
        case QueueState::Transferring: return "Transferring";
        case QueueState::Deleting: return "Deleting";
        case QueueState::BatchComplete: return "BatchComplete";
    }
    return "Unknown";
}

struct TransferItem {
    enum class Status { Pending, InProgress, Completed, Failed, Skipped };

    QString localPath;   // Empty for delete operations
    QString remotePath;
    OperationType operationType;
    Status status = Status::Pending;
    qint64 bytesTransferred = 0;
    qint64 totalBytes = 0;
    QString errorMessage;
    bool isDirectory = false;  // For delete operations
    bool needsConfirmation = false;  // True if destination exists
    bool confirmed = false;  // User said yes to overwrite
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
    QString folderName;  // Top-level folder name for progress display
    OperationType operationType = OperationType::Download;
    QString sourcePath;  // Root path being operated on (for duplicate detection)
    QList<TransferItem> items;

    // Progress tracking
    int completedCount = 0;
    int failedCount = 0;

    // Batch lifecycle
    bool scanned = false;           // True when all items discovered
    bool folderConfirmed = false;   // True when user confirmed folder action (or not needed)

    // Computed properties
    [[nodiscard]] int totalCount() const { return items.size(); }
    [[nodiscard]] int pendingCount() const { return totalCount() - completedCount - failedCount; }
    [[nodiscard]] bool isComplete() const { return scanned && folderConfirmed && pendingCount() == 0; }
};

/**
 * @brief Progress information for the active batch.
 */
struct BatchProgress {
    int batchId = -1;
    QString description;
    QString folderName;  // Top-level folder name for progress display
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

/**
 * @brief Pending folder operation awaiting confirmation or processing.
 *
 * Unified structure for both uploads and downloads to simplify code paths.
 */
struct PendingFolderOp {
    OperationType operationType;
    QString sourcePath;      // Local for upload, remote for download
    QString destPath;        // Remote for upload, local for download
    QString targetPath;      // Full destination path (destPath + folderName)
    bool destExists = false; // True if destination folder exists
    int batchId = -1;
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

    // Queue operations - public API remains the same
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
    [[nodiscard]] bool isProcessing() const { return state_ == QueueState::Transferring; }
    [[nodiscard]] bool isProcessingDelete() const { return state_ == QueueState::Deleting; }
    [[nodiscard]] bool isScanning() const { return state_ == QueueState::Scanning; }
    [[nodiscard]] bool isScanningForDelete() const;
    [[nodiscard]] bool isCreatingDirectories() const { return state_ == QueueState::CreatingDirectories; }
    [[nodiscard]] int deleteProgress() const { return deletedCount_; }
    [[nodiscard]] int deleteTotalCount() const { return deleteQueue_.size(); }

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
    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    // Confirmation handling
    void respondToOverwrite(OverwriteResponse response);
    void setAutoOverwrite(bool autoOverwrite) { overwriteAll_ = autoOverwrite; }

    void respondToFolderExists(FolderExistsResponse response);
    void setAutoMerge(bool autoMerge) { autoMerge_ = autoMerge; }

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
    QueueState state_ = QueueState::Idle;
    void transitionTo(QueueState newState);

    // Folder operations (unified for upload/download)
    void startFolderOperation(const PendingFolderOp &op);
    void onFolderOperationComplete();
    void checkFolderConfirmation();

    // Scanning
    void startScan(const QString &remotePath, const QString &localBase, const QString &remoteBase, int batchId);
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
    int createBatch(OperationType type, const QString &description, const QString &folderName, const QString &sourcePath = QString());
    void activateNextBatch();
    void completeBatch(int batchId);
    void purgeBatch(int batchId);
    [[nodiscard]] TransferBatch* findBatch(int batchId);
    [[nodiscard]] const TransferBatch* findBatch(int batchId) const;
    [[nodiscard]] TransferBatch* activeBatch();
    [[nodiscard]] int findItemIndex(const QString &localPath, const QString &remotePath) const;

    // Timeout handling
    QTimer *operationTimeoutTimer_ = nullptr;
    static constexpr int OperationTimeoutMs = 300000;  // 5 minutes
    void startOperationTimeout();
    void stopOperationTimeout();
    void onOperationTimeout();

    // Core data
    QPointer<IFtpClient> ftpClient_;
    QList<TransferItem> items_;
    int currentIndex_ = -1;

    // Batch management
    QList<TransferBatch> batches_;
    int nextBatchId_ = 1;
    int activeBatchIndex_ = -1;

    // Debounce timer for collecting items
    QTimer *debounceTimer_ = nullptr;
    static constexpr int DebounceMs = 50;

    // Pending folder operations (collected during debounce)
    QQueue<PendingFolderOp> pendingFolderOps_;
    PendingFolderOp currentFolderOp_;

    // Scanning state
    struct PendingScan {
        QString remotePath;
        QString localBasePath;
        QString remoteBasePath;
        int batchId = -1;
    };
    QQueue<PendingScan> pendingScans_;
    QSet<QString> requestedListings_;
    QString scanningFolderName_;
    int directoriesScanned_ = 0;
    int filesDiscovered_ = 0;

    // Directory creation state
    struct PendingMkdir {
        QString remotePath;
        QString localDir;
        QString remoteBase;
    };
    QQueue<PendingMkdir> pendingMkdirs_;
    int directoriesCreated_ = 0;
    int totalDirectoriesToCreate_ = 0;

    // Delete operation state
    struct DeleteItem {
        QString path;
        bool isDirectory;
    };
    QList<DeleteItem> deleteQueue_;
    QQueue<PendingScan> pendingDeleteScans_;
    QSet<QString> requestedDeleteListings_;
    QString recursiveDeleteBase_;
    int deletedCount_ = 0;

    // Confirmation state
    struct ConfirmationContext {
        int itemIndex = -1;          // For file overwrites
        QStringList folderNames;     // For folder exists dialog
        OperationType opType = OperationType::Download;

        void clear() {
            itemIndex = -1;
            folderNames.clear();
            opType = OperationType::Download;
        }
    };
    ConfirmationContext pendingConfirmation_;

    // User preferences (preserved across batches)
    bool overwriteAll_ = false;
    bool autoMerge_ = false;
    bool replaceExisting_ = false;

    // Compound operation state (delete + upload for Replace)
    bool pendingUploadAfterDelete_ = false;

    // Event queue for deferred processing
    QQueue<std::function<void()>> eventQueue_;
    bool processingEvents_ = false;
    bool eventProcessingScheduled_ = false;

    // Upload file existence checking
    QSet<QString> requestedUploadFileCheckListings_;
    QSet<QString> requestedFolderCheckListings_;
};

#endif // TRANSFERQUEUE_H
