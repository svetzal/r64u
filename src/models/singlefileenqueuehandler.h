/**
 * @file singlefileenqueuehandler.h
 * @brief Handler for single-file upload and download enqueue operations,
 *        and post-directory-creation file queuing.
 *
 * SingleFileEnqueueHandler encapsulates the logic for enqueueUpload(),
 * enqueueDownload(), and finishDirectoryCreation() that were previously part
 * of TransferManager. It operates on the shared transfer::State by reference
 * and emits Qt signals for all side-effects so that TransferManager can remain
 * the single owner of shared state while keeping enqueue logic isolated and
 * independently testable.
 */

#ifndef SINGLEFILEENQUEUEHANDLER_H
#define SINGLEFILEENQUEUEHANDLER_H

#include "services/transfercore.h"

#include <QObject>
#include <QString>

#include <functional>

class ILocalFileSystemService;

using transfer::OperationType;
using transfer::QueueState;

/**
 * @brief Handles single-file enqueue operations for uploads and downloads.
 *
 * The handler holds a non-owning reference to transfer::State (owned by
 * TransferManager) and must never outlive it. The createBatch factory is
 * kept as a std::function because it returns an int (factory pattern) and
 * is provided by TransferManager at construction time.
 *
 * All side-effects are emitted as signals so TransferManager can wire them
 * to the appropriate collaborators without this handler depending on them.
 */
class SingleFileEnqueueHandler : public QObject
{
    Q_OBJECT

public:
    using CreateBatchFn =
        std::function<int(OperationType, const QString &, const QString &, const QString &)>;

    /**
     * @brief Constructs the handler.
     * @param state Non-owning reference to the shared transfer state.
     * @param localFs Non-owning pointer to the local file system service.
     * @param parent Qt parent for memory management.
     */
    explicit SingleFileEnqueueHandler(transfer::State &state, ILocalFileSystemService *localFs,
                                      QObject *parent = nullptr);

    /**
     * @brief Sets the factory callback used to create new transfer batches.
     *
     * The callback returns the new batch's ID. It must remain valid for the
     * lifetime of this handler.
     */
    void setCreateBatchCallback(CreateBatchFn fn);

    /**
     * @brief Sets the callback invoked when an empty-folder upload batch should
     *        be completed without enqueuing any transfer items.
     *
     * The callback receives the batch ID. It must remain valid for the lifetime
     * of this handler.
     */
    void setCompleteBatchCallback(std::function<void(int)> cb);

    /**
     * @brief Updates the local file system pointer (e.g. after injection in tests).
     */
    void setLocalFileSystem(ILocalFileSystemService *fs);

    /**
     * @brief Enqueue a single file upload.
     *
     * If targetBatchId >= 0, appends to that batch. Otherwise, appends to the
     * active Upload batch or creates a new one.
     *
     * @param localPath  Absolute path on the local machine.
     * @param remotePath Absolute path on the remote device.
     * @param targetBatchId Batch to append to, or -1 to auto-select.
     */
    void enqueueUpload(const QString &localPath, const QString &remotePath, int targetBatchId = -1);

    /**
     * @brief Enqueue a single file download.
     *
     * If targetBatchId >= 0, appends to that batch. Otherwise, appends to the
     * active Download batch or creates a new one.
     *
     * @param remotePath    Absolute path on the remote device.
     * @param localPath     Absolute path on the local machine.
     * @param targetBatchId Batch to append to, or -1 to auto-select.
     */
    void enqueueDownload(const QString &remotePath, const QString &localPath,
                         int targetBatchId = -1);

public slots:
    /**
     * @brief Called after all remote directories have been created to queue the
     *        actual file upload items.
     *
     * Reads the list of local files from state_.currentFolderOp.sourcePath,
     * enqueues each as an upload into state_.currentFolderOp.batchId, then either
     * emits transitionToIdleRequested + scheduleProcessNextRequested (non-empty
     * folder) or invokes the completeBatch callback (empty folder).
     *
     * Corresponds to TransferManager::finishDirectoryCreation() prior to extraction.
     */
    void finishDirectoryCreation();

signals:
    /// Emitted before items are inserted into the model.
    void itemsAboutToBeInserted(int first, int last);
    /// Emitted after items have been inserted into the model.
    void itemsInserted();
    /// Emitted when the queue content changes.
    void queueChanged();
    /// Emitted when a new batch becomes active.
    void batchStarted(int batchId);
    /// Emitted when an enqueue operation fails.
    void operationFailed(const QString &fileName, const QString &error);
    /// Request TransferManager to schedule a processNext() call.
    void scheduleProcessNextRequested();
    /// Request TransferManager to transition the queue state to Idle.
    void transitionToIdleRequested();

private:
    [[nodiscard]] int findBatchIndex(int batchId) const;
    void activateAndSchedule(int batchIdx);

    transfer::State &state_;
    ILocalFileSystemService *localFs_ = nullptr;
    CreateBatchFn createBatchCb_;
    std::function<void(int)> completeBatchCb_;
};

#endif  // SINGLEFILEENQUEUEHANDLER_H
