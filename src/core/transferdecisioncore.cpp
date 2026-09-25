/**
 * @file transferdecisioncore.cpp
 * @brief Implementation of processNext/processNextDelete decision and FTP error handling.
 */

#include "transferdecisioncore.h"

#include <QFileInfo>

namespace transfer {

bool mayOverwriteWithoutAsking(const State &state, const TransferItem &item)
{
    return state.autoOverwrite || item.confirmed ||
           (state.overwriteAllBatchId >= 0 && item.batchId == state.overwriteAllBatchId);
}

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

        const bool mayOverwrite = mayOverwriteWithoutAsking(state, item);

        // Check for file existence confirmation (downloads)
        if (item.operationType == OperationType::Download && !mayOverwrite) {
            if (localFileExists(item.localPath)) {
                decision.action = ProcessNextAction::NeedOverwriteCheck_Download;
                return decision;
            }
        }

        // Check for remote file existence (uploads)
        if (item.operationType == OperationType::Upload && !mayOverwrite) {
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

OperationTimeoutResult handleOperationTimeout(const State &state, const QString &errorMessage)
{
    OperationTimeoutResult result;
    result.newState = state;

    auto it = std::find_if(state.items.cbegin(), state.items.cend(), [](const TransferItem &item) {
        return item.status == TransferItem::Status::InProgress;
    });
    if (it != state.items.cend()) {
        result.failedIndex = static_cast<int>(std::distance(state.items.cbegin(), it));
        auto marked =
            markItemComplete(state, result.failedIndex, TransferItem::Status::Failed, errorMessage);
        result.newState = marked.newState;
        result.batchId = marked.batchId;
        result.batchIsComplete = marked.batchIsComplete;
    }

    result.newState.currentIndex = -1;
    result.newState.queueState = QueueState::Idle;

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
