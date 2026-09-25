/**
 * @file singlefileenqueuehandler.cpp
 * @brief Implementation of SingleFileEnqueueHandler.
 */

#include "singlefileenqueuehandler.h"

#include "core/transferftpcore.h"
#include "services/ilocalfilesystemservice.h"
#include "utils/logging.h"

#include <QFileInfo>

#include <algorithm>

SingleFileEnqueueHandler::SingleFileEnqueueHandler(transfer::State &state,
                                                   ILocalFileSystemService *localFs,
                                                   QObject *parent)
    : QObject(parent), state_(state), localFs_(localFs)
{
}

void SingleFileEnqueueHandler::setCreateBatchCallback(CreateBatchFn fn)
{
    createBatchCb_ = std::move(fn);
}

void SingleFileEnqueueHandler::setCompleteBatchCallback(std::function<void(int)> cb)
{
    completeBatchCb_ = std::move(cb);
}

void SingleFileEnqueueHandler::setLocalFileSystem(ILocalFileSystemService *fs)
{
    localFs_ = fs;
}

// ============================================================================
// Private helpers
// ============================================================================

int SingleFileEnqueueHandler::findBatchIndex(int batchId) const
{
    for (int i = 0; i < state_.batches.size(); ++i) {
        if (state_.batches[i].batchId == batchId)
            return i;
    }
    return -1;
}

int SingleFileEnqueueHandler::joinableActiveBatchIndex(OperationType type) const
{
    const int batchIdx = state_.activeBatchIndex;
    if (batchIdx < 0 || batchIdx >= state_.batches.size()) {
        return -1;
    }
    const transfer::TransferBatch &batch = state_.batches[batchIdx];
    if (batch.operationType != type || batch.batchId == state_.currentFolderOp.batchId) {
        return -1;
    }
    return batchIdx;
}

void SingleFileEnqueueHandler::activateAndSchedule(int batchIdx)
{
    if (state_.activeBatchIndex < 0) {
        state_.activeBatchIndex = batchIdx;
        state_.batches[batchIdx].scanned = true;
        state_.batches[batchIdx].folderConfirmed = true;
        emit batchStarted(state_.batches[batchIdx].batchId);
    }
    emit queueChanged();
    // An active batch does not mean anything is running: a lost connection can
    // leave its items Pending with the queue Idle.
    if (!m_bulkQueuing && state_.queueState == QueueState::Idle)
        emit scheduleProcessNextRequested();
}

// ============================================================================
// Post-directory-creation file queuing
// ============================================================================

void SingleFileEnqueueHandler::finishDirectoryCreation()
{
    qCDebug(LogTransfer)
        << "SingleFileEnqueueHandler: All directories created, queueing files for upload";

    for (auto &b : state_.batches) {
        if (b.batchId == state_.currentFolderOp.batchId) {
            b.scanned = true;
            break;
        }
    }

    const QString &sourcePath = state_.currentFolderOp.sourcePath;
    if (!localFs_->directoryExists(sourcePath)) {
        qCWarning(LogTransfer) << "SingleFileEnqueueHandler: Source directory doesn't exist:"
                               << sourcePath;
        return;
    }

    const QStringList files = localFs_->listFilesRecursively(sourcePath);
    int fileCount = 0;
    m_bulkQueuing = true;
    for (const QString &filePath : files) {
        const QString relativePath = localFs_->relativePath(sourcePath, filePath);
        const QString remotePath = state_.currentFolderOp.targetPath + '/' + relativePath;

        enqueueUpload(filePath, remotePath, state_.currentFolderOp.batchId);
        fileCount++;
    }
    m_bulkQueuing = false;

    qCDebug(LogTransfer) << "SingleFileEnqueueHandler: Queued" << fileCount << "files for upload";

    if (fileCount == 0) {
        bool batchExists = std::any_of(state_.batches.cbegin(), state_.batches.cend(),
                                       [this](const transfer::TransferBatch &b) {
                                           return b.batchId == state_.currentFolderOp.batchId;
                                       });
        if (batchExists) {
            qCDebug(LogTransfer) << "SingleFileEnqueueHandler: Empty folder upload batch"
                                 << state_.currentFolderOp.batchId;
            if (completeBatchCb_)
                completeBatchCb_(state_.currentFolderOp.batchId);
            return;
        }
    }

    emit transitionToIdleRequested();
    emit scheduleProcessNextRequested();
}

// ============================================================================
// Single-file enqueue operations
// ============================================================================

void SingleFileEnqueueHandler::enqueueUpload(const QString &localPath, const QString &remotePath,
                                             int targetBatchId)
{
    int batchIdx = (targetBatchId >= 0) ? findBatchIndex(targetBatchId) : -1;

    if (batchIdx < 0) {
        batchIdx = joinableActiveBatchIndex(OperationType::Upload);
        if (batchIdx < 0) {
            QString fileName = QFileInfo(localPath).fileName();
            QString sourcePath = state_.currentFolderOp.sourcePath.isEmpty()
                                     ? QString()
                                     : state_.currentFolderOp.sourcePath;
            int batchId = createBatchCb_(OperationType::Upload, tr("Uploading %1").arg(fileName),
                                         fileName, sourcePath);
            batchIdx = findBatchIndex(batchId);
        }
    }

    if (batchIdx < 0 || batchIdx >= state_.batches.size()) {
        qCWarning(LogTransfer) << "SingleFileEnqueueHandler::enqueueUpload - no valid batch";
        emit operationFailed(QFileInfo(localPath).fileName(), tr("Failed to create upload batch"));
        return;
    }

    transfer::TransferItem item;
    item.localPath = localPath;
    item.remotePath = remotePath;
    item.operationType = OperationType::Upload;
    item.status = transfer::TransferItem::Status::Pending;
    item.totalBytes = localFs_->fileSize(localPath);
    item.batchId = state_.batches[batchIdx].batchId;

    emit itemsAboutToBeInserted(state_.items.size(), state_.items.size());
    auto enqResult = transfer::enqueueItem(state_, item, batchIdx);
    state_ = enqResult.newState;
    batchIdx = enqResult.batchIdx;
    emit itemsInserted();
    activateAndSchedule(batchIdx);
}

void SingleFileEnqueueHandler::enqueueDownload(const QString &remotePath, const QString &localPath,
                                               int targetBatchId, qint64 expectedSize)
{
    int batchIdx = -1;

    if (targetBatchId >= 0) {
        batchIdx = findBatchIndex(targetBatchId);
        if (batchIdx < 0) {
            qCWarning(LogTransfer)
                << "SingleFileEnqueueHandler::enqueueDownload - explicit batch not found:"
                << targetBatchId;
            emit operationFailed(QFileInfo(remotePath).fileName(),
                                 tr("Failed to create download batch"));
            return;
        }
    }

    if (batchIdx < 0) {
        batchIdx = joinableActiveBatchIndex(OperationType::Download);
        if (batchIdx < 0) {
            QString fileName = QFileInfo(remotePath).fileName();
            QString sourcePath = (state_.queueState == QueueState::Scanning)
                                     ? state_.currentFolderOp.sourcePath
                                     : QString();
            int batchId = createBatchCb_(OperationType::Download,
                                         tr("Downloading %1").arg(fileName), fileName, sourcePath);
            batchIdx = findBatchIndex(batchId);
        }
    }

    if (batchIdx < 0 || batchIdx >= state_.batches.size()) {
        qCWarning(LogTransfer) << "SingleFileEnqueueHandler::enqueueDownload - no valid batch";
        emit operationFailed(QFileInfo(remotePath).fileName(),
                             tr("Failed to create download batch"));
        return;
    }

    transfer::TransferItem item;
    item.localPath = localPath;
    item.remotePath = remotePath;
    item.operationType = OperationType::Download;
    item.status = transfer::TransferItem::Status::Pending;
    item.totalBytes = expectedSize;
    item.batchId = state_.batches[batchIdx].batchId;

    emit itemsAboutToBeInserted(state_.items.size(), state_.items.size());
    auto enqResult = transfer::enqueueItem(state_, item, batchIdx);
    state_ = enqResult.newState;
    batchIdx = enqResult.batchIdx;
    emit itemsInserted();
    activateAndSchedule(batchIdx);
}
