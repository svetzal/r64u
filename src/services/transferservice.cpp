#include "transferservice.h"

#include "deviceconnectionmanager.h"
#include "ierroremitter.h"

#include <QFileInfo>

TransferService::TransferService(DeviceConnectionManager *connection, TransferQueue *queue,
                                 QObject *parent)
    : IErrorEmitter(parent), connection_(connection), queue_(queue)
{
    setupSignalForwarding();
    // Forward operationFailed to the uniform IErrorEmitter signal
    connect(this, &TransferService::operationFailed, this,
            [this](const QString &fileName, const QString &error) {
                emit errorReported(ErrorCategory::FileOperation, ErrorSeverity::Warning,
                                   tr("%1 failed").arg(fileName), error);
            });
}

void TransferService::setupSignalForwarding()
{
    connect(queue_, &TransferQueue::operationStarted, this, &TransferService::operationStarted);
    connect(queue_, &TransferQueue::operationCompleted, this, &TransferService::operationCompleted);
    connect(queue_, &TransferQueue::operationFailed, this, &TransferService::operationFailed);
    connect(queue_, &TransferQueue::allOperationsCompleted, this,
            &TransferService::allOperationsCompleted);
    connect(queue_, &TransferQueue::operationsCancelled, this,
            &TransferService::operationsCancelled);
    connect(queue_, &TransferQueue::queueChanged, this, &TransferService::queueChanged);
    connect(queue_, &TransferQueue::deleteProgressUpdate, this,
            &TransferService::deleteProgressUpdate);
    connect(queue_, &TransferQueue::overwriteConfirmationNeeded, this,
            &TransferService::overwriteConfirmationNeeded);
    connect(queue_, &TransferQueue::folderExistsConfirmationNeeded, this,
            &TransferService::folderExistsConfirmationNeeded);
    connect(queue_, &TransferQueue::batchStarted, this, &TransferService::batchStarted);
    connect(queue_, &TransferQueue::batchProgressUpdate, this,
            &TransferService::batchProgressUpdate);
    connect(queue_, &TransferQueue::batchCompleted, this, &TransferService::batchCompleted);
    connect(queue_, &TransferQueue::statusMessage, this, &TransferService::statusMessage);
    connect(queue_, &TransferQueue::scanningStarted, this, &TransferService::scanningStarted);
    connect(queue_, &TransferQueue::scanningProgress, this, &TransferService::scanningProgress);
    connect(queue_, &TransferQueue::directoryCreationProgress, this,
            &TransferService::directoryCreationProgress);
}

TransferService::~TransferService() = default;

bool TransferService::requiresConnection(const QString &path)
{
    if (!connection_->isConnected()) {
        emit operationFailed(QFileInfo(path).fileName(), tr("Not connected to device"));
        return false;
    }
    return true;
}

bool TransferService::uploadFile(const QString &localPath, const QString &remoteDir)
{
    if (!requiresConnection(localPath))
        return false;

    QFileInfo fileInfo(localPath);
    QString targetDir = remoteDir;
    if (!targetDir.endsWith('/')) {
        targetDir += '/';
    }
    QString remotePath = targetDir + fileInfo.fileName();

    queue_->enqueueUpload(localPath, remotePath);
    emit statusMessage(tr("Queued upload: %1 -> %2").arg(fileInfo.fileName(), remoteDir));
    return true;
}

bool TransferService::uploadDirectory(const QString &localDir, const QString &remoteDir)
{
    if (!requiresConnection(localDir))
        return false;

    QFileInfo fileInfo(localDir);
    queue_->enqueueRecursiveUpload(localDir, remoteDir);
    emit statusMessage(tr("Queued folder upload: %1 -> %2").arg(fileInfo.fileName(), remoteDir));
    return true;
}

bool TransferService::downloadFile(const QString &remotePath, const QString &localDir)
{
    if (!requiresConnection(remotePath))
        return false;

    QString fileName = QFileInfo(remotePath).fileName();
    QString localPath = localDir + "/" + fileName;
    queue_->enqueueDownload(remotePath, localPath);
    emit statusMessage(tr("Queued download: %1 -> %2").arg(fileName, localDir));
    return true;
}

bool TransferService::downloadDirectory(const QString &remoteDir, const QString &localDir)
{
    if (!requiresConnection(remoteDir))
        return false;

    QString folderName = QFileInfo(remoteDir).fileName();
    queue_->enqueueRecursiveDownload(remoteDir, localDir);
    emit statusMessage(tr("Queued folder download: %1 -> %2").arg(folderName, localDir));
    return true;
}

bool TransferService::deleteRemote(const QString &remotePath, bool isDirectory)
{
    if (!requiresConnection(remotePath))
        return false;

    QString fileName = QFileInfo(remotePath).fileName();
    queue_->enqueueDelete(remotePath, isDirectory);
    emit statusMessage(tr("Queued delete: %1").arg(fileName));
    return true;
}

bool TransferService::deleteRecursive(const QString &remotePath)
{
    if (!requiresConnection(remotePath))
        return false;

    QString fileName = QFileInfo(remotePath).fileName();
    queue_->enqueueRecursiveDelete(remotePath);
    emit statusMessage(tr("Queued folder delete: %1").arg(fileName));
    return true;
}

void TransferService::cancelAll()
{
    queue_->cancelAll();
}

void TransferService::cancelBatch(int batchId)
{
    queue_->cancelBatch(batchId);
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

BatchProgress TransferService::activeBatchProgress() const
{
    return queue_->activeBatchProgress();
}

BatchProgress TransferService::batchProgress(int batchId) const
{
    return queue_->batchProgress(batchId);
}

QList<int> TransferService::allBatchIds() const
{
    return queue_->allBatchIds();
}

bool TransferService::hasActiveBatch() const
{
    return queue_->hasActiveBatch();
}
