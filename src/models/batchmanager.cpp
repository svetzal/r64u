#include "batchmanager.h"

#include "../utils/logging.h"

#include <QFileInfo>

#include <algorithm>

BatchManager::BatchManager(transfer::State &state, QObject *parent) : QObject(parent), state_(state)
{
}

void BatchManager::setRowRemovalCallbacks(RowRangeCallback beginRemove, VoidCallback endRemove)
{
    beginRemoveCb_ = std::move(beginRemove);
    endRemoveCb_ = std::move(endRemove);
}

int BatchManager::createBatch(OperationType type, const QString &description,
                              const QString &folderName, const QString &sourcePath)
{
    emit modelAboutToReset();
    auto result = transfer::createBatch(state_, type, description, folderName, sourcePath);
    state_ = result.newState;
    emit modelReset();

    qCDebug(LogTransfer) << "BatchManager: Created batch" << result.batchId << ":" << description;
    emit queueChanged();

    return result.batchId;
}

void BatchManager::activateNextBatch()
{
    state_ = transfer::activateNextBatch(state_);

    if (state_.activeBatchIndex >= 0) {
        qCDebug(LogTransfer) << "BatchManager: Activated batch"
                             << state_.batches[state_.activeBatchIndex].batchId;
        emit batchStarted(state_.batches[state_.activeBatchIndex].batchId);
    } else {
        qCDebug(LogTransfer) << "BatchManager: No more batches to activate";
    }
}

void BatchManager::completeBatch(int batchId)
{
    const TransferBatch *batch = findBatch(batchId);
    if (!batch) {
        return;
    }

    qCDebug(LogTransfer) << "BatchManager: Completing batch" << batchId
                         << "completed:" << batch->completedCount << "failed:" << batch->failedCount
                         << "total:" << batch->totalCount();

    // Delegate state transitions entirely to the pure core, including activateNextBatch()
    auto result = transfer::completeBatch(state_, batchId);
    state_ = result.newState;

    emit stopTimeoutRequested();
    emit batchCompleted(batchId);

    if (result.isFolderOperation) {
        emit folderOpCompleteRequested();
        return;
    }

    // transfer::completeBatch() already called activateNextBatch() internally;
    // emit batchStarted if a new batch was activated.
    if (state_.activeBatchIndex >= 0) {
        qCDebug(LogTransfer) << "BatchManager: Activated batch"
                             << state_.batches[state_.activeBatchIndex].batchId;
        emit batchStarted(state_.batches[state_.activeBatchIndex].batchId);
    } else {
        qCDebug(LogTransfer) << "BatchManager: No more batches to activate";
    }

    if (!result.hasRemainingActiveBatches) {
        qCDebug(LogTransfer) << "BatchManager: All batches complete";
        emit allOperationsCompleted();
    } else if (state_.activeBatchIndex >= 0) {
        emit scheduleProcessNextRequested();
    }
}

void BatchManager::purgeBatch(int batchId)
{
    // Use the pure core to plan which rows to remove (indices in descending order),
    // then apply them one-by-one so Qt model signals can be interleaved correctly.
    const auto plan = transfer::planBatchPurge(state_, batchId);
    if (plan.batchIndex < 0) {
        return;
    }

    qCDebug(LogTransfer) << "BatchManager: Purging batch" << batchId;

    for (int idx : plan.itemIndicesToRemove) {
        emit rowsAboutToBeRemoved(idx, idx);
        if (beginRemoveCb_) {
            beginRemoveCb_(idx, idx);
        }
        state_.items.removeAt(idx);
        emit rowsRemoved();
        if (endRemoveCb_) {
            endRemoveCb_();
        }

        if (state_.currentIndex > idx) {
            state_.currentIndex--;
        } else if (state_.currentIndex == idx) {
            state_.currentIndex = -1;
        }
    }

    if (state_.activeBatchIndex == plan.batchIndex) {
        state_.activeBatchIndex = -1;
    } else if (state_.activeBatchIndex > plan.batchIndex) {
        state_.activeBatchIndex--;
    }

    state_.batches.removeAt(plan.batchIndex);
    emit queueChanged();
}

bool BatchManager::handleSkipBatchCompletion(int batchId)
{
    if (batchId < 0) {
        return false;
    }
    const TransferBatch *batch = findBatch(batchId);
    if (!batch || !batch->isComplete()) {
        return false;
    }
    emitBatchProgressAndComplete(batchId, true);
    return true;
}

void BatchManager::emitBatchProgressAndComplete(int batchId, bool batchIsComplete,
                                                bool includeFailed)
{
    if (TransferBatch *batch = findBatch(batchId)) {
        int completed =
            includeFailed ? batch->completedCount + batch->failedCount : batch->completedCount;
        emit batchProgressUpdate(batchId, completed, batch->totalCount());
    }
    if (batchIsComplete) {
        completeBatch(batchId);
    }
}

TransferBatch *BatchManager::findBatch(int batchId)
{
    auto it = std::find_if(state_.batches.begin(), state_.batches.end(),
                           [batchId](const TransferBatch &b) { return b.batchId == batchId; });
    return it != state_.batches.end() ? &(*it) : nullptr;
}

const TransferBatch *BatchManager::findBatch(int batchId) const
{
    auto it = std::find_if(state_.batches.cbegin(), state_.batches.cend(),
                           [batchId](const TransferBatch &b) { return b.batchId == batchId; });
    return it != state_.batches.cend() ? &(*it) : nullptr;
}

TransferBatch *BatchManager::activeBatch()
{
    if (state_.activeBatchIndex >= 0 && state_.activeBatchIndex < state_.batches.size()) {
        return &state_.batches[state_.activeBatchIndex];
    }
    return nullptr;
}

QList<int> BatchManager::allBatchIds() const
{
    return transfer::allBatchIds(state_);
}

bool BatchManager::hasActiveBatch() const
{
    return transfer::hasActiveBatch(state_);
}

int BatchManager::queuedBatchCount() const
{
    return transfer::queuedBatchCount(state_);
}

BatchProgress BatchManager::activeBatchProgress() const
{
    return transfer::computeActiveBatchProgress(state_);
}

BatchProgress BatchManager::batchProgress(int batchId) const
{
    return transfer::computeBatchProgress(state_, batchId);
}
