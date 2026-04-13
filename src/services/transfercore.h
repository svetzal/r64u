/**
 * @file transfercore.h
 * @brief Pure core functions for transfer queue management logic.
 *
 * All functions in this namespace are pure: they take immutable input and return
 * new output with no side effects. This enables comprehensive unit testing and
 * clean separation from I/O concerns (FTP operations, Qt model signals, timers).
 *
 * The central type is \c transfer::State, which captures the complete mutable
 * state of the transfer queue. TransferQueue stores a single \c state_ member
 * of this type and delegates all business logic to functions in this namespace.
 *
 * ## Architecture
 *
 * - **Pure core** (this file): State transitions, item management, batch lifecycle,
 *   confirmation handling, directory scanning, error handling
 * - **Imperative shell** (TransferQueue): FTP calls, Qt model signals, timers,
 *   debounce, I/O operations
 */

#ifndef TRANSFERCORE_H
#define TRANSFERCORE_H

#include "ftpentry.h"

#include <QAbstractItemModel>
#include <QList>
#include <QPair>
#include <QQueue>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <functional>

namespace transfer {

// ---------------------------------------------------------------------------
// Enumerations
// ---------------------------------------------------------------------------

/// @brief Classifies the direction and kind of a transfer operation.
enum class OperationType { Upload, Download, Delete };

/// @brief User's response to a file-overwrite confirmation dialog.
enum class OverwriteResponse { Overwrite, OverwriteAll, Skip, Cancel };

/// @brief User's response to a folder-exists confirmation dialog.
enum class FolderExistsResponse { Merge, Replace, Cancel };

/**
 * @brief State machine states for the transfer queue.
 *
 * This enum is the SINGLE source of truth for queue state.
 * No parallel boolean flags — the state machine controls everything.
 */
enum class QueueState {
    Idle,                   ///< No operations — ready for new work
    CollectingItems,        ///< Debouncing user selections (50 ms)
    AwaitingFolderConfirm,  ///< Waiting for Merge/Replace dialog
    Scanning,               ///< Scanning directories for recursive ops
    AwaitingFileConfirm,    ///< Waiting for Overwrite/Skip dialog
    CreatingDirectories,    ///< Creating remote dirs (upload only)
    Transferring,           ///< Active file transfer in progress
    Deleting,               ///< Active delete operation in progress
    BatchComplete           ///< Batch finished, transitioning to next
};

/// @brief Convert QueueState to a human-readable string for debugging.
[[nodiscard]] inline const char *queueStateToString(QueueState state)
{
    switch (state) {
    case QueueState::Idle:
        return "Idle";
    case QueueState::CollectingItems:
        return "CollectingItems";
    case QueueState::AwaitingFolderConfirm:
        return "AwaitingFolderConfirm";
    case QueueState::Scanning:
        return "Scanning";
    case QueueState::AwaitingFileConfirm:
        return "AwaitingFileConfirm";
    case QueueState::CreatingDirectories:
        return "CreatingDirectories";
    case QueueState::Transferring:
        return "Transferring";
    case QueueState::Deleting:
        return "Deleting";
    case QueueState::BatchComplete:
        return "BatchComplete";
    }
    return "Unknown";
}

// ---------------------------------------------------------------------------
// Transfer item
// ---------------------------------------------------------------------------

/// @brief Represents a single file or directory in the transfer queue.
struct TransferItem
{
    enum class Status { Pending, InProgress, Completed, Failed, Skipped };

    QString localPath;  ///< Empty for delete operations
    QString remotePath;
    OperationType operationType = OperationType::Download;
    Status status = Status::Pending;
    qint64 bytesTransferred = 0;
    qint64 totalBytes = 0;
    QString errorMessage;
    bool isDirectory = false;        ///< For delete operations
    bool needsConfirmation = false;  ///< True if destination exists
    bool confirmed = false;          ///< User said yes to overwrite
    int batchId = -1;                ///< Links item to its parent batch
};

// ---------------------------------------------------------------------------
// Batch
// ---------------------------------------------------------------------------

/**
 * @brief A batch groups related transfer items from a single user action.
 *
 * When the user clicks "Download" with 5 files selected, those 5 files
 * form one batch. When the batch completes, it is purged from the queue.
 */
struct TransferBatch
{
    int batchId = 0;
    QString description;
    QString folderName;  ///< Top-level folder name for progress display
    OperationType operationType = OperationType::Download;
    QString sourcePath;  ///< Root path being operated on (for duplicate detection)
    QList<TransferItem> items;

    // Progress tracking
    int completedCount = 0;
    int failedCount = 0;

    // Batch lifecycle
    bool scanned = false;          ///< True when all items discovered
    bool folderConfirmed = false;  ///< True when user confirmed folder action (or not needed)

    // Computed properties
    [[nodiscard]] int totalCount() const { return items.size(); }
    [[nodiscard]] int pendingCount() const { return totalCount() - completedCount - failedCount; }
    [[nodiscard]] bool isComplete() const
    {
        return scanned && folderConfirmed && pendingCount() == 0;
    }
};

// ---------------------------------------------------------------------------
// Progress snapshot
// ---------------------------------------------------------------------------

/// @brief Progress information for the active batch, used by the UI.
struct BatchProgress
{
    int batchId = -1;
    QString description;
    QString folderName;  ///< Top-level folder name for progress display
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
    QString scanningFolder;        ///< Name of folder being scanned
    int directoriesScanned = 0;    ///< Number of directories scanned so far
    int directoriesRemaining = 0;  ///< Number of directories left to scan
    int filesDiscovered = 0;       ///< Number of files found during scanning

    // Directory creation progress (for uploads)
    int directoriesCreated = 0;   ///< Number of directories created so far
    int directoriesToCreate = 0;  ///< Total directories to create

    [[nodiscard]] bool isValid() const { return batchId >= 0; }
    [[nodiscard]] int pendingItems() const { return totalItems - completedItems - failedItems; }
};

// ---------------------------------------------------------------------------
// Folder operation
// ---------------------------------------------------------------------------

/**
 * @brief Pending folder operation awaiting confirmation or processing.
 *
 * Unified structure for both uploads and downloads to simplify code paths.
 */
struct PendingFolderOp
{
    OperationType operationType = OperationType::Download;
    QString sourcePath;       ///< Local for upload, remote for download
    QString destPath;         ///< Remote for upload, local for download
    QString targetPath;       ///< Full destination path (destPath + folderName)
    bool destExists = false;  ///< True if destination folder exists
    int batchId = -1;
};

// ---------------------------------------------------------------------------
// Internal operation structs (promoted from TransferQueue private scope)
// ---------------------------------------------------------------------------

/// @brief A directory that still needs to be scanned for recursive operations.
struct PendingScan
{
    QString remotePath;
    QString localBasePath;
    QString remoteBasePath;
    int batchId = -1;
};

/// @brief A remote directory that needs to be created during an upload.
struct PendingMkdir
{
    QString remotePath;
    QString localDir;
    QString remoteBase;
};

/// @brief A file or directory queued for deletion.
struct DeleteItem
{
    QString path;
    bool isDirectory = false;
};

/// @brief Context saved while waiting for the user to respond to a confirmation dialog.
struct ConfirmationContext
{
    int itemIndex = -1;       ///< For file overwrites
    QStringList folderNames;  ///< For folder-exists dialog
    OperationType opType = OperationType::Download;

    void clear()
    {
        itemIndex = -1;
        folderNames.clear();
        opType = OperationType::Download;
    }
};

// ---------------------------------------------------------------------------
// Aggregate mutable state
// ---------------------------------------------------------------------------

/**
 * @brief All mutable state owned by the transfer queue.
 *
 * Capturing the full state in one struct enables pure functions to compute
 * transitions without touching I/O or Qt signals. Phase 1 only defines the
 * struct; migration of the member variables in TransferQueue happens later.
 */
struct State
{
    // Core item tracking
    QList<TransferItem> items;
    int currentIndex = -1;

    // State machine
    QueueState queueState = QueueState::Idle;

    // Batch management
    QList<TransferBatch> batches;
    int nextBatchId = 1;
    int activeBatchIndex = -1;

    // User preferences
    bool overwriteAll = false;
    bool autoMerge = false;
    bool replaceExisting = false;

    // Folder operations
    QQueue<PendingFolderOp> pendingFolderOps;
    PendingFolderOp currentFolderOp;
    bool pendingUploadAfterDelete = false;

    // Confirmation state
    ConfirmationContext pendingConfirmation;

    // Scanning state (download)
    QQueue<PendingScan> pendingScans;
    QSet<QString> requestedListings;
    QString scanningFolderName;
    int directoriesScanned = 0;
    int filesDiscovered = 0;

    // Directory creation state (upload)
    QQueue<PendingMkdir> pendingMkdirs;
    int directoriesCreated = 0;
    int totalDirectoriesToCreate = 0;

    // Delete operation state
    QList<DeleteItem> deleteQueue;
    QQueue<PendingScan> pendingDeleteScans;
    QSet<QString> requestedDeleteListings;
    QString recursiveDeleteBase;
    int deletedCount = 0;

    // Upload file existence checking
    QSet<QString> requestedUploadFileCheckListings;
    QSet<QString> requestedFolderCheckListings;
};

// ---------------------------------------------------------------------------
// Qt model roles
// ---------------------------------------------------------------------------

/// @brief Qt model roles for TransferItem data.
enum class ItemRole {
    LocalPath = Qt::UserRole + 1,
    RemotePath,
    OperationType,
    Status,
    Progress,
    BytesTransferred,
    TotalBytes,
    ErrorMessage,
    FileName
};

// ---------------------------------------------------------------------------
// Path utilities
// ---------------------------------------------------------------------------

/// @brief Strips trailing slashes from a path, preserving the root "/".
/// @param path Input path string.
/// @return Normalized path without trailing slashes.
[[nodiscard]] QString normalizePath(const QString &path);

// ---------------------------------------------------------------------------
// Item counting (read-only queries)
// ---------------------------------------------------------------------------

/// @brief Count items with Pending status.
[[nodiscard]] int pendingCount(const State &state);

/// @brief Count items with InProgress status.
[[nodiscard]] int activeCount(const State &state);

/// @brief Count items that are Pending or InProgress.
[[nodiscard]] int activeAndPendingCount(const State &state);

// ---------------------------------------------------------------------------
// Batch queries
// ---------------------------------------------------------------------------

/// @brief Count incomplete batches (not yet isComplete()).
[[nodiscard]] int queuedBatchCount(const State &state);

/// @brief Returns all batch IDs for incomplete batches.
[[nodiscard]] QList<int> allBatchIds(const State &state);

/// @brief Returns true if activeBatchIndex refers to a valid batch.
[[nodiscard]] bool hasActiveBatch(const State &state);

// ---------------------------------------------------------------------------
// Lookup functions
// ---------------------------------------------------------------------------

/// @brief Find a batch index by batchId. Returns -1 if not found.
[[nodiscard]] int findBatchIndex(const State &state, int batchId);

/// @brief Find an item index by local and remote path. Returns -1 if not found.
[[nodiscard]] int findItemIndex(const State &state, const QString &localPath,
                                const QString &remotePath);

/// @brief Find the index of the next pending item. Returns -1 if none.
[[nodiscard]] int findNextPendingItem(const State &state);

// ---------------------------------------------------------------------------
// Duplicate detection
// ---------------------------------------------------------------------------

/// @brief Returns true if a path is already being transferred.
/// Checks active batches (by sourcePath), pending scans, and pending folder ops.
/// @param path Path to check (will be normalized internally).
/// @param type Operation type to match.
[[nodiscard]] bool isPathBeingTransferred(const State &state, const QString &path,
                                          OperationType type);

// ---------------------------------------------------------------------------
// State machine gate
// ---------------------------------------------------------------------------

/// @brief Returns true if the state machine allows processNext() to run.
/// Only QueueState::Idle allows processing.
[[nodiscard]] bool canProcessNext(QueueState queueState);

// ---------------------------------------------------------------------------
// Progress computation
// ---------------------------------------------------------------------------

/// @brief Compute BatchProgress for the active batch (activeBatchIndex).
/// Returns invalid BatchProgress (batchId == -1) if no active batch.
[[nodiscard]] BatchProgress computeActiveBatchProgress(const State &state);

/// @brief Compute BatchProgress for a specific batch by ID.
/// Returns invalid BatchProgress (batchId == -1) if not found.
[[nodiscard]] BatchProgress computeBatchProgress(const State &state, int batchId);

// ---------------------------------------------------------------------------
// Model data
// ---------------------------------------------------------------------------

/// @brief Extract display data for a transfer item at the given Qt model role.
/// Used by QAbstractListModel::data().
[[nodiscard]] QVariant itemData(const TransferItem &item, int role);

// ---------------------------------------------------------------------------
// State mutation result structs
// ---------------------------------------------------------------------------

/// @brief Result of creating a new batch.
struct CreateBatchResult
{
    State newState;
    int batchId = -1;  ///< ID of the newly created batch
};

/// @brief Result of completing a batch.
struct CompleteBatchResult
{
    State newState;
    int batchId = -1;
    bool isFolderOperation = false;          ///< True if batch belongs to currentFolderOp
    bool hasRemainingActiveBatches = false;  ///< True if there are more incomplete batches
};

/// @brief Result of cancelling a specific batch.
struct CancelBatchResult
{
    State newState;
    bool wasActiveBatch = false;  ///< True if the cancelled batch was the active one
};

/// @brief Result of marking an item complete.
struct MarkCompleteResult
{
    State newState;
    int batchId = -1;
    bool batchIsComplete = false;  ///< True if this was the last item in its batch
};

// ---------------------------------------------------------------------------
// State mutation functions
// ---------------------------------------------------------------------------

/// @brief Purge a completed batch and all its items from state.
/// Adjusts currentIndex and activeBatchIndex as needed.
/// @note Does NOT emit model signals — caller must call beginRemoveRows/endRemoveRows.
[[nodiscard]] State purgeBatch(const State &state, int batchId);

/// @brief Plan for purging a batch: which item indices to remove (descending) and the batch index.
struct PurgeBatchPlan
{
    QList<int> itemIndicesToRemove;  ///< Indices in descending order — safe for sequential removal
    int batchIndex = -1;             ///< Index of the batch in batches list, -1 if not found
};

/// @brief Compute which items to remove for a batch purge, without modifying state.
/// Returns indices in descending order so the caller can remove them one-by-one with Qt model
/// signals interleaved, without invalidating earlier indices.
[[nodiscard]] PurgeBatchPlan planBatchPurge(const State &state, int batchId);

/// @brief Create a new batch (purging completed batches first).
/// @param type Operation type for the batch.
/// @param description Human-readable description.
/// @param folderName Top-level folder name for display.
/// @param sourcePath Root path (for duplicate detection). May be empty.
[[nodiscard]] CreateBatchResult createBatch(const State &state, OperationType type,
                                            const QString &description, const QString &folderName,
                                            const QString &sourcePath = QString());

/// @brief Find the next incomplete batch with pending items and mark it active.
/// Sets activeBatchIndex = -1 if none found.
[[nodiscard]] State activateNextBatch(const State &state);

/// @brief Complete a batch: reset active batch index, transition to Idle, reset currentIndex.
/// Does NOT call onFolderOperationComplete — returns metadata for caller.
[[nodiscard]] CompleteBatchResult completeBatch(const State &state, int batchId);

/// @brief Mark an item as complete/failed/skipped and update its batch counters.
/// Also resets currentIndex to -1.
[[nodiscard]] MarkCompleteResult markItemComplete(const State &state, int itemIndex,
                                                  TransferItem::Status status,
                                                  const QString &errorMessage = QString());

/// @brief Clear all state — items, batches, scanning state, preferences.
/// Equivalent to a full reset.
[[nodiscard]] State clearAll(const State &state);

/// @brief Remove Completed/Failed/Skipped items from the item list.
/// Adjusts currentIndex to stay valid.
[[nodiscard]] State removeCompleted(const State &state);

/// @brief Mark all Pending and InProgress items as Failed("Cancelled").
/// Clears batches, scanning state, folder ops, and preferences.
[[nodiscard]] State cancelAllItems(const State &state);

/// @brief Mark a specific batch's Pending/InProgress items as Failed("Cancelled").
/// If the batch was the active batch, transitions to Idle and resets scanning state.
[[nodiscard]] CancelBatchResult cancelBatch(const State &state, int batchId);

// ---------------------------------------------------------------------------
// Phase 4: Confirmation result structs
// ---------------------------------------------------------------------------

/// @brief Result of processing an overwrite response.
struct OverwriteResult
{
    State newState;
    bool shouldScheduleProcessNext = false;
    bool shouldCancelAll = false;
};

/// @brief Result of processing a folder-exists response.
struct FolderExistsResult
{
    State newState;
    bool shouldStartFolderOp = false;
    PendingFolderOp folderOpToStart;  ///< Valid when shouldStartFolderOp is true
    bool shouldCancelFolderOps = false;
};

/// @brief Result of checking folder confirmation requirements.
struct FolderConfirmResult
{
    State newState;
    bool needsConfirmation = false;
    QStringList existingFolderNames;  ///< Non-empty when needsConfirmation is true
    bool shouldStartFolderOp = false;
    PendingFolderOp folderOpToStart;  ///< Valid when shouldStartFolderOp is true
};

/// @brief Process an overwrite confirmation response.
/// Precondition: state.queueState == QueueState::AwaitingFileConfirm.
/// Returns unchanged state if precondition not met.
[[nodiscard]] OverwriteResult respondToOverwrite(const State &state, OverwriteResponse response);

/// @brief Process a folder-exists confirmation response.
/// Precondition: state.queueState == QueueState::AwaitingFolderConfirm.
/// Returns unchanged state if precondition not met.
[[nodiscard]] FolderExistsResult respondToFolderExists(const State &state,
                                                       FolderExistsResponse response);

/// @brief Check pending folder ops and decide if confirmation is needed.
/// Returns FolderConfirmResult describing what the shell should do next.
[[nodiscard]] FolderConfirmResult checkFolderConfirmation(const State &state);

// ---------------------------------------------------------------------------
// Phase 5: Scanning and directory listing helpers
// ---------------------------------------------------------------------------

/// @brief Sort a delete queue: files first, then directories deepest-first.
[[nodiscard]] QList<DeleteItem> sortDeleteQueue(const QList<DeleteItem> &queue);

/// @brief Result of processing a directory listing for download scanning.
struct DirectoryListingResult
{
    QList<PendingScan> newSubScans;                   ///< Subdirectories to scan next
    QList<QPair<QString, QString>> newFileDownloads;  ///< (remotePath, localPath) pairs
    int directoriesScanned = 0;
};

/// @brief Process a FTP directory listing for download scanning.
/// Produces sub-scans for subdirectories and download file pairs.
/// @note Does NOT create local directories — caller must call QDir().mkpath()
[[nodiscard]] DirectoryListingResult
processDirectoryListingForDownload(const PendingScan &currentScan, const QList<FtpEntry> &entries);

/// @brief Result of processing a directory listing for delete scanning.
struct DeleteListingResult
{
    QList<PendingScan> newSubScans;  ///< Subdirectories to scan for deletion
    QList<DeleteItem> fileItems;     ///< Files discovered (to delete)
    DeleteItem directoryItem;        ///< The directory itself (delete after contents)
};

/// @brief Process a FTP directory listing for recursive delete scanning.
[[nodiscard]] DeleteListingResult processDirectoryListingForDelete(const QString &path,
                                                                   const QList<FtpEntry> &entries);

/// @brief Update pending folder ops' destExists flags from a remote directory listing.
/// @param parentPath The remote directory that was listed.
/// @param entries The entries in that directory.
[[nodiscard]] State updateFolderExistence(const State &state, const QString &parentPath,
                                          const QList<FtpEntry> &entries);

/// @brief Result of checking whether an upload's target file already exists.
struct UploadFileCheckResult
{
    State newState;
    bool fileExists = false;
    QString fileName;
};

/// @brief Check if the current upload item's remote target file exists in a directory listing.
[[nodiscard]] UploadFileCheckResult checkUploadFileExists(const State &state,
                                                          const QList<FtpEntry> &entries);

// ---------------------------------------------------------------------------
// Phase 7: processNext decision logic
// ---------------------------------------------------------------------------

/// @brief Describes what action TransferQueue::processNext() should take.
enum class ProcessNextAction {
    Blocked,                      ///< State machine disallows processing
    NoFtpClient,                  ///< FTP client not connected
    StartFolderOp,                ///< Dequeue and start a pending folder op
    NeedOverwriteCheck_Download,  ///< Local file exists, ask user
    NeedOverwriteCheck_Upload,    ///< Check remote file existence first
    StartTransfer,                ///< Start the next pending transfer
    NoPending                     ///< No pending items remain
};

/// @brief Decision produced by decideNextAction().
struct ProcessNextDecision
{
    ProcessNextAction action = ProcessNextAction::Blocked;
    int itemIndex = -1;               ///< For StartTransfer/NeedOverwriteCheck
    PendingFolderOp folderOpToStart;  ///< For StartFolderOp
    QString uploadCheckDir;           ///< Remote dir to LIST for NeedOverwriteCheck_Upload
    QString fileNameForSignal;        ///< File name for operationStarted signal
};

/// @brief Decide what processNext() should do next, given current state.
/// @param state Current transfer queue state.
/// @param ftpConnected Whether the FTP client is connected and ready.
/// @param localFileExists Predicate to check if a local file exists (injected to keep pure).
[[nodiscard]] ProcessNextDecision
decideNextAction(const State &state, bool ftpConnected,
                 const std::function<bool(const QString &)> &localFileExists);

// ---------------------------------------------------------------------------
// Phase 7: FTP error result
// ---------------------------------------------------------------------------

/// @brief Result of handling an FTP error.
struct FtpErrorResult
{
    State newState;
    bool isDeleteError = false;            ///< Error during recursive delete
    bool shouldProcessNextDelete = false;  ///< True when delete error should continue to next item
    QString deleteFileName;                ///< For operationFailed signal (delete)
    bool hasCurrentItem = false;           ///< Error during file transfer
    QString transferFileName;              ///< For operationFailed signal (transfer)
    int failedBatchId = -1;
    bool batchIsComplete = false;
    bool shouldScheduleProcessNext = false;
    bool isFolderCreationError = false;  ///< Error during directory creation
    QString folderName;                  ///< For operationFailed signal (folder)
    int folderBatchId = -1;
};

/// @brief Handle an FTP error, returning the new state and metadata for signal emission.
/// @param state Current transfer queue state.
/// @param message Error message from the FTP client.
[[nodiscard]] FtpErrorResult handleFtpError(const State &state, const QString &message);

// ---------------------------------------------------------------------------
// Phase 7: Operation timeout
// ---------------------------------------------------------------------------

/// @brief Handle an operation timeout — mark any InProgress item as Failed.
/// @param state Current transfer queue state.
/// @return Updated state with the timed-out item marked Failed and queue transitioned to Idle.
[[nodiscard]] State handleOperationTimeout(const State &state);

// ---------------------------------------------------------------------------
// FTP handler helpers
// ---------------------------------------------------------------------------

/// @brief Result of transitioning queue state to Idle after a transfer completes.
struct CompleteTransferResult
{
    State newState;
    bool transitionedToIdle = false;  ///< True if queueState changed from Transferring to Idle
};

/// @brief Transition queue state to Idle after a file transfer completes.
/// Only transitions if currently in Transferring state; leaves all other states unchanged.
[[nodiscard]] CompleteTransferResult completeTransferOperation(const State &state);

/// @brief Result of advancing the recursive delete progress counter.
struct AdvanceDeleteResult
{
    State newState;
    bool advanced = false;  ///< True if the path matched and deletedCount was incremented
    QString fileName;       ///< Extracted file name for the progress signal
    int currentCount = 0;   ///< New deletedCount value after advance
    int totalCount = 0;     ///< Total deleteQueue size
};

/// @brief Advance the recursive delete progress counter when a file is removed.
/// Only advances if the queue is in Deleting state and the path matches deleteQueue[deletedCount].
[[nodiscard]] AdvanceDeleteResult advanceDeleteProgress(const State &state, const QString &path);

/// @brief Result of finding an InProgress delete item matching a remote path.
struct FindDeleteItemResult
{
    bool found = false;  ///< True if a matching InProgress delete item was found
    int itemIndex = -1;  ///< Index of the matched item in state.items
    QString fileName;    ///< Extracted file name for signal emission
};

/// @brief Find an InProgress delete item matching the given remote path.
/// Does NOT modify state — returns the item index for the caller to act on.
[[nodiscard]] FindDeleteItemResult findInProgressDeleteItem(const State &state,
                                                            const QString &path);

}  // namespace transfer

#endif  // TRANSFERCORE_H
