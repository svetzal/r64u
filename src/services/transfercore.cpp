/**
 * @file transfercore.cpp
 * @brief Implementation of pure transfer core functions.
 */

#include "transfercore.h"

#include <QFileInfo>

#include <algorithm>

namespace transfer {

// ---------------------------------------------------------------------------
// Path utilities
// ---------------------------------------------------------------------------

QString normalizePath(const QString &path)
{
    QString result = path;
    while (result.endsWith('/') && result.length() > 1) {
        result.chop(1);
    }
    return result;
}

// ---------------------------------------------------------------------------
// Item counting
// ---------------------------------------------------------------------------

int pendingCount(const State &state)
{
    return static_cast<int>(
        std::count_if(state.items.begin(), state.items.end(), [](const auto &item) {
            return item.status == TransferItem::Status::Pending;
        }));
}

int activeCount(const State &state)
{
    return static_cast<int>(
        std::count_if(state.items.begin(), state.items.end(), [](const auto &item) {
            return item.status == TransferItem::Status::InProgress;
        }));
}

int activeAndPendingCount(const State &state)
{
    return static_cast<int>(
        std::count_if(state.items.begin(), state.items.end(), [](const auto &item) {
            return item.status == TransferItem::Status::Pending ||
                   item.status == TransferItem::Status::InProgress;
        }));
}

// ---------------------------------------------------------------------------
// Batch queries
// ---------------------------------------------------------------------------

int queuedBatchCount(const State &state)
{
    return static_cast<int>(std::count_if(state.batches.begin(), state.batches.end(),
                                          [](const auto &batch) { return !batch.isComplete(); }));
}

QList<int> allBatchIds(const State &state)
{
    QList<int> ids;
    for (const auto &batch : state.batches) {
        if (!batch.isComplete())
            ids.append(batch.batchId);
    }
    return ids;
}

bool hasActiveBatch(const State &state)
{
    return state.activeBatchIndex >= 0 && state.activeBatchIndex < state.batches.size();
}

// ---------------------------------------------------------------------------
// Lookup functions
// ---------------------------------------------------------------------------

int findBatchIndex(const State &state, int batchId)
{
    for (int i = 0; i < state.batches.size(); ++i) {
        if (state.batches[i].batchId == batchId)
            return i;
    }
    return -1;
}

int findItemIndex(const State &state, const QString &localPath, const QString &remotePath)
{
    for (int i = 0; i < state.items.size(); ++i) {
        if (state.items[i].localPath == localPath && state.items[i].remotePath == remotePath)
            return i;
    }
    return -1;
}

int findNextPendingItem(const State &state)
{
    for (int i = 0; i < state.items.size(); ++i) {
        if (state.items[i].status == TransferItem::Status::Pending)
            return i;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Duplicate detection
// ---------------------------------------------------------------------------

bool isPathBeingTransferred(const State &state, const QString &path, OperationType type)
{
    QString normalizedPath = normalizePath(path);

    bool inBatch =
        std::any_of(state.batches.begin(), state.batches.end(), [&](const TransferBatch &batch) {
            return !batch.isComplete() && batch.operationType == type &&
                   !batch.sourcePath.isEmpty() && normalizePath(batch.sourcePath) == normalizedPath;
        });
    if (inBatch)
        return true;

    bool inScan = std::any_of(
        state.pendingScans.begin(), state.pendingScans.end(),
        [&](const PendingScan &scan) { return normalizePath(scan.remotePath) == normalizedPath; });
    if (inScan)
        return true;

    return std::any_of(state.pendingFolderOps.begin(), state.pendingFolderOps.end(),
                       [&](const PendingFolderOp &op) {
                           return op.operationType == type &&
                                  normalizePath(op.sourcePath) == normalizedPath;
                       });
}

// ---------------------------------------------------------------------------
// State machine gate
// ---------------------------------------------------------------------------

bool canProcessNext(QueueState queueState)
{
    return queueState == QueueState::Idle;
}

// ---------------------------------------------------------------------------
// Progress computation
// ---------------------------------------------------------------------------

BatchProgress computeActiveBatchProgress(const State &state)
{
    BatchProgress progress;

    if (state.activeBatchIndex >= 0 && state.activeBatchIndex < state.batches.size()) {
        const TransferBatch &batch = state.batches[state.activeBatchIndex];
        progress.batchId = batch.batchId;
        progress.description = batch.description;
        progress.folderName = batch.folderName;
        progress.operationType = batch.operationType;
        progress.totalItems = batch.totalCount();
        progress.completedItems = batch.completedCount;
        progress.failedItems = batch.failedCount;
    }

    progress.isScanning = (state.queueState == QueueState::Scanning);
    progress.isCreatingDirectories = (state.queueState == QueueState::CreatingDirectories);
    progress.isProcessingDelete = (state.queueState == QueueState::Deleting);
    progress.deleteProgress = state.deletedCount;
    progress.deleteTotalCount = state.deleteQueue.size();
    progress.scanningFolder = state.scanningFolderName;
    progress.directoriesScanned = state.directoriesScanned;
    progress.directoriesRemaining = state.pendingScans.size() + state.pendingDeleteScans.size();
    progress.filesDiscovered = state.filesDiscovered;
    progress.directoriesCreated = state.directoriesCreated;
    progress.directoriesToCreate = state.totalDirectoriesToCreate;

    return progress;
}

BatchProgress computeBatchProgress(const State &state, int batchId)
{
    BatchProgress progress;

    for (int i = 0; i < state.batches.size(); ++i) {
        const TransferBatch &batch = state.batches[i];
        if (batch.batchId == batchId) {
            progress.batchId = batch.batchId;
            progress.description = batch.description;
            progress.folderName = batch.folderName;
            progress.operationType = batch.operationType;
            progress.totalItems = batch.totalCount();
            progress.completedItems = batch.completedCount;
            progress.failedItems = batch.failedCount;

            if (state.activeBatchIndex == i) {
                progress.isScanning = (state.queueState == QueueState::Scanning);
                progress.isCreatingDirectories =
                    (state.queueState == QueueState::CreatingDirectories);
                progress.isProcessingDelete = (state.queueState == QueueState::Deleting);
                progress.deleteProgress = state.deletedCount;
                progress.deleteTotalCount = state.deleteQueue.size();
                progress.scanningFolder = state.scanningFolderName;
                progress.directoriesScanned = state.directoriesScanned;
                progress.directoriesRemaining =
                    state.pendingScans.size() + state.pendingDeleteScans.size();
                progress.filesDiscovered = state.filesDiscovered;
                progress.directoriesCreated = state.directoriesCreated;
                progress.directoriesToCreate = state.totalDirectoriesToCreate;
            }
            break;
        }
    }

    return progress;
}

// ---------------------------------------------------------------------------
// Model data
// ---------------------------------------------------------------------------

QVariant itemData(const TransferItem &item, int role)
{
    switch (role) {
    case Qt::DisplayRole:
    case static_cast<int>(ItemRole::FileName): {
        const QString &path =
            (item.operationType == OperationType::Upload) ? item.localPath : item.remotePath;
        return QFileInfo(path).fileName();
    }
    case static_cast<int>(ItemRole::LocalPath):
        return item.localPath;
    case static_cast<int>(ItemRole::RemotePath):
        return item.remotePath;
    case static_cast<int>(ItemRole::OperationType):
        return static_cast<int>(item.operationType);
    case static_cast<int>(ItemRole::Status):
        return static_cast<int>(item.status);
    case static_cast<int>(ItemRole::Progress):
        if (item.totalBytes > 0)
            return static_cast<int>((item.bytesTransferred * 100) / item.totalBytes);
        return 0;
    case static_cast<int>(ItemRole::BytesTransferred):
        return item.bytesTransferred;
    case static_cast<int>(ItemRole::TotalBytes):
        return item.totalBytes;
    case static_cast<int>(ItemRole::ErrorMessage):
        return item.errorMessage;
    default:
        return {};
    }
}

// ---------------------------------------------------------------------------
// State mutation functions
// ---------------------------------------------------------------------------

State purgeBatch(const State &state, int batchId)
{
    State result = state;

    for (int i = 0; i < result.batches.size(); ++i) {
        if (result.batches[i].batchId == batchId) {
            for (int j = result.items.size() - 1; j >= 0; --j) {
                if (result.items[j].batchId == batchId) {
                    result.items.removeAt(j);
                    if (result.currentIndex > j) {
                        result.currentIndex--;
                    } else if (result.currentIndex == j) {
                        result.currentIndex = -1;
                    }
                }
            }

            if (result.activeBatchIndex == i) {
                result.activeBatchIndex = -1;
            } else if (result.activeBatchIndex > i) {
                result.activeBatchIndex--;
            }

            result.batches.removeAt(i);
            return result;
        }
    }

    return result;
}

CreateBatchResult createBatch(const State &state, OperationType type, const QString &description,
                              const QString &folderName, const QString &sourcePath)
{
    State result = state;

    // Purge completed batches first (skip those still being populated by pending scans)
    for (int i = result.batches.size() - 1; i >= 0; --i) {
        if (result.batches[i].isComplete()) {
            bool stillPopulating = std::any_of(
                result.pendingScans.begin(), result.pendingScans.end(),
                [&](const PendingScan &scan) { return scan.batchId == result.batches[i].batchId; });
            if (!stillPopulating) {
                result = purgeBatch(result, result.batches[i].batchId);
            }
        }
    }

    TransferBatch batch;
    batch.batchId = result.nextBatchId++;
    batch.operationType = type;
    batch.description = description;
    batch.folderName = folderName;
    batch.sourcePath = sourcePath;
    batch.scanned = false;
    batch.folderConfirmed = false;

    result.batches.append(batch);

    return {result, batch.batchId};
}

State activateNextBatch(const State &state)
{
    State result = state;

    for (int i = 0; i < result.batches.size(); ++i) {
        if (!result.batches[i].isComplete() && result.batches[i].pendingCount() > 0) {
            result.activeBatchIndex = i;
            return result;
        }
    }

    result.activeBatchIndex = -1;
    return result;
}

CompleteBatchResult completeBatch(const State &state, int batchId)
{
    CompleteBatchResult result;
    result.batchId = batchId;

    if (findBatchIndex(state, batchId) < 0) {
        result.newState = state;
        return result;
    }

    result.newState = state;
    result.newState.activeBatchIndex = -1;
    result.newState.currentIndex = -1;
    result.newState.queueState = QueueState::Idle;

    // Detect if this is a folder operation batch
    result.isFolderOperation = (result.newState.currentFolderOp.batchId == batchId);

    if (!result.isFolderOperation) {
        result.newState = activateNextBatch(result.newState);

        result.hasRemainingActiveBatches =
            std::any_of(result.newState.batches.begin(), result.newState.batches.end(),
                        [](const TransferBatch &b) { return !b.isComplete(); });

        if (!result.hasRemainingActiveBatches) {
            result.newState.overwriteAll = false;
        }
    }

    return result;
}

MarkCompleteResult markItemComplete(const State &state, int itemIndex, TransferItem::Status status,
                                    const QString &errorMessage)
{
    MarkCompleteResult result;
    result.newState = state;

    if (itemIndex < 0 || itemIndex >= result.newState.items.size()) {
        return result;
    }

    result.newState.items[itemIndex].status = status;
    if (status == TransferItem::Status::Completed) {
        result.newState.items[itemIndex].bytesTransferred =
            result.newState.items[itemIndex].totalBytes;
    }
    if (!errorMessage.isEmpty()) {
        result.newState.items[itemIndex].errorMessage = errorMessage;
    }

    result.batchId = result.newState.items[itemIndex].batchId;

    int batchIdx = findBatchIndex(result.newState, result.batchId);
    if (batchIdx >= 0) {
        if (status == TransferItem::Status::Completed || status == TransferItem::Status::Skipped) {
            result.newState.batches[batchIdx].completedCount++;
        } else if (status == TransferItem::Status::Failed) {
            result.newState.batches[batchIdx].failedCount++;
        }
        result.batchIsComplete = result.newState.batches[batchIdx].isComplete();
    }

    result.newState.currentIndex = -1;
    return result;
}

State clearAll(const State & /*state*/)
{
    return State{};
}

State removeCompleted(const State &state)
{
    State result = state;

    for (int i = result.items.size() - 1; i >= 0; --i) {
        if (result.items[i].status == TransferItem::Status::Completed ||
            result.items[i].status == TransferItem::Status::Failed ||
            result.items[i].status == TransferItem::Status::Skipped) {
            result.items.removeAt(i);
            if (result.currentIndex > i) {
                result.currentIndex--;
            } else if (result.currentIndex == i) {
                result.currentIndex = -1;
            }
        }
    }

    return result;
}

State cancelAllItems(const State &state)
{
    State result = state;

    for (auto &item : result.items) {
        if (item.status == TransferItem::Status::Pending ||
            item.status == TransferItem::Status::InProgress) {
            item.status = TransferItem::Status::Failed;
            item.errorMessage = QStringLiteral("Cancelled");
        }
    }

    result.currentIndex = -1;
    result.batches.clear();
    result.activeBatchIndex = -1;

    result.pendingScans.clear();
    result.requestedListings.clear();
    result.pendingMkdirs.clear();
    result.pendingDeleteScans.clear();
    result.requestedDeleteListings.clear();
    result.deleteQueue.clear();
    result.deletedCount = 0;

    result.pendingConfirmation.clear();
    result.pendingFolderOps.clear();
    result.currentFolderOp = PendingFolderOp();
    result.replaceExisting = false;

    result.queueState = QueueState::Idle;

    return result;
}

CancelBatchResult cancelBatch(const State &state, int batchId)
{
    CancelBatchResult result;
    result.newState = state;

    int batchIdx = findBatchIndex(state, batchId);
    if (batchIdx < 0) {
        return result;
    }

    result.wasActiveBatch = (batchIdx == result.newState.activeBatchIndex);

    if (result.wasActiveBatch) {
        result.newState.currentIndex = -1;

        if (result.newState.queueState == QueueState::Scanning) {
            result.newState.pendingScans.clear();
            result.newState.pendingDeleteScans.clear();
            result.newState.requestedListings.clear();
            result.newState.requestedDeleteListings.clear();
        }

        if (result.newState.queueState == QueueState::CreatingDirectories) {
            result.newState.pendingMkdirs.clear();
        }

        if (result.newState.queueState == QueueState::Deleting) {
            result.newState.deleteQueue.clear();
            result.newState.deletedCount = 0;
        }

        result.newState.queueState = QueueState::Idle;
    }

    for (auto &item : result.newState.items) {
        if (item.batchId == batchId && (item.status == TransferItem::Status::Pending ||
                                        item.status == TransferItem::Status::InProgress)) {
            item.status = TransferItem::Status::Failed;
            item.errorMessage = QStringLiteral("Cancelled");
        }
    }

    result.newState = purgeBatch(result.newState, batchId);

    if (result.wasActiveBatch) {
        result.newState = activateNextBatch(result.newState);
    }

    return result;
}

// ---------------------------------------------------------------------------
// Phase 5: Scanning and directory listing helpers
// ---------------------------------------------------------------------------

QList<DeleteItem> sortDeleteQueue(const QList<DeleteItem> &queue)
{
    QList<DeleteItem> result = queue;
    std::sort(result.begin(), result.end(), [](const DeleteItem &a, const DeleteItem &b) {
        if (!a.isDirectory && b.isDirectory)
            return true;
        if (a.isDirectory && !b.isDirectory)
            return false;
        if (a.isDirectory && b.isDirectory) {
            return a.path.count('/') > b.path.count('/');
        }
        return false;
    });
    return result;
}

DirectoryListingResult processDirectoryListingForDownload(const PendingScan &currentScan,
                                                          const QList<FtpEntry> &entries)
{
    DirectoryListingResult result;
    result.directoriesScanned = 1;

    // Calculate local directory for this path
    QString localTargetDir;
    if (currentScan.remotePath == currentScan.remoteBasePath) {
        localTargetDir = currentScan.localBasePath;
    } else {
        QString relativePath = currentScan.remotePath.mid(currentScan.remoteBasePath.length());
        if (relativePath.startsWith('/'))
            relativePath = relativePath.mid(1);
        localTargetDir = currentScan.localBasePath + '/' + relativePath;
    }

    for (const FtpEntry &entry : entries) {
        QString entryRemotePath = currentScan.remotePath;
        if (!entryRemotePath.endsWith('/'))
            entryRemotePath += '/';
        entryRemotePath += entry.name;

        if (entry.isDirectory) {
            PendingScan subScan;
            subScan.remotePath = entryRemotePath;
            subScan.localBasePath = currentScan.localBasePath;
            subScan.remoteBasePath = currentScan.remoteBasePath;
            subScan.batchId = currentScan.batchId;
            result.newSubScans.append(subScan);
        } else {
            QString localFilePath = localTargetDir + '/' + entry.name;
            result.newFileDownloads.append({entryRemotePath, localFilePath});
        }
    }

    return result;
}

DeleteListingResult processDirectoryListingForDelete(const QString &path,
                                                     const QList<FtpEntry> &entries)
{
    DeleteListingResult result;

    for (const FtpEntry &entry : entries) {
        QString entryPath = path;
        if (!entryPath.endsWith('/'))
            entryPath += '/';
        entryPath += entry.name;

        if (entry.isDirectory) {
            PendingScan subScan;
            subScan.remotePath = entryPath;
            result.newSubScans.append(subScan);
        } else {
            DeleteItem item;
            item.path = entryPath;
            item.isDirectory = false;
            result.fileItems.append(item);
        }
    }

    // The directory itself is deleted after its contents
    result.directoryItem.path = path;
    result.directoryItem.isDirectory = true;

    return result;
}

State updateFolderExistence(const State &state, const QString &parentPath,
                            const QList<FtpEntry> &entries)
{
    State result = state;

    QSet<QString> existingDirs;
    for (const FtpEntry &entry : entries) {
        if (entry.isDirectory) {
            existingDirs.insert(entry.name);
        }
    }

    QQueue<PendingFolderOp> updatedOps;
    while (!result.pendingFolderOps.isEmpty()) {
        PendingFolderOp op = result.pendingFolderOps.dequeue();
        if (op.destPath == parentPath) {
            QString targetFolderName = QFileInfo(op.targetPath).fileName();
            op.destExists = existingDirs.contains(targetFolderName);
        }
        updatedOps.enqueue(op);
    }
    result.pendingFolderOps = updatedOps;

    return result;
}

UploadFileCheckResult checkUploadFileExists(const State &state, const QList<FtpEntry> &entries)
{
    UploadFileCheckResult result;
    result.newState = state;

    if (state.currentIndex < 0 || state.currentIndex >= state.items.size()) {
        return result;
    }

    const QString targetFileName = QFileInfo(state.items[state.currentIndex].remotePath).fileName();
    result.fileName = targetFileName;

    result.fileExists = std::any_of(entries.begin(), entries.end(), [&](const FtpEntry &entry) {
        return !entry.isDirectory && entry.name == targetFileName;
    });

    if (result.fileExists) {
        result.newState.queueState = QueueState::AwaitingFileConfirm;
        result.newState.pendingConfirmation.itemIndex = state.currentIndex;
        result.newState.pendingConfirmation.opType = OperationType::Upload;
    } else {
        result.newState.items[state.currentIndex].confirmed = true;
    }

    return result;
}

// ---------------------------------------------------------------------------
// Phase 4: Confirmation functions
// ---------------------------------------------------------------------------

OverwriteResult respondToOverwrite(const State &state, OverwriteResponse response)
{
    if (state.queueState != QueueState::AwaitingFileConfirm) {
        return {state, false, false};
    }

    OverwriteResult result;
    result.newState = state;
    result.newState.queueState = QueueState::Idle;

    int itemIdx = result.newState.pendingConfirmation.itemIndex;
    result.newState.pendingConfirmation.clear();

    switch (response) {
    case OverwriteResponse::Overwrite:
        if (itemIdx >= 0 && itemIdx < result.newState.items.size()) {
            result.newState.items[itemIdx].confirmed = true;
        }
        result.shouldScheduleProcessNext = true;
        break;

    case OverwriteResponse::OverwriteAll:
        result.newState.overwriteAll = true;
        result.shouldScheduleProcessNext = true;
        break;

    case OverwriteResponse::Skip:
        if (itemIdx >= 0 && itemIdx < result.newState.items.size()) {
            result.newState.items[itemIdx].status = TransferItem::Status::Skipped;
            result.newState.items[itemIdx].errorMessage = QStringLiteral("Skipped");

            int batchId = result.newState.items[itemIdx].batchId;
            int batchIdx = findBatchIndex(result.newState, batchId);
            if (batchIdx >= 0) {
                result.newState.batches[batchIdx].completedCount++;
            }
        }
        result.newState.currentIndex = -1;
        result.shouldScheduleProcessNext = true;
        break;

    case OverwriteResponse::Cancel:
        result.shouldCancelAll = true;
        break;
    }

    return result;
}

FolderExistsResult respondToFolderExists(const State &state, FolderExistsResponse response)
{
    if (state.queueState != QueueState::AwaitingFolderConfirm) {
        return {state, false, PendingFolderOp{}, false};
    }

    FolderExistsResult result;
    result.newState = state;
    result.newState.pendingConfirmation.clear();
    result.newState.queueState = QueueState::Idle;

    switch (response) {
    case FolderExistsResponse::Merge:
        result.newState.replaceExisting = false;
        if (!result.newState.pendingFolderOps.isEmpty()) {
            result.folderOpToStart = result.newState.pendingFolderOps.dequeue();
            result.shouldStartFolderOp = true;
        }
        break;

    case FolderExistsResponse::Replace:
        result.newState.replaceExisting = true;
        if (!result.newState.pendingFolderOps.isEmpty()) {
            result.folderOpToStart = result.newState.pendingFolderOps.dequeue();
            result.shouldStartFolderOp = true;
        }
        break;

    case FolderExistsResponse::Cancel:
        result.newState.pendingFolderOps.clear();
        result.newState.currentFolderOp = PendingFolderOp{};
        result.shouldCancelFolderOps = true;
        break;
    }

    return result;
}

FolderConfirmResult checkFolderConfirmation(const State &state)
{
    FolderConfirmResult result;
    result.newState = state;

    QStringList existingFolders;
    for (const PendingFolderOp &op : result.newState.pendingFolderOps) {
        if (op.destExists) {
            existingFolders.append(QFileInfo(op.targetPath).fileName());
        }
    }

    if (!existingFolders.isEmpty()) {
        result.needsConfirmation = true;
        result.existingFolderNames = existingFolders;
        result.newState.queueState = QueueState::AwaitingFolderConfirm;
        if (!result.newState.pendingFolderOps.isEmpty()) {
            result.newState.pendingConfirmation.opType =
                result.newState.pendingFolderOps.head().operationType;
        }
        result.newState.pendingConfirmation.folderNames = existingFolders;
        return result;
    }

    // No confirmation needed
    if (!result.newState.pendingFolderOps.isEmpty()) {
        result.folderOpToStart = result.newState.pendingFolderOps.dequeue();
        result.shouldStartFolderOp = true;
    } else {
        result.newState.queueState = QueueState::Idle;
    }

    return result;
}

// ---------------------------------------------------------------------------
// Phase 7: processNext decision logic
// ---------------------------------------------------------------------------

ProcessNextDecision decideNextAction(const State &state, bool ftpConnected,
                                     const std::function<bool(const QString &)> &localFileExists)
{
    ProcessNextDecision decision;

    if (!canProcessNext(state.queueState)) {
        decision.action = ProcessNextAction::Blocked;
        return decision;
    }

    if (!ftpConnected) {
        decision.action = ProcessNextAction::NoFtpClient;
        return decision;
    }

    // Check for pending folder operations
    if (!state.pendingFolderOps.isEmpty() && state.currentFolderOp.batchId < 0) {
        decision.action = ProcessNextAction::StartFolderOp;
        decision.folderOpToStart = state.pendingFolderOps.head();
        return decision;
    }

    // Find next pending item
    for (int i = 0; i < state.items.size(); ++i) {
        if (state.items[i].status != TransferItem::Status::Pending) {
            continue;
        }

        decision.itemIndex = i;
        const TransferItem &item = state.items[i];
        QString fileName = QFileInfo(item.operationType == OperationType::Upload ? item.localPath
                                                                                 : item.remotePath)
                               .fileName();
        decision.fileNameForSignal = fileName;

        // Check for file existence confirmation (downloads)
        if (item.operationType == OperationType::Download && !state.overwriteAll &&
            !item.confirmed) {
            if (localFileExists(item.localPath)) {
                decision.action = ProcessNextAction::NeedOverwriteCheck_Download;
                return decision;
            }
        }

        // Check for remote file existence (uploads)
        if (item.operationType == OperationType::Upload && !state.overwriteAll && !item.confirmed) {
            QString parentDir = QFileInfo(item.remotePath).path();
            if (parentDir.isEmpty()) {
                parentDir = QStringLiteral("/");
            }
            decision.action = ProcessNextAction::NeedOverwriteCheck_Upload;
            decision.uploadCheckDir = parentDir;
            return decision;
        }

        // Start the transfer
        decision.action = ProcessNextAction::StartTransfer;
        return decision;
    }

    decision.action = ProcessNextAction::NoPending;
    decision.itemIndex = -1;
    return decision;
}

// ---------------------------------------------------------------------------
// Phase 7: FTP error handling
// ---------------------------------------------------------------------------

FtpErrorResult handleFtpError(const State &state, const QString &message)
{
    FtpErrorResult result;
    result.newState = state;

    // Handle delete errors: skip and continue (before clearing pending requests)
    if (state.queueState == QueueState::Deleting && state.deletedCount < state.deleteQueue.size()) {
        result.isDeleteError = true;
        result.deleteFileName = QFileInfo(state.deleteQueue[state.deletedCount].path).fileName();
        result.newState.deletedCount++;
        result.shouldProcessNextDelete = true;
        return result;
    }

    // Clear all pending requests
    result.newState.requestedListings.clear();
    result.newState.requestedDeleteListings.clear();
    result.newState.requestedFolderCheckListings.clear();
    result.newState.requestedUploadFileCheckListings.clear();
    result.newState.pendingScans.clear();
    result.newState.pendingDeleteScans.clear();
    result.newState.pendingMkdirs.clear();

    // Handle folder upload failure during directory creation
    if (state.currentFolderOp.batchId > 0 && state.queueState == QueueState::CreatingDirectories) {
        result.isFolderCreationError = true;
        result.folderName = QFileInfo(state.currentFolderOp.sourcePath).fileName();
        result.folderBatchId = state.currentFolderOp.batchId;
        result.newState.queueState = QueueState::Idle;
        return result;
    }

    // Handle transfer error
    if (state.currentIndex >= 0 && state.currentIndex < state.items.size()) {
        result.hasCurrentItem = true;
        result.newState.items[state.currentIndex].status = TransferItem::Status::Failed;
        result.newState.items[state.currentIndex].errorMessage = message;

        const TransferItem &item = result.newState.items[state.currentIndex];
        result.transferFileName =
            QFileInfo(item.operationType == OperationType::Upload ? item.localPath
                                                                  : item.remotePath)
                .fileName();

        result.failedBatchId = item.batchId;
        int batchIdx = findBatchIndex(result.newState, result.failedBatchId);
        if (batchIdx >= 0) {
            result.newState.batches[batchIdx].failedCount++;
            result.batchIsComplete = result.newState.batches[batchIdx].isComplete();
        }
    }

    result.newState.queueState = QueueState::Idle;
    result.newState.currentIndex = -1;
    result.shouldScheduleProcessNext = true;

    return result;
}

// ---------------------------------------------------------------------------
// Phase 7: Operation timeout
// ---------------------------------------------------------------------------

State handleOperationTimeout(const State &state)
{
    State result = state;

    for (int i = 0; i < result.items.size(); ++i) {
        if (result.items[i].status == TransferItem::Status::InProgress) {
            result.items[i].status = TransferItem::Status::Failed;
            result.items[i].errorMessage = QStringLiteral("Operation timed out");
            break;
        }
    }

    result.currentIndex = -1;
    result.queueState = QueueState::Idle;

    return result;
}

}  // namespace transfer
