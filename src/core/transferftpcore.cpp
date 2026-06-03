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

EnqueueItemResult enqueueItem(const State &state, const TransferItem &item, int batchIdx)
{
    State newState = state;
    int insertedRow = newState.items.size();
    newState.items.append(item);
    newState.batches[batchIdx].items.append(item);
    return {newState, batchIdx, insertedRow};
}

}  // namespace transfer
