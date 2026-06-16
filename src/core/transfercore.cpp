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

bool shouldAbortFtp(QueueState queueState)
{
    return queueState == QueueState::Transferring || queueState == QueueState::Deleting;
}

std::optional<QString> validateEnqueuePreconditions(bool connected, bool localDirExists)
{
    if (!connected) {
        return QString("Not connected to device");
    }
    if (!localDirExists) {
        return QString("Local directory does not exist");
    }
    return std::nullopt;
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

PurgeBatchPlan planBatchPurge(const State &state, int batchId)
{
    PurgeBatchPlan plan;

    for (int i = 0; i < state.batches.size(); ++i) {
        if (state.batches[i].batchId == batchId) {
            plan.batchIndex = i;
            break;
        }
    }

    if (plan.batchIndex < 0) {
        return plan;
    }

    // Iterate in reverse so indices are already in descending order
    for (int j = state.items.size() - 1; j >= 0; --j) {
        if (state.items[j].batchId == batchId) {
            plan.itemIndicesToRemove.append(j);
        }
    }

    return plan;
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

}  // namespace transfer
