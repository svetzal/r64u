#include "batchmanager.h"

#include <QDebug>
#include <QFileInfo>

#include <algorithm>

BatchManager::BatchManager(transfer::State &state, QObject *parent) : QObject(parent), state_(state)
{
}

void BatchManager::setModelResetCallbacks(VoidCallback beginReset, VoidCallback endReset)
{
    beginResetCb_ = std::move(beginReset);
    endResetCb_ = std::move(endReset);
}

void BatchManager::setRowRemovalCallbacks(RowRangeCallback beginRemove, VoidCallback endRemove)
{
    beginRemoveCb_ = std::move(beginRemove);
    endRemoveCb_ = std::move(endRemove);
}

void BatchManager::setStopTimeoutCallback(VoidCallback stopTimeout)
{
    stopTimeoutCb_ = std::move(stopTimeout);
}

void BatchManager::setFolderOpCompleteCallback(VoidCallback folderOpComplete)
{
    folderOpCompleteCb_ = std::move(folderOpComplete);
}

void BatchManager::setScheduleProcessNextCallback(VoidCallback scheduleNext)
{
    scheduleNextCb_ = std::move(scheduleNext);
}

int BatchManager::createBatch(OperationType type, const QString &description,
                              const QString &folderName, const QString &sourcePath)
{
    if (beginResetCb_) {
        beginResetCb_();
    }
    auto result = transfer::createBatch(state_, type, description, folderName, sourcePath);
    state_ = result.newState;
    if (endResetCb_) {
        endResetCb_();
    }

    qDebug() << "BatchManager: Created batch" << result.batchId << ":" << description;
    emit queueChanged();

    return result.batchId;
}

void BatchManager::activateNextBatch()
{
    state_ = transfer::activateNextBatch(state_);

    if (state_.activeBatchIndex >= 0) {
        qDebug() << "BatchManager: Activated batch"
                 << state_.batches[state_.activeBatchIndex].batchId;
        emit batchStarted(state_.batches[state_.activeBatchIndex].batchId);
    } else {
        qDebug() << "BatchManager: No more batches to activate";
    }
}

void BatchManager::completeBatch(int batchId)
{
    const TransferBatch *batch = findBatch(batchId);
    if (!batch) {
        return;
    }

    qDebug() << "BatchManager: Completing batch" << batchId << "completed:" << batch->completedCount
             << "failed:" << batch->failedCount << "total:" << batch->totalCount();

    state_.activeBatchIndex = -1;
    if (stopTimeoutCb_) {
        stopTimeoutCb_();
    }
    state_.currentIndex = -1;
    state_.queueState = transfer::QueueState::Idle;

    emit batchCompleted(batchId);

    if (state_.currentFolderOp.batchId == batchId) {
        if (folderOpCompleteCb_) {
            folderOpCompleteCb_();
        }
        return;
    }

    activateNextBatch();

    bool hasActiveBatches = std::any_of(state_.batches.cbegin(), state_.batches.cend(),
                                        [](const TransferBatch &b) { return !b.isComplete(); });

    if (!hasActiveBatches) {
        qDebug() << "BatchManager: All batches complete";
        state_.overwriteAll = false;
        emit allOperationsCompleted();
    } else if (state_.activeBatchIndex >= 0) {
        if (scheduleNextCb_) {
            scheduleNextCb_();
        }
    }
}

void BatchManager::purgeBatch(int batchId)
{
    for (int i = 0; i < state_.batches.size(); ++i) {
        if (state_.batches[i].batchId == batchId) {
            qDebug() << "BatchManager: Purging batch" << batchId;

            for (int j = state_.items.size() - 1; j >= 0; --j) {
                if (state_.items[j].batchId == batchId) {
                    if (beginRemoveCb_) {
                        beginRemoveCb_(j, j);
                    }
                    state_.items.removeAt(j);
                    if (endRemoveCb_) {
                        endRemoveCb_();
                    }

                    if (state_.currentIndex > j) {
                        state_.currentIndex--;
                    } else if (state_.currentIndex == j) {
                        state_.currentIndex = -1;
                    }
                }
            }

            if (state_.activeBatchIndex == i) {
                state_.activeBatchIndex = -1;
            } else if (state_.activeBatchIndex > i) {
                state_.activeBatchIndex--;
            }

            state_.batches.removeAt(i);
            emit queueChanged();
            return;
        }
    }
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
