#include "transferftphandler.h"

#include "transfertimeoutmanager.h"

#include "../ftp/recursivescancoordinator.h"
#include "../ftp/remotedirectorycreator.h"
#include "../services/ftpclientmixin.h"

#include <QDebug>
#include <QFileInfo>

TransferFtpHandler::TransferFtpHandler(transfer::State &state, QObject *parent)
    : QObject(parent), state_(state)
{
}

void TransferFtpHandler::setFtpClient(IFtpClient *client)
{
    disconnectFtpClient(ftpClient_, this);

    ftpClient_ = client;

    if (ftpClient_) {
        connect(ftpClient_, &IFtpClient::uploadProgress, this,
                &TransferFtpHandler::onUploadProgress);
        connect(ftpClient_, &IFtpClient::uploadFinished, this,
                &TransferFtpHandler::onUploadFinished);
        connect(ftpClient_, &IFtpClient::downloadProgress, this,
                &TransferFtpHandler::onDownloadProgress);
        connect(ftpClient_, &IFtpClient::downloadFinished, this,
                &TransferFtpHandler::onDownloadFinished);
        connect(ftpClient_, &IFtpClient::error, this, &TransferFtpHandler::onFtpError);
        connect(ftpClient_, &IFtpClient::directoryCreated, this,
                &TransferFtpHandler::onFtpDirectoryCreated);
        connect(ftpClient_, &IFtpClient::directoryListed, this,
                &TransferFtpHandler::onDirectoryListed);
        connect(ftpClient_, &IFtpClient::fileRemoved, this, &TransferFtpHandler::onFileRemoved);
    }
}

void TransferFtpHandler::setTimeoutManager(TransferTimeoutManager *manager)
{
    timeoutManager_ = manager;
}

void TransferFtpHandler::setDirCreator(RemoteDirectoryCreator *creator)
{
    dirCreator_ = creator;
}

void TransferFtpHandler::setScanCoordinator(RecursiveScanCoordinator *coordinator)
{
    scanCoordinator_ = coordinator;
}

// ============================================================================
// Private helpers
// ============================================================================

void TransferFtpHandler::startTimeout()
{
    if (timeoutManager_) {
        timeoutManager_->start();
    }
}

void TransferFtpHandler::stopTimeout()
{
    if (timeoutManager_) {
        timeoutManager_->stop();
    }
}

void TransferFtpHandler::markCurrentComplete(transfer::TransferItem::Status status)
{
    if (state_.currentIndex < 0 || state_.currentIndex >= state_.items.size()) {
        return;
    }

    int completedIndex = state_.currentIndex;
    auto result = transfer::markItemComplete(state_, completedIndex, status);
    state_ = result.newState;

    emit itemDataChanged(completedIndex);

    if (result.batchId >= 0) {
        emit batchProgressRequested(result.batchId, result.batchIsComplete, false);
    }
}

// ============================================================================
// FTP callbacks
// ============================================================================

void TransferFtpHandler::onUploadProgress(const QString &file, qint64 sent, qint64 total)
{
    Q_UNUSED(file)
    startTimeout();

    if (state_.currentIndex >= 0 && state_.currentIndex < state_.items.size()) {
        state_.items[state_.currentIndex].bytesTransferred = sent;
        state_.items[state_.currentIndex].totalBytes = total;
        emit itemDataChanged(state_.currentIndex);
    }
}

void TransferFtpHandler::onUploadFinished(const QString &localPath, const QString &remotePath)
{
    stopTimeout();

    int idx = transfer::findItemIndex(state_, localPath, remotePath);
    if (idx >= 0) {
        state_.currentIndex = idx;
        markCurrentComplete(transfer::TransferItem::Status::Completed);

        QString fileName = QFileInfo(localPath).fileName();
        emit operationCompleted(fileName);
    }

    if (state_.queueState == transfer::QueueState::Transferring) {
        state_.queueState = transfer::QueueState::Idle;
    }
    emit queueChanged();
    emit scheduleProcessNextRequested();
}

void TransferFtpHandler::onDownloadProgress(const QString &file, qint64 received, qint64 total)
{
    Q_UNUSED(file)
    startTimeout();

    if (state_.currentIndex >= 0 && state_.currentIndex < state_.items.size()) {
        state_.items[state_.currentIndex].bytesTransferred = received;
        state_.items[state_.currentIndex].totalBytes = total;
        emit itemDataChanged(state_.currentIndex);
    }
}

void TransferFtpHandler::onDownloadFinished(const QString &remotePath, const QString &localPath)
{
    stopTimeout();

    int idx = transfer::findItemIndex(state_, localPath, remotePath);
    if (idx >= 0) {
        state_.currentIndex = idx;
        markCurrentComplete(transfer::TransferItem::Status::Completed);

        QString fileName = QFileInfo(remotePath).fileName();
        emit operationCompleted(fileName);
    }

    if (state_.queueState == transfer::QueueState::Transferring) {
        state_.queueState = transfer::QueueState::Idle;
    }
    emit queueChanged();
    emit scheduleProcessNextRequested();
}

void TransferFtpHandler::onFtpError(const QString &message)
{
    qDebug() << "TransferFtpHandler: onFtpError:" << message
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
    if (dirCreator_) {
        dirCreator_->onDirectoryCreated(path);
    }
}

void TransferFtpHandler::onDirectoryListed(const QString &path, const QList<FtpEntry> &entries)
{
    qDebug() << "TransferFtpHandler: onDirectoryListed:" << path << "entries:" << entries.size();

    if (scanCoordinator_ && scanCoordinator_->handlesListing(path)) {
        scanCoordinator_->onDirectoryListed(path, entries);
        return;
    }

    qDebug() << "TransferFtpHandler: Ignoring untracked listing for" << path;
}

void TransferFtpHandler::onFileRemoved(const QString &path)
{
    qDebug() << "TransferFtpHandler: onFileRemoved:" << path;

    if (state_.queueState == transfer::QueueState::Deleting &&
        state_.deletedCount < state_.deleteQueue.size()) {
        if (state_.deleteQueue[state_.deletedCount].path == path) {
            state_.deletedCount++;
            QString fileName = QFileInfo(path).fileName();
            emit deleteProgressUpdate(fileName, state_.deletedCount, state_.deleteQueue.size());
            emit queueChanged();
            emit processNextDeleteRequested();
            return;
        }
    }

    for (const auto &item : state_.items) {
        if (item.operationType == transfer::OperationType::Delete && item.remotePath == path &&
            item.status == transfer::TransferItem::Status::InProgress) {

            stopTimeout();
            markCurrentComplete(transfer::TransferItem::Status::Completed);

            QString fileName = QFileInfo(path).fileName();
            emit operationCompleted(fileName);

            state_.queueState = transfer::QueueState::Idle;
            emit queueChanged();
            emit scheduleProcessNextRequested();
            return;
        }
    }
}
