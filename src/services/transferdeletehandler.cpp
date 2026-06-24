#include "transferdeletehandler.h"

#include "ftp/recursivescancoordinator.h"
#include "ftp/remotedirectorycoordinator.h"
#include "utils/logging.h"

#include <QFileInfo>

TransferDeleteHandler::TransferDeleteHandler(transfer::State &state, QObject *parent)
    : TransferHandlerBase(state, parent)
{
}

void TransferDeleteHandler::setScanCoordinator(RecursiveScanCoordinator *coordinator)
{
    scanCoordinator_ = coordinator;
}

void TransferDeleteHandler::setDirCreator(RemoteDirectoryCoordinator *creator)
{
    dirCreator_ = creator;
}

void TransferDeleteHandler::setCreateBatchCallback(CreateBatchFn fn)
{
    createBatchCb_ = std::move(fn);
}

void TransferDeleteHandler::enqueueDelete(const QString &remotePath, bool isDirectory)
{
    int batchIdx = state_.activeBatchIndex;
    if (batchIdx < 0 || state_.batches[batchIdx].operationType != OperationType::Delete) {
        QString fileName = QFileInfo(remotePath).fileName();
        QString sourcePath =
            !state_.recursiveDeleteBase.isEmpty() ? state_.recursiveDeleteBase : QString();
        int batchId = createBatchCb_(OperationType::Delete, tr("Deleting %1").arg(fileName),
                                     fileName, sourcePath);
        for (int i = 0; i < state_.batches.size(); ++i) {
            if (state_.batches[i].batchId == batchId) {
                batchIdx = i;
                break;
            }
        }
    }

    if (batchIdx < 0 || batchIdx >= state_.batches.size()) {
        qCWarning(LogTransfer) << "TransferDeleteHandler::enqueueDelete - no valid batch";
        emit operationFailed(QFileInfo(remotePath).fileName(), tr("Failed to create delete batch"));
        return;
    }

    transfer::TransferItem item;
    item.remotePath = remotePath;
    item.operationType = OperationType::Delete;
    item.status = transfer::TransferItem::Status::Pending;
    item.isDirectory = isDirectory;
    item.batchId = state_.batches[batchIdx].batchId;

    emit rowsAboutToBeInserted(state_.items.size(), state_.items.size());
    auto enqResult = transfer::enqueueItem(state_, item, batchIdx);
    state_ = enqResult.newState;
    batchIdx = enqResult.batchIdx;
    emit rowsInserted();

    if (state_.activeBatchIndex < 0) {
        state_.activeBatchIndex = batchIdx;
        state_.batches[batchIdx].scanned = true;
        state_.batches[batchIdx].folderConfirmed = true;
        emit batchStarted(state_.batches[batchIdx].batchId);
    }

    emit queueChanged();

    if (state_.queueState == QueueState::Idle) {
        emit scheduleProcessNextRequested();
    }
}

void TransferDeleteHandler::enqueueRecursiveDelete(const QString &remotePath)
{
    if (!ftpConnected()) {
        qCWarning(LogTransfer) << "enqueueRecursiveDelete skipped: FTP not connected";
        emit operationFailed(QFileInfo(remotePath).fileName(), tr("Not connected to device"));
        return;
    }

    QString normalizedPath = transfer::normalizePath(remotePath);

    if (transfer::isPathBeingTransferred(state_, normalizedPath, OperationType::Delete)) {
        qCDebug(LogTransfer) << "TransferDeleteHandler: Ignoring duplicate delete request for"
                             << normalizedPath;
        emit statusMessage(
            tr("'%1' is already being deleted").arg(QFileInfo(normalizedPath).fileName()));
        return;
    }

    state_.deleteQueue.clear();
    state_.deletedCount = 0;
    state_.recursiveDeleteBase = normalizedPath;

    emit queueChanged();

    emit transitionToRequested(QueueState::Scanning);
    scanCoordinator_->startDeleteScan(normalizedPath);
}

void TransferDeleteHandler::processNextDelete()
{
    if (!ftpConnected()) {
        qCWarning(LogTransfer) << "processNextDelete: FTP disconnected, resetting to Idle";
        emit transitionToRequested(QueueState::Idle);
        emit operationFailed(QString(), tr("Not connected to device"));
        return;
    }

    auto decision = transfer::decideNextDeleteAction(state_);

    switch (decision.action) {
    case transfer::NextDeleteAction::AllDone:
        qCDebug(LogTransfer) << "TransferDeleteHandler: All deletes complete";
        emit transitionToRequested(QueueState::Idle);
        state_.deleteQueue.clear();
        state_.recursiveDeleteBase.clear();
        emit operationCompleted(tr("Deleted %1 items").arg(decision.completedCount));
        emit allOperationsCompleted();
        return;

    case transfer::NextDeleteAction::PendingUploadReady:
        qCDebug(LogTransfer) << "TransferDeleteHandler: Delete completed, starting pending upload";
        emit transitionToRequested(QueueState::Idle);
        state_.deleteQueue.clear();
        state_.recursiveDeleteBase.clear();
        emit operationCompleted(tr("Deleted %1 items").arg(decision.completedCount));
        state_.pendingUploadAfterDelete = false;
        emit startDirectoryCreationAfterDeleteRequested();
        return;

    case transfer::NextDeleteAction::DispatchNext: {
        const transfer::DeleteItem &item = decision.nextItem;
        qCDebug(LogTransfer) << "TransferDeleteHandler: Deleting" << (state_.deletedCount + 1)
                             << "of" << state_.deleteQueue.size() << ":" << item.path;
        if (item.isDirectory) {
            ftpClient_->removeDirectory(item.path);
        } else {
            ftpClient_->remove(item.path);
        }
        return;
    }
    }
}
