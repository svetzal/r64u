#ifndef TRANSFERTYPES_H
#define TRANSFERTYPES_H

/**
 * @file transfertypes.h
 * @brief Pure data types for the transfer subsystem.
 *
 * Contains all enumerations, value structs, and the aggregate State struct
 * used by the transfer queue.  Separated from transfercore.h so that
 * UI and model files that only need the types do not pull in the full set
 * of pure-function declarations.
 *
 * transfercore.h includes this header, so callers that include only
 * transfercore.h continue to see all types without modification.
 */

#include "ftp/ftpentry.h"

#include <QAbstractItemModel>
#include <QList>
#include <QQueue>
#include <QSet>
#include <QString>
#include <QStringList>

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
 * transitions without touching I/O or Qt signals.
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

}  // namespace transfer

#endif  // TRANSFERTYPES_H
