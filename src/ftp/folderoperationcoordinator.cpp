/**
 * @file folderoperationcoordinator.cpp
 * @brief Implementation of FolderOperationCoordinator.
 */

#include "folderoperationcoordinator.h"

#include "services/iftpclient.h"
#include "services/ilocalfilesystem.h"

#include <QDebug>
#include <QFileInfo>
#include <QTimer>

FolderOperationCoordinator::FolderOperationCoordinator(transfer::State &state,
                                                       IFtpClient *ftpClient,
                                                       ILocalFileSystem *localFs, QObject *parent)
    : QObject(parent), state_(state), ftpClient_(ftpClient), localFs_(localFs),
      debounceTimer_(new QTimer(this))
{
    debounceTimer_->setSingleShot(true);
    connect(debounceTimer_, &QTimer::timeout, this, &FolderOperationCoordinator::onDebounceTimeout);
}

void FolderOperationCoordinator::setFtpClient(IFtpClient *client)
{
    ftpClient_ = client;
}

void FolderOperationCoordinator::setLocalFileSystem(ILocalFileSystem *fs)
{
    localFs_ = fs;
}

void FolderOperationCoordinator::setCreateBatchCallback(
    std::function<int(transfer::OperationType, const QString &, const QString &, const QString &)>
        callback)
{
    createBatchCallback_ = std::move(callback);
}

void FolderOperationCoordinator::enqueueRecursive(transfer::OperationType type,
                                                  const QString &sourcePath,
                                                  const QString &destPath)
{
    const bool isUpload = (type == transfer::OperationType::Upload);
    const QString verb = isUpload ? tr("uploaded") : tr("downloaded");

    if (transfer::isPathBeingTransferred(state_, sourcePath, type)) {
        qDebug() << "FolderOperationCoordinator: Ignoring duplicate"
                 << (isUpload ? "upload" : "download") << "request for" << sourcePath;
        emit statusMessage(
            tr("'%1' is already being %2").arg(QFileInfo(sourcePath).fileName(), verb), 3000);
        return;
    }

    QString folderName = QFileInfo(sourcePath).fileName();
    QString targetDir = destPath;
    if (!targetDir.endsWith('/'))
        targetDir += '/';
    targetDir += folderName;

    transfer::PendingFolderOp op;
    op.operationType = type;
    op.sourcePath = sourcePath;
    op.destPath = destPath;
    op.targetPath = targetDir;
    // For downloads: query local existence via gateway; uploads never have local destExists
    op.destExists = isUpload ? false : (localFs_ ? localFs_->directoryExists(targetDir) : false);

    // Skip confirmation when: autoMerge is set, or (download and dest doesn't exist yet)
    const bool skipConfirmation = state_.autoMerge || (!isUpload && !op.destExists);
    if (skipConfirmation) {
        if (state_.queueState == transfer::QueueState::Idle) {
            // New user operation starting - reset overwrite preference
            state_.overwriteAll = false;
            startFolderOperation(op);
        } else {
            state_.pendingFolderOps.enqueue(op);
        }
        return;
    }

    // Queue for debounce and folder existence check / confirmation
    state_.pendingFolderOps.enqueue(op);

    if (state_.queueState == transfer::QueueState::Idle) {
        // New user operation starting - reset overwrite preference
        state_.overwriteAll = false;
        state_.queueState = transfer::QueueState::CollectingItems;
        debounceTimer_->start(DebounceMs);
    }
}

void FolderOperationCoordinator::respondToFolderExists(transfer::FolderExistsResponse response)
{
    auto result = transfer::respondToFolderExists(state_, response);
    state_ = result.newState;

    if (result.shouldCancelFolderOps) {
        emit operationsCancelled();
        return;
    }

    if (result.shouldStartFolderOp) {
        startFolderOperation(result.folderOpToStart);
    }
}

void FolderOperationCoordinator::resumeAfterFolderCheck()
{
    checkFolderConfirmation();
}

void FolderOperationCoordinator::stopDebounce()
{
    debounceTimer_->stop();
}

void FolderOperationCoordinator::startNextPendingFolderOp()
{
    if (!state_.pendingFolderOps.isEmpty()) {
        transfer::PendingFolderOp op = state_.pendingFolderOps.dequeue();
        startFolderOperation(op);
    }
}

void FolderOperationCoordinator::onFolderOperationComplete()
{
    qDebug() << "FolderOperationCoordinator: Folder operation complete:"
             << state_.currentFolderOp.targetPath;

    state_.currentFolderOp = transfer::PendingFolderOp();

    // Process next pending folder operation if any
    if (!state_.pendingFolderOps.isEmpty()) {
        transfer::PendingFolderOp op = state_.pendingFolderOps.dequeue();
        startFolderOperation(op);
        return;
    }

    // All folders done
    qDebug() << "FolderOperationCoordinator: All folder operations complete";
    state_.replaceExisting = false;
    emit allOperationsCompleted();
}

void FolderOperationCoordinator::onDebounceTimeout()
{
    qDebug() << "FolderOperationCoordinator: Debounce timeout, processing"
             << state_.pendingFolderOps.size() << "pending folder ops";

    if (state_.pendingFolderOps.isEmpty()) {
        state_.queueState = transfer::QueueState::Idle;
        return;
    }

    // For uploads, we need to check remote folder existence
    // For downloads, we already know local folder existence
    transfer::PendingFolderOp &firstOp = state_.pendingFolderOps.head();

    if (firstOp.operationType == transfer::OperationType::Upload) {
        // Need to list the remote directory to check if target exists
        state_.requestedFolderCheckListings.insert(firstOp.destPath);
        if (ftpClient_) {
            ftpClient_->list(firstOp.destPath);
        }
    } else {
        // Downloads: check if any folders exist and need confirmation
        checkFolderConfirmation();
    }
}

void FolderOperationCoordinator::checkFolderConfirmation()
{
    auto result = transfer::checkFolderConfirmation(state_);
    state_ = result.newState;

    if (result.needsConfirmation) {
        qDebug() << "FolderOperationCoordinator: Asking user about existing folders:"
                 << result.existingFolderNames;
        emit folderConfirmationNeeded(result.existingFolderNames);
        return;
    }

    if (result.shouldStartFolderOp) {
        startFolderOperation(result.folderOpToStart);
    }
}

void FolderOperationCoordinator::startFolderOperation(const transfer::PendingFolderOp &op)
{
    state_.currentFolderOp = op;

    QString folderName = QFileInfo(op.sourcePath).fileName();
    qDebug() << "FolderOperationCoordinator: Starting folder operation" << folderName
             << "type:" << static_cast<int>(op.operationType);

    // Create batch for this operation
    int batchId = 0;
    if (createBatchCallback_) {
        batchId = createBatchCallback_(op.operationType,
                                       op.operationType == transfer::OperationType::Upload
                                           ? tr("Uploading %1").arg(folderName)
                                           : tr("Downloading %1").arg(folderName),
                                       folderName, op.sourcePath);
    }
    state_.currentFolderOp.batchId = batchId;

    // Mark batch as not yet scanned (TransferQueue will do this via findBatch)
    for (auto &batch : state_.batches) {
        if (batch.batchId == batchId) {
            batch.scanned = false;
            batch.folderConfirmed = true;  // Already confirmed at this point
            break;
        }
    }

    emit operationStarted(folderName, op.operationType);

    if (op.operationType == transfer::OperationType::Upload) {
        // Handle Replace: delete existing folder first
        if (op.destExists && state_.replaceExisting) {
            qDebug() << "FolderOperationCoordinator: Folder" << op.targetPath
                     << "needs deletion before upload (Replace)";
            state_.pendingUploadAfterDelete = true;
            emit pendingUploadAfterDeleteSet(op.targetPath);
            return;
        }

        // Queue all directories for creation, then upload files
        emit startDirectoryCreationRequested(op.sourcePath, op.targetPath);
    } else {
        // Download: Handle Replace - delete existing local folder first via gateway
        if (op.destExists && state_.replaceExisting) {
            qDebug() << "FolderOperationCoordinator: Local folder" << op.targetPath
                     << "needs deletion before download (Replace)";
            if (localFs_) {
                if (!localFs_->removeDirectoryRecursively(op.targetPath)) {
                    qDebug() << "FolderOperationCoordinator: Failed to delete local folder"
                             << op.targetPath;
                    emit statusMessage(tr("Failed to delete local folder '%1'").arg(op.targetPath),
                                       5000);
                }
            }
        }

        // Create local base directory via gateway
        if (localFs_ && !localFs_->createDirectoryPath(op.targetPath)) {
            qDebug() << "FolderOperationCoordinator: Failed to create local directory"
                     << op.targetPath;
        }

        // Start scanning remote directory
        emit startDownloadScanRequested(op.sourcePath, op.targetPath, op.sourcePath, batchId);
    }
}
