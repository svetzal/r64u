/**
 * @file transferconfirmcore.cpp
 * @brief Implementation of overwrite and folder-exists confirmation pure functions.
 */

#include "transferconfirmcore.h"

#include <QFileInfo>

#include <algorithm>

namespace transfer {

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
        // Covers the rest of this batch only, never transfers started later
        if (itemIdx >= 0 && itemIdx < result.newState.items.size()) {
            result.newState.items[itemIdx].confirmed = true;
            result.newState.overwriteAllBatchId = result.newState.items[itemIdx].batchId;
        }
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

bool needsFolderCheck(const State &state, const PendingFolderOp &op)
{
    return !op.confirmed && !state.autoMerge && op.operationType != OperationType::Delete;
}

namespace {

/// True for a pending operation the folder-exists dialog asks about.
bool isAskedAbout(const PendingFolderOp &op)
{
    return !op.confirmed && op.destExists;
}

}  // namespace

FolderExistsResult respondToFolderExists(const State &state, FolderExistsResponse response)
{
    if (state.queueState != QueueState::AwaitingFolderConfirm) {
        return {state, false, PendingFolderOp{}, false};
    }

    FolderExistsResult result;
    result.newState = state;
    result.newState.pendingConfirmation.clear();
    result.newState.queueState = QueueState::Idle;

    if (response != FolderExistsResponse::Cancel) {
        // The answer covers every folder the dialog listed: they are not asked about again
        for (PendingFolderOp &op : result.newState.pendingFolderOps) {
            if (isAskedAbout(op)) {
                op.confirmed = true;
            }
        }
    }

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

    case FolderExistsResponse::Cancel: {
        // Only the folders the dialog asked about are cancelled; the others still run
        auto &ops = result.newState.pendingFolderOps;
        ops.erase(std::remove_if(ops.begin(), ops.end(), isAskedAbout), ops.end());
        if (!ops.isEmpty()) {
            result.folderOpToStart = ops.dequeue();
            result.shouldStartFolderOp = true;
        } else {
            result.shouldCancelFolderOps = true;
        }
        break;
    }
    }

    return result;
}

FolderConfirmResult checkFolderConfirmation(const State &state)
{
    FolderConfirmResult result;
    result.newState = state;

    QStringList existingFolders;
    for (const PendingFolderOp &op : result.newState.pendingFolderOps) {
        if (isAskedAbout(op)) {
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

}  // namespace transfer
