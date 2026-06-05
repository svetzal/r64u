#include "transferqueue.h"

#include "services/transfermanager.h"

TransferQueue::TransferQueue(QObject *parent)
    : QAbstractListModel(parent), orchestrator_(new TransferManager(this))
{
    connect(orchestrator_, &TransferManager::itemsAboutToBeInserted, this,
            [this](int first, int last) { beginInsertRows(QModelIndex(), first, last); });
    connect(orchestrator_, &TransferManager::itemsInserted, this, [this]() { endInsertRows(); });
    connect(orchestrator_, &TransferManager::itemDataChanged, this,
            [this](int row) { emit dataChanged(index(row), index(row)); });
    connect(orchestrator_, &TransferManager::modelAboutToReset, this,
            [this]() { beginResetModel(); });
    connect(orchestrator_, &TransferManager::modelReset, this, [this]() { endResetModel(); });
    connect(orchestrator_, &TransferManager::itemsAboutToBeRemoved, this,
            [this](int first, int last) { beginRemoveRows(QModelIndex(), first, last); });
    connect(orchestrator_, &TransferManager::itemsRemoved, this, [this]() { endRemoveRows(); });
    setupSignalForwarding();
}

void TransferQueue::setupSignalForwarding()
{
    connect(orchestrator_, &TransferManager::operationStarted, this,
            &TransferQueue::operationStarted);
    connect(orchestrator_, &TransferManager::operationCompleted, this,
            &TransferQueue::operationCompleted);
    connect(orchestrator_, &TransferManager::operationFailed, this,
            &TransferQueue::operationFailed);
    connect(orchestrator_, &TransferManager::allOperationsCompleted, this,
            &TransferQueue::allOperationsCompleted);
    connect(orchestrator_, &TransferManager::operationsCancelled, this,
            &TransferQueue::operationsCancelled);
    connect(orchestrator_, &TransferManager::queueChanged, this, &TransferQueue::queueChanged);
    connect(orchestrator_, &TransferManager::deleteProgressUpdate, this,
            &TransferQueue::deleteProgressUpdate);
    connect(orchestrator_, &TransferManager::overwriteConfirmationNeeded, this,
            &TransferQueue::overwriteConfirmationNeeded);
    connect(orchestrator_, &TransferManager::folderExistsConfirmationNeeded, this,
            &TransferQueue::folderExistsConfirmationNeeded);
    connect(orchestrator_, &TransferManager::batchStarted, this, &TransferQueue::batchStarted);
    connect(orchestrator_, &TransferManager::batchProgressUpdate, this,
            &TransferQueue::batchProgressUpdate);
    connect(orchestrator_, &TransferManager::batchCompleted, this, &TransferQueue::batchCompleted);
    connect(orchestrator_, &TransferManager::statusMessage, this, &TransferQueue::statusMessage);
    connect(orchestrator_, &TransferManager::scanningStarted, this,
            &TransferQueue::scanningStarted);
    connect(orchestrator_, &TransferManager::scanningProgress, this,
            &TransferQueue::scanningProgress);
    connect(orchestrator_, &TransferManager::directoryCreationProgress, this,
            &TransferQueue::directoryCreationProgress);
}

void TransferQueue::setFtpClient(IFtpClient *client)
{
    orchestrator_->setFtpClient(client);
}

void TransferQueue::setLocalFileSystem(ILocalFileSystemService *fs)
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
    return orchestrator_->batchManager()->hasActiveBatch();
}

int TransferQueue::queuedBatchCount() const
{
    return orchestrator_->batchManager()->queuedBatchCount();
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
    return orchestrator_->batchManager()->activeBatchProgress();
}

BatchProgress TransferQueue::batchProgress(int batchId) const
{
    return orchestrator_->batchManager()->batchProgress(batchId);
}

QList<int> TransferQueue::allBatchIds() const
{
    return orchestrator_->batchManager()->allBatchIds();
}
