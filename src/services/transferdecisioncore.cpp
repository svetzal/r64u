/**
 * @file transferdecisioncore.cpp
 * @brief Implementation of processNext/processNextDelete decision and FTP error handling.
 */

#include "transferdecisioncore.h"

#include <QFileInfo>

namespace transfer {

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

State handleOperationTimeout(const State &state)
{
    State result = state;

    auto it = std::find_if(result.items.begin(), result.items.end(), [](const TransferItem &item) {
        return item.status == TransferItem::Status::InProgress;
    });
    if (it != result.items.end()) {
        it->status = TransferItem::Status::Failed;
        it->errorMessage = QStringLiteral("Operation timed out");
    }

    result.currentIndex = -1;
    result.queueState = QueueState::Idle;

    return result;
}

NextDeleteDecision decideNextDeleteAction(const State &state)
{
    NextDeleteDecision decision;
    if (state.deletedCount >= state.deleteQueue.size()) {
        decision.completedCount = state.deletedCount;
        decision.action = state.pendingUploadAfterDelete ? NextDeleteAction::PendingUploadReady
                                                         : NextDeleteAction::AllDone;
        return decision;
    }
    decision.action = NextDeleteAction::DispatchNext;
    decision.nextItem = state.deleteQueue[state.deletedCount];
    return decision;
}

}  // namespace transfer
