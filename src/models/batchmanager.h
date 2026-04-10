#ifndef BATCHMANAGER_H
#define BATCHMANAGER_H

#include "services/transfercore.h"

#include <QObject>

#include <functional>

using transfer::BatchProgress;
using transfer::OperationType;
using transfer::TransferBatch;

/// Manages the lifecycle of transfer batches: creation, activation, completion, and progress.
/// Operates on a shared transfer::State by reference (same pattern as the coordinator objects).
/// Uses callbacks for operations that belong to the owning TransferQueue (model resets,
/// timeout stop, folder-op notification, and process scheduling).
class BatchManager : public QObject
{
    Q_OBJECT

public:
    using VoidCallback = std::function<void()>;
    using RowRangeCallback = std::function<void(int, int)>;

    explicit BatchManager(transfer::State &state, QObject *parent = nullptr);

    // Callbacks set by TransferQueue for operations BatchManager cannot own directly
    void setModelResetCallbacks(VoidCallback beginReset, VoidCallback endReset);
    void setRowRemovalCallbacks(RowRangeCallback beginRemove, VoidCallback endRemove);
    void setStopTimeoutCallback(VoidCallback stopTimeout);
    void setFolderOpCompleteCallback(VoidCallback folderOpComplete);
    void setScheduleProcessNextCallback(VoidCallback scheduleNext);

    // Batch lifecycle
    int createBatch(OperationType type, const QString &description, const QString &folderName,
                    const QString &sourcePath = QString());
    void activateNextBatch();
    void completeBatch(int batchId);
    void purgeBatch(int batchId);

    // Batch progress
    void emitBatchProgressAndComplete(int batchId, bool batchIsComplete,
                                      bool includeFailed = false);

    // Find / access
    [[nodiscard]] TransferBatch *findBatch(int batchId);
    [[nodiscard]] const TransferBatch *findBatch(int batchId) const;
    [[nodiscard]] TransferBatch *activeBatch();

    // Queries (delegate to transfer:: pure functions)
    [[nodiscard]] QList<int> allBatchIds() const;
    [[nodiscard]] bool hasActiveBatch() const;
    [[nodiscard]] int queuedBatchCount() const;
    [[nodiscard]] BatchProgress activeBatchProgress() const;
    [[nodiscard]] BatchProgress batchProgress(int batchId) const;

signals:
    void batchStarted(int batchId);
    void batchProgressUpdate(int batchId, int completed, int total);
    void batchCompleted(int batchId);
    void allOperationsCompleted();
    void queueChanged();

private:
    transfer::State &state_;

    VoidCallback beginResetCb_;
    VoidCallback endResetCb_;
    RowRangeCallback beginRemoveCb_;
    VoidCallback endRemoveCb_;
    VoidCallback stopTimeoutCb_;
    VoidCallback folderOpCompleteCb_;
    VoidCallback scheduleNextCb_;
};

#endif  // BATCHMANAGER_H
