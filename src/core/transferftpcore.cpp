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

bool isInFlightItem(const State &state, OperationType type, const QString &remotePath,
                    const QString &localPath)
{
    if (state.currentIndex < 0 || state.currentIndex >= state.items.size()) {
        return false;
    }
    const TransferItem &item = state.items[state.currentIndex];
    if (item.status != TransferItem::Status::InProgress || item.operationType != type ||
        item.remotePath != remotePath) {
        return false;
    }
    return type == OperationType::Delete || item.localPath == localPath;
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
