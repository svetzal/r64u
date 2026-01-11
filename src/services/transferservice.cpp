#include "transferservice.h"
#include "deviceconnection.h"

#include <QFileInfo>

TransferService::TransferService(DeviceConnection *connection,
                                 TransferQueue *queue,
                                 QObject *parent)
    : QObject(parent)
    , connection_(connection)
    , queue_(queue)
{
    // Forward signals from TransferQueue
    connect(queue_, &TransferQueue::operationStarted,
            this, &TransferService::operationStarted);
    connect(queue_, &TransferQueue::operationCompleted,
            this, &TransferService::operationCompleted);
    connect(queue_, &TransferQueue::operationFailed,
            this, &TransferService::operationFailed);
    connect(queue_, &TransferQueue::allOperationsCompleted,
            this, &TransferService::allOperationsCompleted);
    connect(queue_, &TransferQueue::operationsCancelled,
            this, &TransferService::operationsCancelled);
    connect(queue_, &TransferQueue::queueChanged,
            this, &TransferService::queueChanged);
    connect(queue_, &TransferQueue::deleteProgressUpdate,
            this, &TransferService::deleteProgressUpdate);
    connect(queue_, &TransferQueue::overwriteConfirmationNeeded,
            this, &TransferService::overwriteConfirmationNeeded);
    connect(queue_, &TransferQueue::folderExistsConfirmationNeeded,
            this, &TransferService::folderExistsConfirmationNeeded);
}

TransferService::~TransferService() = default;

bool TransferService::uploadFile(const QString &localPath, const QString &remoteDir)
{
    if (!connection_->isConnected()) {
        return false;
    }

    QFileInfo fileInfo(localPath);
    QString targetDir = remoteDir;
    if (!targetDir.endsWith('/')) {
        targetDir += '/';
    }
    QString remotePath = targetDir + fileInfo.fileName();

    queue_->enqueueUpload(localPath, remotePath);
    emit statusMessage(tr("Queued upload: %1 -> %2").arg(fileInfo.fileName(), remoteDir), 3000);
    return true;
}

bool TransferService::uploadDirectory(const QString &localDir, const QString &remoteDir)
{
    if (!connection_->isConnected()) {
        return false;
    }

    QFileInfo fileInfo(localDir);
    queue_->enqueueRecursiveUpload(localDir, remoteDir);
    emit statusMessage(tr("Queued folder upload: %1 -> %2").arg(fileInfo.fileName(), remoteDir), 3000);
    return true;
}

bool TransferService::downloadFile(const QString &remotePath, const QString &localDir)
{
    if (!connection_->isConnected()) {
        return false;
    }

    QString fileName = QFileInfo(remotePath).fileName();
    QString localPath = localDir + "/" + fileName;
    queue_->enqueueDownload(remotePath, localPath);
    emit statusMessage(tr("Queued download: %1 -> %2").arg(fileName, localDir), 3000);
    return true;
}

bool TransferService::downloadDirectory(const QString &remoteDir, const QString &localDir)
{
    if (!connection_->isConnected()) {
        return false;
    }

    QString folderName = QFileInfo(remoteDir).fileName();
    queue_->enqueueRecursiveDownload(remoteDir, localDir);
    emit statusMessage(tr("Queued folder download: %1 -> %2").arg(folderName, localDir), 3000);
    return true;
}

bool TransferService::deleteRemote(const QString &remotePath, bool isDirectory)
{
    if (!connection_->isConnected()) {
        return false;
    }

    QString fileName = QFileInfo(remotePath).fileName();
    queue_->enqueueDelete(remotePath, isDirectory);
    emit statusMessage(tr("Queued delete: %1").arg(fileName), 3000);
    return true;
}

bool TransferService::deleteRecursive(const QString &remotePath)
{
    if (!connection_->isConnected()) {
        return false;
    }

    QString fileName = QFileInfo(remotePath).fileName();
    queue_->enqueueRecursiveDelete(remotePath);
    emit statusMessage(tr("Queued folder delete: %1").arg(fileName), 3000);
    return true;
}

void TransferService::cancelAll()
{
    queue_->cancelAll();
}

void TransferService::removeCompleted()
{
    queue_->removeCompleted();
}

void TransferService::clear()
{
    queue_->clear();
}

bool TransferService::isProcessing() const
{
    return queue_->isProcessing();
}

bool TransferService::isScanning() const
{
    return queue_->isScanning();
}

bool TransferService::isProcessingDelete() const
{
    return queue_->isProcessingDelete();
}

bool TransferService::isCreatingDirectories() const
{
    return queue_->isCreatingDirectories();
}

int TransferService::pendingCount() const
{
    return queue_->pendingCount();
}

int TransferService::activeCount() const
{
    return queue_->activeCount();
}

int TransferService::activeAndPendingCount() const
{
    return queue_->activeAndPendingCount();
}

int TransferService::totalCount() const
{
    return queue_->rowCount();
}

int TransferService::deleteProgress() const
{
    return queue_->deleteProgress();
}

int TransferService::deleteTotalCount() const
{
    return queue_->deleteTotalCount();
}

bool TransferService::isScanningForDelete() const
{
    return queue_->isScanningForDelete();
}

void TransferService::respondToOverwrite(OverwriteResponse response)
{
    queue_->respondToOverwrite(response);
}

void TransferService::setAutoOverwrite(bool autoOverwrite)
{
    queue_->setAutoOverwrite(autoOverwrite);
}

void TransferService::respondToFolderExists(FolderExistsResponse response)
{
    queue_->respondToFolderExists(response);
}

void TransferService::setAutoMerge(bool autoMerge)
{
    queue_->setAutoMerge(autoMerge);
}
