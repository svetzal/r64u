/**
 * @file transferftpcore.cpp
 * @brief Implementation of FTP operation completion and enqueue helper pure functions.
 */

#include "transferftpcore.h"

#include <QFileInfo>

namespace transfer {

CompleteTransferResult completeTransferOperation(const State &state)
{
    CompleteTransferResult result;
    result.newState = state;

    if (result.newState.queueState == QueueState::Transferring) {
        result.newState.queueState = QueueState::Idle;
        result.transitionedToIdle = true;
    }

    return result;
}

AdvanceDeleteResult advanceDeleteProgress(const State &state, const QString &path)
{
    AdvanceDeleteResult result;
    result.newState = state;

    if (result.newState.queueState != QueueState::Deleting ||
        result.newState.deletedCount >= result.newState.deleteQueue.size()) {
        return result;
    }

    if (result.newState.deleteQueue[result.newState.deletedCount].path == path) {
        result.newState.deletedCount++;
        result.advanced = true;
        result.fileName = QFileInfo(path).fileName();
        result.currentCount = result.newState.deletedCount;
        result.totalCount = result.newState.deleteQueue.size();
    }

    return result;
}

FindDeleteItemResult findInProgressDeleteItem(const State &state, const QString &path)
{
    FindDeleteItemResult result;

    for (int i = 0; i < state.items.size(); ++i) {
        const auto &item = state.items[i];
        if (item.operationType == OperationType::Delete && item.remotePath == path &&
            item.status == TransferItem::Status::InProgress) {
            result.found = true;
            result.itemIndex = i;
            result.fileName = QFileInfo(path).fileName();
            return result;
        }
    }

    return result;
}

bool hasInFlightItem(const State &state)
{
    return state.currentIndex >= 0 && state.currentIndex < state.items.size() &&
           state.items[state.currentIndex].status == TransferItem::Status::InProgress;
}

bool isInFlightItem(const State &state, OperationType type, const QString &remotePath,
                    const QString &localPath)
{
    if (!hasInFlightItem(state)) {
        return false;
    }
    const TransferItem &item = state.items[state.currentIndex];
    if (item.operationType != type || item.remotePath != remotePath) {
        return false;
    }
    return type == OperationType::Delete || item.localPath == localPath;
}

int inFlightItemIndex(const State &state, OperationType type, const QString &remotePath,
                      const QString &localPath)
{
    return isInFlightItem(state, type, remotePath, localPath) ? state.currentIndex : -1;
}

int inFlightDownloadIndex(const State &state, const QString &remotePath)
{
    if (state.currentIndex < 0 || state.currentIndex >= state.items.size()) {
        return -1;
    }
    const TransferItem &item = state.items[state.currentIndex];
    return isInFlightItem(state, OperationType::Download, remotePath, item.localPath)
               ? state.currentIndex
               : -1;
}

int inFlightUploadIndex(const State &state, const QString &localPath)
{
    if (state.currentIndex < 0 || state.currentIndex >= state.items.size()) {
        return -1;
    }
    const TransferItem &item = state.items[state.currentIndex];
    return isInFlightItem(state, OperationType::Upload, item.remotePath, localPath)
               ? state.currentIndex
               : -1;
}

bool isAwaitedListing(const State &state, const QString &path)
{
    return state.requestedListings.contains(path) || state.requestedDeleteListings.contains(path) ||
           state.requestedFolderCheckListings.contains(path) ||
           state.requestedUploadFileCheckListings.contains(path);
}

std::optional<AbortableRequest> abortableRequest(const State &state)
{
    if (state.queueState == QueueState::Transferring && hasInFlightItem(state)) {
        const TransferItem &item = state.items[state.currentIndex];
        return AbortableRequest{item.operationType, item.isDirectory, item.remotePath};
    }
    if (state.queueState == QueueState::Deleting && state.deletedCount < state.deleteQueue.size()) {
        const DeleteItem &entry = state.deleteQueue[state.deletedCount];
        return AbortableRequest{OperationType::Delete, entry.isDirectory, entry.path};
    }
    return std::nullopt;
}

bool isAwaitedMkdir(const State &state, const QString &path)
{
    return state.queueState == QueueState::CreatingDirectories && !state.pendingMkdirs.isEmpty() &&
           state.pendingMkdirs.head().remotePath == path;
}

bool isAwaitedRecursiveDelete(const State &state, const QString &path)
{
    return state.queueState == QueueState::Deleting &&
           state.deletedCount < state.deleteQueue.size() &&
           state.deleteQueue[state.deletedCount].path == path;
}

EnqueueItemResult enqueueItem(const State &state, const TransferItem &item, int batchIdx)
{
    State newState = state;
    int insertedRow = newState.items.size();
    newState.items.append(item);
    newState.batches[batchIdx].items.append(item);
    return {newState, batchIdx, insertedRow};
}

}  // namespace transfer
