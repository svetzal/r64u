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
/// Uses Qt signals for operations that belong to the owning TransferManager (model resets,
/// timeout stop, folder-op notification, and process scheduling). The setStopTimeoutCallback,
/// setFolderOpCompleteCallback, and setScheduleProcessNextCallback setters are kept for
/// unit tests that verify those behaviors without a full TransferManager.
class BatchManager : public QObject
{
    Q_OBJECT

public:
    using VoidCallback = std::function<void()>;
    using RowRangeCallback = std::function<void(int, int)>;

    explicit BatchManager(transfer::State &state, QObject *parent = nullptr);

    // Kept for purgeBatch unit tests that verify row-removal callbacks are interleaved correctly
    void setRowRemovalCallbacks(RowRangeCallback beginRemove, VoidCallback endRemove);

    // Batch lifecycle
    int createBatch(OperationType type, const QString &description, const QString &folderName,
                    const QString &sourcePath = QString());
    void activateNextBatch();
    void completeBatch(int batchId);
    void purgeBatch(int batchId);

    // Batch progress
    void emitBatchProgressAndComplete(int batchId, bool batchIsComplete,
                                      bool includeFailed = false);

    /// Check whether the batch for a skipped item is now complete and, if so,
    /// emit progress and complete it. Returns true when the batch was complete
    /// (caller should early-return its own handler), false otherwise.
    bool handleSkipBatchCompletion(int batchId);

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

    /// Model-notification signals — connect to TransferManager's model signals
    void modelAboutToReset();
    void modelReset();
    void rowsAboutToBeRemoved(int first, int last);
    void rowsRemoved();

    /// Orchestration signals — connect to TransferManager private slots
    void stopTimeoutRequested();
    void folderOpCompleteRequested();
    void scheduleProcessNextRequested();

private:
    transfer::State &state_;

    RowRangeCallback beginRemoveCb_;
    VoidCallback endRemoveCb_;
};

#endif  // BATCHMANAGER_H
