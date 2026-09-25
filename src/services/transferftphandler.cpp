#include "transferftphandler.h"

#include "transfertimeoutmanager.h"

#include "../ftp/recursivescancoordinator.h"
#include "../ftp/remotedirectorycoordinator.h"
#include "../utils/logging.h"

#include <QFileInfo>

TransferFtpHandler::TransferFtpHandler(transfer::State &state, QObject *parent)
    : TransferHandlerBase(state, parent)
{
}

void TransferFtpHandler::setTimeoutManager(TransferTimeoutManager *manager)
{
    timeoutManager_ = manager;
}

void TransferFtpHandler::setDirCreator(RemoteDirectoryCoordinator *creator)
{
    dirCreator_ = creator;
}

void TransferFtpHandler::setScanCoordinator(RecursiveScanCoordinator *coordinator)
{
    scanCoordinator_ = coordinator;
}

void TransferFtpHandler::connectFtpSignals()
{
    connect(ftpClient_, &IFtpClient::uploadProgress, this, &TransferFtpHandler::onUploadProgress);
    connect(ftpClient_, &IFtpClient::uploadFinished, this, &TransferFtpHandler::onUploadFinished);
    connect(ftpClient_, &IFtpClient::downloadProgress, this,
            &TransferFtpHandler::onDownloadProgress);
    connect(ftpClient_, &IFtpClient::downloadFinished, this,
            &TransferFtpHandler::onDownloadFinished);
    connect(ftpClient_, &IFtpClient::operationFailed, this,
            &TransferFtpHandler::onFtpOperationFailed);
    connect(ftpClient_, &IFtpClient::directoryCreated, this,
            &TransferFtpHandler::onFtpDirectoryCreated);
    connect(ftpClient_, &IFtpClient::directoryListed, this, &TransferFtpHandler::onDirectoryListed);
    connect(ftpClient_, &IFtpClient::fileRemoved, this, &TransferFtpHandler::onFileRemoved);
}

// ============================================================================
// Private helpers
// ============================================================================

void TransferFtpHandler::startTimeout()
{
    if (!timeoutManager_) {
        qCWarning(LogTransfer) << "TransferFtpHandler: timeoutManager_ is null";
        return;
    }
    timeoutManager_->start();
}

void TransferFtpHandler::stopTimeout()
{
    if (!timeoutManager_) {
        qCWarning(LogTransfer) << "TransferFtpHandler: timeoutManager_ is null";
        return;
    }
    timeoutManager_->stop();
}

// ============================================================================
// FTP callbacks
// ============================================================================

void TransferFtpHandler::onUploadProgress(const QString &file, qint64 sent, qint64 total)
{
    const int idx = transfer::inFlightUploadIndex(state_, file);
    if (idx < 0) {
        return;  // Another component's upload on the shared client
    }
    startTimeout();

    state_.items[idx].bytesTransferred = sent;
    state_.items[idx].totalBytes = total;
    emit itemDataChanged(idx);
}

void TransferFtpHandler::onUploadFinished(const QString &localPath, const QString &remotePath)
{
    int idx = transfer::findItemIndex(state_, localPath, remotePath);
    if (idx < 0) {
        return;  // Not one of the queue's uploads
    }
    stopTimeout();

    state_.currentIndex = idx;
    markCurrentComplete(transfer::TransferItem::Status::Completed);
    emit operationCompleted(QFileInfo(localPath).fileName());

    completeAndNotify();
}

void TransferFtpHandler::onDownloadProgress(const QString &file, qint64 received, qint64 total)
{
    const int idx = transfer::inFlightDownloadIndex(state_, file);
    if (idx < 0) {
        return;  // e.g. a preview's downloadToMemory on the shared client
    }
    startTimeout();

    state_.items[idx].bytesTransferred = received;
    state_.items[idx].totalBytes = total;
    emit itemDataChanged(idx);
}

void TransferFtpHandler::onDownloadFinished(const QString &remotePath, const QString &localPath)
{
    int idx = transfer::findItemIndex(state_, localPath, remotePath);
    if (idx < 0) {
        return;  // Not one of the queue's downloads
    }
    stopTimeout();

    state_.currentIndex = idx;
    markCurrentComplete(transfer::TransferItem::Status::Completed);
    emit operationCompleted(QFileInfo(remotePath).fileName());

    completeAndNotify();
}

bool TransferFtpHandler::isQueueRequest(IFtpClient::Operation operation, const QString &remotePath,
                                        const QString &localPath) const
{
    using Operation = IFtpClient::Operation;
    using transfer::OperationType;

    switch (operation) {
    case Operation::Download:
        return transfer::isInFlightItem(state_, OperationType::Download, remotePath, localPath);
    case Operation::Upload:
        return transfer::isInFlightItem(state_, OperationType::Upload, remotePath, localPath);
    case Operation::Remove:
    case Operation::RemoveDirectory:
        return transfer::isInFlightItem(state_, OperationType::Delete, remotePath, localPath) ||
               transfer::isAwaitedRecursiveDelete(state_, remotePath);
    case Operation::List:
        return transfer::isAwaitedListing(state_, remotePath);
    case Operation::MakeDirectory:
        return transfer::isAwaitedMkdir(state_, remotePath);
    case Operation::ChangeDirectory:
    case Operation::DownloadToMemory:
    case Operation::Rename:
        return false;
    }
    return false;
}

void TransferFtpHandler::onFtpOperationFailed(IFtpClient::Operation operation,
                                              const QString &remotePath, const QString &localPath,
                                              const QString &message)
{
    if (!isQueueRequest(operation, remotePath, localPath)) {
        qCDebug(LogTransfer) << "TransferFtpHandler: Ignoring failure of another component's"
                             << operation << remotePath;
        return;
    }
    handleQueueRequestFailure(message);
}

void TransferFtpHandler::handleQueueRequestFailure(const QString &message)
{
    qCDebug(LogTransfer) << "TransferFtpHandler: queue request failed:" << message
                         << "state:" << transfer::queueStateToString(state_.queueState);

    stopTimeout();

    int originalIndex = state_.currentIndex;

    auto result = transfer::handleFtpError(state_, message);
    state_ = result.newState;

    if (result.isDeleteError) {
        emit operationFailed(result.deleteFileName, message);
        emit queueChanged();
        emit processNextDeleteRequested();
        return;
    }

    if (result.isFolderCreationError) {
        emit operationFailed(result.folderName, message);
        emit completeBatchRequested(result.folderBatchId);
        return;
    }

    if (result.hasCurrentItem && originalIndex >= 0 && originalIndex < state_.items.size()) {
        emit itemDataChanged(originalIndex);
        emit operationFailed(result.transferFileName, message);

        if (result.failedBatchId >= 0) {
            emit batchProgressRequested(result.failedBatchId, result.batchIsComplete, true);
            if (result.batchIsComplete) {
                return;
            }
        }
    }

    emit queueChanged();
    if (result.shouldScheduleProcessNext) {
        emit scheduleProcessNextRequested();
    }
}

void TransferFtpHandler::onFtpDirectoryCreated(const QString &path)
{
    if (!transfer::isAwaitedMkdir(state_, path)) {
        return;  // e.g. a folder created from the remote browser
    }
    if (dirCreator_) {
        dirCreator_->onDirectoryCreated(path);
    }
}

void TransferFtpHandler::onDirectoryListed(const QString &path, const QList<FtpEntry> &entries)
{
    qCDebug(LogTransfer) << "TransferFtpHandler: onDirectoryListed:" << path
                         << "entries:" << entries.size();

    if (scanCoordinator_ && scanCoordinator_->handlesListing(path)) {
        scanCoordinator_->onDirectoryListed(path, entries);
        return;
    }

    qCDebug(LogTransfer) << "TransferFtpHandler: Ignoring untracked listing for" << path;
}

void TransferFtpHandler::onFileRemoved(const QString &path)
{
    qCDebug(LogTransfer) << "TransferFtpHandler: onFileRemoved:" << path;

    // Try recursive delete progress first (updates deletedCount, no item state change)
    auto deleteResult = transfer::advanceDeleteProgress(state_, path);
    if (deleteResult.advanced) {
        state_ = deleteResult.newState;
        emit deleteProgressUpdate(deleteResult.fileName, deleteResult.currentCount,
                                  deleteResult.totalCount);
        emit queueChanged();
        emit processNextDeleteRequested();
        return;
    }

    // Try individual delete item completion (single-file delete, not recursive)
    auto findResult = transfer::findInProgressDeleteItem(state_, path);
    if (findResult.found) {
        stopTimeout();
        state_.currentIndex = findResult.itemIndex;
        markCurrentComplete(transfer::TransferItem::Status::Completed);

        emit operationCompleted(findResult.fileName);

        completeAndNotify();
        return;
    }
}
