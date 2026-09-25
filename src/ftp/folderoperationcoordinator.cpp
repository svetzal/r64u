/**
 * @file folderoperationcoordinator.cpp
 * @brief Implementation of FolderOperationCoordinator.
 */

#include "folderoperationcoordinator.h"

#include "services/iftpclient.h"
#include "services/ilocalfilesystemservice.h"
#include "utils/logging.h"

#include <QFileInfo>
#include <QTimer>

FolderOperationCoordinator::FolderOperationCoordinator(transfer::State &state,
                                                       IFtpClient *ftpClient,
                                                       ILocalFileSystemService *localFs,
                                                       QObject *parent)
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

void FolderOperationCoordinator::setLocalFileSystem(ILocalFileSystemService *fs)
{
    localFs_ = fs;
}

void FolderOperationCoordinator::setCreateBatchCallback(
    std::function<int(transfer::OperationType, const QString &, const QString &, const QString &)>
        callback)
{
    createBatchCallback_ = std::move(callback);
}

namespace {

QString pastTenseVerb(transfer::OperationType type)
{
    switch (type) {
    case transfer::OperationType::Upload:
        return FolderOperationCoordinator::tr("uploaded");
    case transfer::OperationType::Download:
        return FolderOperationCoordinator::tr("downloaded");
    case transfer::OperationType::Delete:
        return FolderOperationCoordinator::tr("deleted");
    }
    return {};
}

}  // namespace

void FolderOperationCoordinator::enqueueRecursive(transfer::OperationType type,
                                                  const QString &sourcePath,
                                                  const QString &destPath)
{
    const bool isDelete = (type == transfer::OperationType::Delete);

    if (transfer::isPathBeingTransferred(state_, sourcePath, type)) {
        qCDebug(LogTransfer) << "FolderOperationCoordinator: Ignoring duplicate request for"
                             << sourcePath << "type:" << static_cast<int>(type);
        emit statusMessage(tr("'%1' is already being %2")
                               .arg(QFileInfo(sourcePath).fileName(), pastTenseVerb(type)));
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
    op.targetPath = isDelete ? sourcePath : targetDir;
    // Downloads check the local target via the gateway; uploads learn remote existence
    // from a listing later, and deletes have no destination
    const bool isDownload = type == transfer::OperationType::Download;
    op.destExists = isDownload && localFs_ && localFs_->directoryExists(targetDir);
    // A download into a folder that does not exist has nothing to ask about
    op.confirmed = isDownload && !op.destExists;

    // Only one folder operation runs at a time; the others wait their turn
    const bool queueFree = state_.queueState == transfer::QueueState::Idle &&
                           state_.currentFolderOp.batchId < 0 && state_.pendingFolderOps.isEmpty();

    if (!transfer::needsFolderCheck(state_, op)) {
        if (queueFree) {
            startFolderOperation(op);
        } else {
            state_.pendingFolderOps.enqueue(op);
            if (state_.queueState == transfer::QueueState::Idle) {
                emit scheduleProcessNextRequested();
            }
        }
        return;
    }

    // Queue for debounce and folder existence check / confirmation
    state_.pendingFolderOps.enqueue(op);

    if (queueFree) {
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
        // Transfers queued while the dialog was open run now
        emit scheduleProcessNextRequested();
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
    if (state_.pendingFolderOps.isEmpty()) {
        return;
    }
    if (transfer::needsFolderCheck(state_, state_.pendingFolderOps.head())) {
        // Queued behind other work, or left over from a check the connection cut short
        checkPendingFolderOps();
        return;
    }
    startFolderOperation(state_.pendingFolderOps.dequeue());
}

void FolderOperationCoordinator::onFolderOperationComplete()
{
    qCDebug(LogTransfer) << "FolderOperationCoordinator: Folder operation complete:"
                         << state_.currentFolderOp.targetPath;

    state_.currentFolderOp = transfer::PendingFolderOp();

    // Process next pending folder operation if any
    if (!state_.pendingFolderOps.isEmpty()) {
        if (!ftpClient_ || !ftpClient_->isConnected()) {
            // Left queued: the queue resumes them once the connection is back
            qCDebug(LogTransfer) << "FolderOperationCoordinator: Not connected, keeping"
                                 << state_.pendingFolderOps.size() << "folder operations queued";
            return;
        }
        startNextPendingFolderOp();
        return;
    }

    // All folders done
    qCDebug(LogTransfer) << "FolderOperationCoordinator: All folder operations complete";
    state_.replaceExisting = false;
    if (transfer::queuedBatchCount(state_) == 0) {
        emit allOperationsCompleted();
    }
}

void FolderOperationCoordinator::onDebounceTimeout()
{
    qCDebug(LogTransfer) << "FolderOperationCoordinator: Debounce timeout, processing"
                         << state_.pendingFolderOps.size() << "pending folder ops";

    if (state_.queueState != transfer::QueueState::CollectingItems) {
        return;  // The check was abandoned (connection lost); it runs when the queue resumes
    }
    if (state_.pendingFolderOps.isEmpty()) {
        state_.queueState = transfer::QueueState::Idle;
        return;
    }
    checkPendingFolderOps();
}

void FolderOperationCoordinator::checkPendingFolderOps()
{
    debounceTimer_->stop();

    // For uploads, we need to check remote folder existence
    // For downloads, we already know local folder existence
    const transfer::PendingFolderOp &firstOp = state_.pendingFolderOps.head();

    if (firstOp.operationType != transfer::OperationType::Upload) {
        // Downloads: check if any folders exist and need confirmation
        checkFolderConfirmation();
        return;
    }

    if (!ftpClient_ || !ftpClient_->isConnected()) {
        // Left queued: the queue resumes the check once the connection is back
        state_.queueState = transfer::QueueState::Idle;
        return;
    }
    // Busy until the listing tells whether the target folder exists
    state_.queueState = transfer::QueueState::CollectingItems;
    state_.requestedFolderCheckListings.insert(firstOp.destPath);
    ftpClient_->list(firstOp.destPath);
}

void FolderOperationCoordinator::checkFolderConfirmation()
{
    auto result = transfer::checkFolderConfirmation(state_);
    state_ = result.newState;

    if (result.needsConfirmation) {
        qCDebug(LogTransfer) << "FolderOperationCoordinator: Asking user about existing folders:"
                             << result.existingFolderNames;
        emit folderConfirmationNeeded(result.existingFolderNames);
        return;
    }

    if (result.shouldStartFolderOp) {
        startFolderOperation(result.folderOpToStart);
    }
}

QString FolderOperationCoordinator::batchDescription(transfer::OperationType type,
                                                     const QString &folderName)
{
    switch (type) {
    case transfer::OperationType::Upload:
        return tr("Uploading %1").arg(folderName);
    case transfer::OperationType::Download:
        return tr("Downloading %1").arg(folderName);
    case transfer::OperationType::Delete:
        return tr("Deleting %1").arg(folderName);
    }
    return folderName;
}

void FolderOperationCoordinator::startFolderOperation(const transfer::PendingFolderOp &op)
{
    state_.currentFolderOp = op;

    QString folderName = QFileInfo(op.sourcePath).fileName();
    qCDebug(LogTransfer) << "FolderOperationCoordinator: Starting folder operation" << folderName
                         << "type:" << static_cast<int>(op.operationType);

    // Create batch for this operation
    int batchId = 0;
    if (createBatchCallback_) {
        batchId =
            createBatchCallback_(op.operationType, batchDescription(op.operationType, folderName),
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

    if (op.operationType == transfer::OperationType::Delete) {
        emit startDeleteRequested(op.sourcePath);
        return;
    }

    if (op.operationType == transfer::OperationType::Upload) {
        // Handle Replace: delete existing folder first
        if (op.destExists && state_.replaceExisting) {
            qCDebug(LogTransfer) << "FolderOperationCoordinator: Folder" << op.targetPath
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
            qCDebug(LogTransfer) << "FolderOperationCoordinator: Local folder" << op.targetPath
                                 << "needs deletion before download (Replace)";
            if (localFs_) {
                if (!localFs_->removeDirectoryRecursively(op.targetPath)) {
                    qCDebug(LogTransfer)
                        << "FolderOperationCoordinator: Failed to delete local folder"
                        << op.targetPath;
                    emit statusMessage(tr("Failed to delete local folder '%1'").arg(op.targetPath),
                                       5000);
                    emit operationFailed(QFileInfo(op.targetPath).fileName(),
                                         tr("Failed to delete existing local folder"));
                    return;
                }
            }
        }

        // Create local base directory via gateway
        if (localFs_ && !localFs_->createDirectoryPath(op.targetPath)) {
            qCDebug(LogTransfer) << "FolderOperationCoordinator: Failed to create local directory"
                                 << op.targetPath;
            emit operationFailed(QFileInfo(op.targetPath).fileName(),
                                 tr("Failed to create local directory"));
            return;
        }

        // Start scanning remote directory
        emit startDownloadScanRequested(op.sourcePath, op.targetPath, op.sourcePath, batchId);
    }
}
