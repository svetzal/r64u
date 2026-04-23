#include "transferqueue.h"

#include "transferorchestrator.h"

TransferQueue::TransferQueue(QObject *parent)
    : QAbstractListModel(parent), orchestrator_(new TransferOrchestrator(this))
{
    TransferOrchestrator::ModelCallbacks callbacks;
    callbacks.beginInsertRows = [this](int first, int last) {
        beginInsertRows(QModelIndex(), first, last);
    };
    callbacks.endInsertRows = [this]() { endInsertRows(); };
    callbacks.dataChangedRow = [this](int row) { emit dataChanged(index(row), index(row)); };
    callbacks.beginResetModel = [this]() { beginResetModel(); };
    callbacks.endResetModel = [this]() { endResetModel(); };
    callbacks.beginRemoveRows = [this](int first, int last) {
        beginRemoveRows(QModelIndex(), first, last);
    };
    callbacks.endRemoveRows = [this]() { endRemoveRows(); };
    orchestrator_->setModelCallbacks(callbacks);

    connect(orchestrator_, &TransferOrchestrator::operationStarted, this,
            &TransferQueue::operationStarted);
    connect(orchestrator_, &TransferOrchestrator::operationCompleted, this,
            &TransferQueue::operationCompleted);
    connect(orchestrator_, &TransferOrchestrator::operationFailed, this,
            &TransferQueue::operationFailed);
    connect(orchestrator_, &TransferOrchestrator::allOperationsCompleted, this,
            &TransferQueue::allOperationsCompleted);
    connect(orchestrator_, &TransferOrchestrator::operationsCancelled, this,
            &TransferQueue::operationsCancelled);
    connect(orchestrator_, &TransferOrchestrator::queueChanged, this, &TransferQueue::queueChanged);
    connect(orchestrator_, &TransferOrchestrator::deleteProgressUpdate, this,
            &TransferQueue::deleteProgressUpdate);
    connect(orchestrator_, &TransferOrchestrator::overwriteConfirmationNeeded, this,
            &TransferQueue::overwriteConfirmationNeeded);
    connect(orchestrator_, &TransferOrchestrator::folderExistsConfirmationNeeded, this,
            &TransferQueue::folderExistsConfirmationNeeded);
    connect(orchestrator_, &TransferOrchestrator::batchStarted, this, &TransferQueue::batchStarted);
    connect(orchestrator_, &TransferOrchestrator::batchProgressUpdate, this,
            &TransferQueue::batchProgressUpdate);
    connect(orchestrator_, &TransferOrchestrator::batchCompleted, this,
            &TransferQueue::batchCompleted);
    connect(orchestrator_, &TransferOrchestrator::statusMessage, this,
            &TransferQueue::statusMessage);
    connect(orchestrator_, &TransferOrchestrator::scanningStarted, this,
            &TransferQueue::scanningStarted);
    connect(orchestrator_, &TransferOrchestrator::scanningProgress, this,
            &TransferQueue::scanningProgress);
    connect(orchestrator_, &TransferOrchestrator::directoryCreationProgress, this,
            &TransferQueue::directoryCreationProgress);
}

void TransferQueue::setFtpClient(IFtpClient *client)
{
    orchestrator_->setFtpClient(client);
}

void TransferQueue::setLocalFileSystem(ILocalFileSystem *fs)
{
    orchestrator_->setLocalFileSystem(fs);
}

void TransferQueue::flushEventQueue()
{
    orchestrator_->flushEventQueue();
}

void TransferQueue::enqueueUpload(const QString &localPath, const QString &remotePath,
                                  int targetBatchId)
{
    orchestrator_->enqueueUpload(localPath, remotePath, targetBatchId);
}

void TransferQueue::enqueueDownload(const QString &remotePath, const QString &localPath,
                                    int targetBatchId)
{
    orchestrator_->enqueueDownload(remotePath, localPath, targetBatchId);
}

void TransferQueue::enqueueRecursiveUpload(const QString &localDir, const QString &remoteDir)
{
    orchestrator_->enqueueRecursiveUpload(localDir, remoteDir);
}

void TransferQueue::enqueueRecursiveDownload(const QString &remoteDir, const QString &localDir)
{
    orchestrator_->enqueueRecursiveDownload(remoteDir, localDir);
}

void TransferQueue::enqueueDelete(const QString &remotePath, bool isDirectory)
{
    orchestrator_->enqueueDelete(remotePath, isDirectory);
}

void TransferQueue::enqueueRecursiveDelete(const QString &remotePath)
{
    orchestrator_->enqueueRecursiveDelete(remotePath);
}

void TransferQueue::clear()
{
    orchestrator_->clear();
}

void TransferQueue::removeCompleted()
{
    const transfer::State &state = orchestrator_->state();
    for (int i = state.items.size() - 1; i >= 0; --i) {
        if (state.items[i].status == TransferItem::Status::Completed ||
            state.items[i].status == TransferItem::Status::Failed ||
            state.items[i].status == TransferItem::Status::Skipped) {
            beginRemoveRows(QModelIndex(), i, i);
            // We need mutable access to remove the item; use a cast since this is a
            // controlled mutation through the model interface
            const_cast<transfer::State &>(state).items.removeAt(i);
            endRemoveRows();

            if (state.currentIndex > i) {
                const_cast<transfer::State &>(state).currentIndex--;
            } else if (state.currentIndex == i) {
                const_cast<transfer::State &>(state).currentIndex = -1;
            }
        }
    }
    emit queueChanged();
}

void TransferQueue::cancelAll()
{
    orchestrator_->cancelAll();
}

void TransferQueue::cancelBatch(int batchId)
{
    orchestrator_->cancelBatch(batchId);
}

void TransferQueue::respondToOverwrite(OverwriteResponse response)
{
    orchestrator_->respondToOverwrite(response);
}

void TransferQueue::respondToFolderExists(FolderExistsResponse response)
{
    orchestrator_->respondToFolderExists(response);
}

int TransferQueue::pendingCount() const
{
    return orchestrator_->pendingCount();
}

int TransferQueue::activeCount() const
{
    return orchestrator_->activeCount();
}

int TransferQueue::activeAndPendingCount() const
{
    return orchestrator_->activeAndPendingCount();
}

bool TransferQueue::isScanningForDelete() const
{
    return orchestrator_->isScanningForDelete();
}

bool TransferQueue::hasActiveBatch() const
{
    return orchestrator_->hasActiveBatch();
}

int TransferQueue::queuedBatchCount() const
{
    return orchestrator_->queuedBatchCount();
}

bool TransferQueue::isPathBeingTransferred(const QString &path, OperationType type) const
{
    return orchestrator_->isPathBeingTransferred(path, type);
}

int TransferQueue::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return orchestrator_->state().items.size();
}

QVariant TransferQueue::data(const QModelIndex &idx, int role) const
{
    const transfer::State &state = orchestrator_->state();
    if (!idx.isValid() || idx.row() >= state.items.size()) {
        return {};
    }
    return transfer::itemData(state.items[idx.row()], role);
}

QHash<int, QByteArray> TransferQueue::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[LocalPathRole] = "localPath";
    roles[RemotePathRole] = "remotePath";
    roles[OperationTypeRole] = "operationType";
    roles[StatusRole] = "status";
    roles[ProgressRole] = "progress";
    roles[BytesTransferredRole] = "bytesTransferred";
    roles[TotalBytesRole] = "totalBytes";
    roles[ErrorMessageRole] = "errorMessage";
    roles[FileNameRole] = "fileName";
    return roles;
}

BatchProgress TransferQueue::activeBatchProgress() const
{
    return orchestrator_->activeBatchProgress();
}

BatchProgress TransferQueue::batchProgress(int batchId) const
{
    return orchestrator_->batchProgress(batchId);
}

QList<int> TransferQueue::allBatchIds() const
{
    return orchestrator_->allBatchIds();
}
