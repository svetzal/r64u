#include "transferorchestrator.h"

#include "transferftphandler.h"

#include "../services/ftpclientmixin.h"
#include "../services/iftpclient.h"
#include "../services/localfilesystem.h"
#include "../utils/logging.h"

#include <QDebug>
#include <QFileInfo>

TransferOrchestrator::TransferOrchestrator(QObject *parent)
    : QObject(parent), localFs_(new LocalFileSystem(this)),
      batchManager_(new BatchManager(state_, this)),
      timeoutManager_(new TransferTimeoutManager(TransferTimeoutManager::OperationTimeoutMs, this)),
      eventProcessor_(new TransferEventProcessor(this)),
      scanCoordinator_(new RecursiveScanCoordinator(state_, nullptr, localFs_, this)),
      dirCreator_(new RemoteDirectoryCreator(state_, nullptr, localFs_, this)),
      folderCoordinator_(new FolderOperationCoordinator(state_, nullptr, localFs_, this))
{
    // --- BatchManager setup ---
    batchManager_->setModelResetCallbacks(
        [this]() {
            if (modelCallbacks_.beginResetModel)
                modelCallbacks_.beginResetModel();
        },
        [this]() {
            if (modelCallbacks_.endResetModel)
                modelCallbacks_.endResetModel();
        });
    batchManager_->setRowRemovalCallbacks(
        [this](int first, int last) {
            if (modelCallbacks_.beginRemoveRows)
                modelCallbacks_.beginRemoveRows(first, last);
        },
        [this]() {
            if (modelCallbacks_.endRemoveRows)
                modelCallbacks_.endRemoveRows();
        });
    batchManager_->setStopTimeoutCallback([this]() { stopOperationTimeout(); });
    batchManager_->setFolderOpCompleteCallback(
        [this]() { folderCoordinator_->onFolderOperationComplete(); });
    batchManager_->setScheduleProcessNextCallback([this]() { scheduleProcessNext(); });

    connect(batchManager_, &BatchManager::batchStarted, this, &TransferOrchestrator::batchStarted);
    connect(batchManager_, &BatchManager::batchProgressUpdate, this,
            &TransferOrchestrator::batchProgressUpdate);
    connect(batchManager_, &BatchManager::batchCompleted, this,
            &TransferOrchestrator::batchCompleted);
    connect(batchManager_, &BatchManager::allOperationsCompleted, this,
            &TransferOrchestrator::allOperationsCompleted);
    connect(batchManager_, &BatchManager::queueChanged, this, &TransferOrchestrator::queueChanged);

    // --- TransferTimeoutManager setup ---
    connect(timeoutManager_, &TransferTimeoutManager::operationTimedOut, this,
            &TransferOrchestrator::onOperationTimeout);

    // --- RecursiveScanCoordinator connections ---
    connect(scanCoordinator_, &RecursiveScanCoordinator::downloadFileDiscovered, this,
            [this](const QString &remotePath, const QString &localPath, int batchId) {
                enqueueDownload(remotePath, localPath, batchId);
            });
    connect(scanCoordinator_, &RecursiveScanCoordinator::completeBatchRequested, this,
            [this](int batchId) { completeBatch(batchId); });
    connect(scanCoordinator_, &RecursiveScanCoordinator::scheduleProcessNextRequested, this,
            [this]() {
                if (TransferBatch *batch = findBatch(state_.currentFolderOp.batchId)) {
                    batch->scanned = true;
                }
                transitionTo(QueueState::Idle);
                scheduleProcessNext();
            });
    connect(scanCoordinator_, &RecursiveScanCoordinator::statusMessage, this,
            &TransferOrchestrator::statusMessage);
    connect(scanCoordinator_, &RecursiveScanCoordinator::scanningStarted, this,
            &TransferOrchestrator::scanningStarted);
    connect(scanCoordinator_, &RecursiveScanCoordinator::scanningProgress, this,
            &TransferOrchestrator::scanningProgress);
    connect(scanCoordinator_, &RecursiveScanCoordinator::deleteScanComplete, this, [this]() {
        transitionTo(QueueState::Deleting);
        emit queueChanged();
        processNextDelete();
    });
    connect(scanCoordinator_, &RecursiveScanCoordinator::folderCheckComplete, this,
            [this](const QString & /*path*/) { folderCoordinator_->resumeAfterFolderCheck(); });
    connect(scanCoordinator_, &RecursiveScanCoordinator::uploadCheckFileExists, this,
            [this](const QString &fileName) {
                emit overwriteConfirmationNeeded(fileName, OperationType::Upload);
            });
    connect(scanCoordinator_, &RecursiveScanCoordinator::uploadCheckNoConflict, this,
            [this]() { scheduleProcessNext(); });

    // --- RemoteDirectoryCreator connections ---
    connect(dirCreator_, &RemoteDirectoryCreator::directoryCreationProgress, this,
            &TransferOrchestrator::directoryCreationProgress);
    connect(dirCreator_, &RemoteDirectoryCreator::allDirectoriesCreated, this,
            [this]() { finishDirectoryCreation(); });

    // --- FolderOperationCoordinator connections ---
    folderCoordinator_->setCreateBatchCallback(
        [this](transfer::OperationType type, const QString &description, const QString &folderName,
               const QString &sourcePath) {
            return createBatch(type, description, folderName, sourcePath);
        });
    connect(folderCoordinator_, &FolderOperationCoordinator::startDownloadScanRequested, this,
            [this](const QString &remotePath, const QString &localBase, const QString &remoteBase,
                   int batchId) {
                transitionTo(QueueState::Scanning);
                scanCoordinator_->startDownloadScan(remotePath, localBase, remoteBase, batchId);
            });
    connect(folderCoordinator_, &FolderOperationCoordinator::startDirectoryCreationRequested, this,
            [this](const QString &localDir, const QString &remoteDir) {
                dirCreator_->queueDirectoriesForUpload(localDir, remoteDir);
                if (!state_.pendingMkdirs.isEmpty()) {
                    transitionTo(QueueState::CreatingDirectories);
                    dirCreator_->createNextDirectory();
                } else {
                    if (TransferBatch *batch = findBatch(state_.currentFolderOp.batchId)) {
                        batch->scanned = true;
                    }
                    finishDirectoryCreation();
                }
            });
    connect(folderCoordinator_, &FolderOperationCoordinator::startDeleteRequested, this,
            [this](const QString &remotePath) { enqueueRecursiveDelete(remotePath); });
    connect(folderCoordinator_, &FolderOperationCoordinator::pendingUploadAfterDeleteSet, this,
            [this](const QString &targetPath) { enqueueRecursiveDelete(targetPath); });
    connect(folderCoordinator_, &FolderOperationCoordinator::folderConfirmationNeeded, this,
            &TransferOrchestrator::folderExistsConfirmationNeeded);
    connect(folderCoordinator_, &FolderOperationCoordinator::operationStarted, this,
            &TransferOrchestrator::operationStarted);
    connect(folderCoordinator_, &FolderOperationCoordinator::allOperationsCompleted, this,
            &TransferOrchestrator::allOperationsCompleted);
    connect(folderCoordinator_, &FolderOperationCoordinator::operationsCancelled, this,
            &TransferOrchestrator::operationsCancelled);
    connect(folderCoordinator_, &FolderOperationCoordinator::statusMessage, this,
            &TransferOrchestrator::statusMessage);
    connect(folderCoordinator_, &FolderOperationCoordinator::scheduleProcessNextRequested, this,
            [this]() { scheduleProcessNext(); });

    // --- TransferFtpHandler setup ---
    ftpHandler_ = new TransferFtpHandler(state_, this);
    ftpHandler_->setTimeoutManager(timeoutManager_);
    ftpHandler_->setDirCreator(dirCreator_);
    ftpHandler_->setScanCoordinator(scanCoordinator_);

    connect(ftpHandler_, &TransferFtpHandler::itemDataChanged, this, [this](int row) {
        if (modelCallbacks_.dataChangedRow)
            modelCallbacks_.dataChangedRow(row);
    });
    connect(ftpHandler_, &TransferFtpHandler::operationCompleted, this,
            &TransferOrchestrator::operationCompleted);
    connect(ftpHandler_, &TransferFtpHandler::operationFailed, this,
            &TransferOrchestrator::operationFailed);
    connect(ftpHandler_, &TransferFtpHandler::queueChanged, this,
            &TransferOrchestrator::queueChanged);
    connect(ftpHandler_, &TransferFtpHandler::deleteProgressUpdate, this,
            &TransferOrchestrator::deleteProgressUpdate);
    connect(ftpHandler_, &TransferFtpHandler::scheduleProcessNextRequested, this,
            [this]() { scheduleProcessNext(); });
    connect(ftpHandler_, &TransferFtpHandler::processNextDeleteRequested, this,
            [this]() { processNextDelete(); });
    connect(ftpHandler_, &TransferFtpHandler::completeBatchRequested, this,
            [this](int batchId) { completeBatch(batchId); });
    connect(ftpHandler_, &TransferFtpHandler::batchProgressRequested, this,
            [this](int batchId, bool isComplete, bool includeFailed) {
                emitBatchProgressAndComplete(batchId, isComplete, includeFailed);
            });
}

TransferOrchestrator::~TransferOrchestrator()
{
    disconnectFtpClient(ftpClient_, this);
}

void TransferOrchestrator::setModelCallbacks(const ModelCallbacks &callbacks)
{
    modelCallbacks_ = callbacks;
}

void TransferOrchestrator::setLocalFileSystem(ILocalFileSystem *fs)
{
    localFs_ = fs;
    scanCoordinator_->setLocalFileSystem(fs);
    dirCreator_->setLocalFileSystem(fs);
    folderCoordinator_->setLocalFileSystem(fs);
}

// ============================================================================
// Event queue
// ============================================================================

void TransferOrchestrator::scheduleProcessNext()
{
    eventProcessor_->schedule([this]() { processNext(); });
}

void TransferOrchestrator::flushEventQueue()
{
    eventProcessor_->flush();
}

// ============================================================================
// State machine
// ============================================================================

void TransferOrchestrator::transitionTo(QueueState newState)
{
    if (state_.queueState == newState) {
        return;
    }

    qCDebug(LogTransfer) << "TransferOrchestrator: State transition"
                         << queueStateToString(state_.queueState) << "->"
                         << queueStateToString(newState);

    state_.queueState = newState;
}

// ============================================================================
// FTP client
// ============================================================================

void TransferOrchestrator::setFtpClient(IFtpClient *client)
{
    ftpClient_ = client;

    scanCoordinator_->setFtpClient(client);
    dirCreator_->setFtpClient(client);
    folderCoordinator_->setFtpClient(client);
    ftpHandler_->setFtpClient(client);
}

// ============================================================================
// Enqueue helpers
// ============================================================================

int TransferOrchestrator::findBatchIndex(int batchId) const
{
    for (int i = 0; i < state_.batches.size(); ++i) {
        if (state_.batches[i].batchId == batchId)
            return i;
    }
    return -1;
}

void TransferOrchestrator::activateAndSchedule(int batchIdx)
{
    if (state_.activeBatchIndex < 0) {
        state_.activeBatchIndex = batchIdx;
        state_.batches[batchIdx].scanned = true;
        state_.batches[batchIdx].folderConfirmed = true;
        emit batchStarted(state_.batches[batchIdx].batchId);
    }
    emit queueChanged();
    if (state_.queueState == QueueState::Idle)
        scheduleProcessNext();
}

// ============================================================================
// Single-file enqueue operations
// ============================================================================

void TransferOrchestrator::enqueueUpload(const QString &localPath, const QString &remotePath,
                                         int targetBatchId)
{
    int batchIdx = (targetBatchId >= 0) ? findBatchIndex(targetBatchId) : -1;

    if (batchIdx < 0) {
        batchIdx = state_.activeBatchIndex;
        if (batchIdx < 0 || state_.batches[batchIdx].operationType != OperationType::Upload) {
            QString fileName = QFileInfo(localPath).fileName();
            QString sourcePath = state_.currentFolderOp.sourcePath.isEmpty()
                                     ? QString()
                                     : state_.currentFolderOp.sourcePath;
            int batchId = createBatch(OperationType::Upload, tr("Uploading %1").arg(fileName),
                                      fileName, sourcePath);
            batchIdx = findBatchIndex(batchId);
        }
    }

    if (batchIdx < 0 || batchIdx >= state_.batches.size()) {
        qCWarning(LogTransfer) << "TransferOrchestrator::enqueueUpload - no valid batch";
        return;
    }

    TransferItem item;
    item.localPath = localPath;
    item.remotePath = remotePath;
    item.operationType = OperationType::Upload;
    item.status = TransferItem::Status::Pending;
    item.totalBytes = localFs_->fileSize(localPath);
    item.batchId = state_.batches[batchIdx].batchId;

    if (modelCallbacks_.beginInsertRows)
        modelCallbacks_.beginInsertRows(state_.items.size(), state_.items.size());
    state_.items.append(item);
    if (modelCallbacks_.endInsertRows)
        modelCallbacks_.endInsertRows();

    state_.batches[batchIdx].items.append(item);
    activateAndSchedule(batchIdx);
}

void TransferOrchestrator::enqueueDownload(const QString &remotePath, const QString &localPath,
                                           int targetBatchId)
{
    int batchIdx = (targetBatchId >= 0) ? findBatchIndex(targetBatchId) : -1;

    if (batchIdx < 0) {
        batchIdx = state_.activeBatchIndex;
        if (batchIdx < 0 || state_.batches[batchIdx].operationType != OperationType::Download) {
            QString fileName = QFileInfo(remotePath).fileName();
            QString sourcePath = (state_.queueState == QueueState::Scanning)
                                     ? state_.currentFolderOp.sourcePath
                                     : QString();
            int batchId = createBatch(OperationType::Download, tr("Downloading %1").arg(fileName),
                                      fileName, sourcePath);
            batchIdx = findBatchIndex(batchId);
        }
    }

    if (batchIdx < 0 || batchIdx >= state_.batches.size()) {
        qCWarning(LogTransfer) << "TransferOrchestrator::enqueueDownload - no valid batch";
        return;
    }

    TransferItem item;
    item.localPath = localPath;
    item.remotePath = remotePath;
    item.operationType = OperationType::Download;
    item.status = TransferItem::Status::Pending;
    item.batchId = state_.batches[batchIdx].batchId;

    if (modelCallbacks_.beginInsertRows)
        modelCallbacks_.beginInsertRows(state_.items.size(), state_.items.size());
    state_.items.append(item);
    if (modelCallbacks_.endInsertRows)
        modelCallbacks_.endInsertRows();

    state_.batches[batchIdx].items.append(item);
    activateAndSchedule(batchIdx);
}

// ============================================================================
// Recursive folder operations
// ============================================================================

void TransferOrchestrator::enqueueRecursiveUpload(const QString &localDir, const QString &remoteDir)
{
    if (!ftpClient_ || !ftpClient_->isConnected()) {
        qCWarning(LogTransfer) << "enqueueRecursiveUpload skipped: FTP not connected";
        return;
    }

    if (!localFs_->directoryExists(localDir))
        return;

    folderCoordinator_->enqueueRecursive(OperationType::Upload, localDir, remoteDir);
}

void TransferOrchestrator::enqueueRecursiveDownload(const QString &remoteDir,
                                                    const QString &localDir)
{
    if (!ftpClient_ || !ftpClient_->isConnected()) {
        qCWarning(LogTransfer) << "enqueueRecursiveDownload skipped: FTP not connected";
        return;
    }

    QString normalizedRemote = remoteDir;
    while (normalizedRemote.endsWith('/') && normalizedRemote.length() > 1) {
        normalizedRemote.chop(1);
    }

    folderCoordinator_->enqueueRecursive(OperationType::Download, normalizedRemote, localDir);
}

void TransferOrchestrator::respondToFolderExists(FolderExistsResponse response)
{
    folderCoordinator_->respondToFolderExists(response);
}

// ============================================================================
// Coordinator helpers
// ============================================================================

void TransferOrchestrator::finishDirectoryCreation()
{
    qCDebug(LogTransfer)
        << "TransferOrchestrator: All directories created, queueing files for upload";

    if (TransferBatch *batch = findBatch(state_.currentFolderOp.batchId)) {
        batch->scanned = true;
    }

    const QString &sourcePath = state_.currentFolderOp.sourcePath;
    if (!localFs_->directoryExists(sourcePath)) {
        qCWarning(LogTransfer) << "TransferOrchestrator: Source directory doesn't exist:"
                               << sourcePath;
        return;
    }

    const QStringList files = localFs_->listFilesRecursively(sourcePath);
    int fileCount = 0;
    for (const QString &filePath : files) {
        const QString relativePath = localFs_->relativePath(sourcePath, filePath);
        const QString remotePath = state_.currentFolderOp.targetPath + '/' + relativePath;

        enqueueUpload(filePath, remotePath, state_.currentFolderOp.batchId);
        fileCount++;
    }

    qCDebug(LogTransfer) << "TransferOrchestrator: Queued" << fileCount << "files for upload";

    if (fileCount == 0) {
        if (findBatch(state_.currentFolderOp.batchId)) {
            qCDebug(LogTransfer) << "TransferOrchestrator: Empty folder upload batch"
                                 << state_.currentFolderOp.batchId;
            completeBatch(state_.currentFolderOp.batchId);
            return;
        }
    }

    transitionTo(QueueState::Idle);
    scheduleProcessNext();
}

// ============================================================================
// Delete operations
// ============================================================================

void TransferOrchestrator::enqueueDelete(const QString &remotePath, bool isDirectory)
{
    int batchIdx = state_.activeBatchIndex;
    if (batchIdx < 0 || state_.batches[batchIdx].operationType != OperationType::Delete) {
        QString fileName = QFileInfo(remotePath).fileName();
        QString sourcePath =
            !state_.recursiveDeleteBase.isEmpty() ? state_.recursiveDeleteBase : QString();
        int batchId = createBatch(OperationType::Delete, tr("Deleting %1").arg(fileName), fileName,
                                  sourcePath);
        for (int i = 0; i < state_.batches.size(); ++i) {
            if (state_.batches[i].batchId == batchId) {
                batchIdx = i;
                break;
            }
        }
    }

    if (batchIdx < 0 || batchIdx >= state_.batches.size()) {
        qCWarning(LogTransfer) << "TransferOrchestrator::enqueueDelete - no valid batch";
        return;
    }

    TransferItem item;
    item.remotePath = remotePath;
    item.operationType = OperationType::Delete;
    item.status = TransferItem::Status::Pending;
    item.isDirectory = isDirectory;
    item.batchId = state_.batches[batchIdx].batchId;

    if (modelCallbacks_.beginInsertRows)
        modelCallbacks_.beginInsertRows(state_.items.size(), state_.items.size());
    state_.items.append(item);
    if (modelCallbacks_.endInsertRows)
        modelCallbacks_.endInsertRows();

    state_.batches[batchIdx].items.append(item);

    if (state_.activeBatchIndex < 0) {
        state_.activeBatchIndex = batchIdx;
        state_.batches[batchIdx].scanned = true;
        state_.batches[batchIdx].folderConfirmed = true;
        emit batchStarted(state_.batches[batchIdx].batchId);
    }

    emit queueChanged();

    if (state_.queueState == QueueState::Idle) {
        scheduleProcessNext();
    }
}

void TransferOrchestrator::enqueueRecursiveDelete(const QString &remotePath)
{
    if (!ftpClient_ || !ftpClient_->isConnected()) {
        qCWarning(LogTransfer) << "enqueueRecursiveDelete skipped: FTP not connected";
        return;
    }

    QString normalizedPath = remotePath;
    while (normalizedPath.endsWith('/') && normalizedPath.length() > 1) {
        normalizedPath.chop(1);
    }

    if (isPathBeingTransferred(normalizedPath, OperationType::Delete)) {
        qCDebug(LogTransfer) << "TransferOrchestrator: Ignoring duplicate delete request for"
                             << normalizedPath;
        emit statusMessage(
            tr("'%1' is already being deleted").arg(QFileInfo(normalizedPath).fileName()), 3000);
        return;
    }

    state_.deleteQueue.clear();
    state_.deletedCount = 0;
    state_.recursiveDeleteBase = normalizedPath;

    emit queueChanged();

    transitionTo(QueueState::Scanning);
    scanCoordinator_->startDeleteScan(normalizedPath);
}

void TransferOrchestrator::processNextDelete()
{
    if (!ftpClient_ || !ftpClient_->isConnected()) {
        qCWarning(LogTransfer) << "processNextDelete: FTP disconnected, resetting to Idle";
        transitionTo(QueueState::Idle);
        return;
    }

    if (state_.deletedCount >= state_.deleteQueue.size()) {
        qCDebug(LogTransfer) << "TransferOrchestrator: All deletes complete";
        transitionTo(QueueState::Idle);
        state_.deleteQueue.clear();
        state_.recursiveDeleteBase.clear();
        emit operationCompleted(tr("Deleted %1 items").arg(state_.deletedCount));

        if (state_.pendingUploadAfterDelete) {
            qCDebug(LogTransfer)
                << "TransferOrchestrator: Delete completed, starting pending upload";
            state_.pendingUploadAfterDelete = false;

            dirCreator_->queueDirectoriesForUpload(state_.currentFolderOp.sourcePath,
                                                   state_.currentFolderOp.targetPath);

            if (!state_.pendingMkdirs.isEmpty()) {
                transitionTo(QueueState::CreatingDirectories);
                dirCreator_->createNextDirectory();
            } else {
                finishDirectoryCreation();
            }
        } else {
            emit allOperationsCompleted();
        }
        return;
    }

    const DeleteItem &item = state_.deleteQueue[state_.deletedCount];
    qCDebug(LogTransfer) << "TransferOrchestrator: Deleting" << (state_.deletedCount + 1) << "of"
                         << state_.deleteQueue.size() << ":" << item.path;

    if (item.isDirectory) {
        ftpClient_->removeDirectory(item.path);
    } else {
        ftpClient_->remove(item.path);
    }
}

// ============================================================================
// Core processing loop
// ============================================================================

void TransferOrchestrator::processNext()
{
    qCDebug(LogTransfer) << "TransferOrchestrator: processNext, state:"
                         << queueStateToString(state_.queueState);

    bool ftpReady = ftpClient_ && ftpClient_->isConnected();

    auto decision = transfer::decideNextAction(
        state_, ftpReady, [this](const QString &path) { return localFs_->fileExists(path); });

    switch (decision.action) {
    case transfer::ProcessNextAction::Blocked:
        qCDebug(LogTransfer) << "TransferOrchestrator: processNext blocked by state:"
                             << queueStateToString(state_.queueState);
        return;

    case transfer::ProcessNextAction::NoFtpClient:
        qCDebug(LogTransfer) << "TransferOrchestrator: FTP client not ready";
        return;

    case transfer::ProcessNextAction::StartFolderOp: {
        folderCoordinator_->startNextPendingFolderOp();
        return;
    }

    case transfer::ProcessNextAction::NeedOverwriteCheck_Download:
        transitionTo(QueueState::AwaitingFileConfirm);
        state_.currentIndex = decision.itemIndex;
        state_.pendingConfirmation.itemIndex = decision.itemIndex;
        state_.pendingConfirmation.opType = OperationType::Download;
        emit overwriteConfirmationNeeded(decision.fileNameForSignal, OperationType::Download);
        return;

    case transfer::ProcessNextAction::NeedOverwriteCheck_Upload:
        state_.currentIndex = decision.itemIndex;
        state_.requestedUploadFileCheckListings.insert(decision.uploadCheckDir);
        ftpClient_->list(decision.uploadCheckDir);
        return;

    case transfer::ProcessNextAction::StartTransfer: {
        int i = decision.itemIndex;
        state_.currentIndex = i;
        state_.items[i].status = TransferItem::Status::InProgress;
        transitionTo(QueueState::Transferring);

        if (modelCallbacks_.dataChangedRow)
            modelCallbacks_.dataChangedRow(i);
        emit operationStarted(decision.fileNameForSignal, state_.items[i].operationType);

        startOperationTimeout();

        if (state_.items[i].operationType == OperationType::Upload) {
            ftpClient_->upload(state_.items[i].localPath, state_.items[i].remotePath);
        } else if (state_.items[i].operationType == OperationType::Download) {
            ftpClient_->download(state_.items[i].remotePath, state_.items[i].localPath);
        } else if (state_.items[i].operationType == OperationType::Delete) {
            if (state_.items[i].isDirectory) {
                ftpClient_->removeDirectory(state_.items[i].remotePath);
            } else {
                ftpClient_->remove(state_.items[i].remotePath);
            }
        }
        return;
    }

    case transfer::ProcessNextAction::NoPending:
        qCDebug(LogTransfer) << "TransferOrchestrator: No more pending items";
        stopOperationTimeout();
        state_.currentIndex = -1;
        if (state_.batches.isEmpty()) {
            emit allOperationsCompleted();
        }
        return;
    }
}

// ============================================================================
// Confirmation handling
// ============================================================================

void TransferOrchestrator::respondToOverwrite(OverwriteResponse response)
{
    int affectedBatchId = -1;
    if (response == OverwriteResponse::Skip) {
        int itemIdx = state_.pendingConfirmation.itemIndex;
        if (itemIdx >= 0 && itemIdx < state_.items.size()) {
            affectedBatchId = state_.items[itemIdx].batchId;
        }
    }

    auto result = transfer::respondToOverwrite(state_, response);
    state_ = result.newState;

    if (result.shouldCancelAll) {
        cancelAll();
        return;
    }

    if (!state_.items.isEmpty()) {
        for (int i = 0; i < state_.items.size(); ++i) {
            if (modelCallbacks_.dataChangedRow)
                modelCallbacks_.dataChangedRow(i);
        }
    }

    if (response == OverwriteResponse::Skip && affectedBatchId >= 0) {
        bool batchIsComplete = false;
        if (const TransferBatch *batch = findBatch(affectedBatchId)) {
            batchIsComplete = batch->isComplete();
        }
        emitBatchProgressAndComplete(affectedBatchId, batchIsComplete);
        if (batchIsComplete) {
            return;
        }
    }

    if (result.shouldScheduleProcessNext) {
        scheduleProcessNext();
    }
}

// ============================================================================
// Batch management (delegated to batchManager_)
// ============================================================================

int TransferOrchestrator::createBatch(OperationType type, const QString &description,
                                      const QString &folderName, const QString &sourcePath)
{
    return batchManager_->createBatch(type, description, folderName, sourcePath);
}

void TransferOrchestrator::activateNextBatch()
{
    batchManager_->activateNextBatch();
}

void TransferOrchestrator::completeBatch(int batchId)
{
    batchManager_->completeBatch(batchId);
}

void TransferOrchestrator::purgeBatch(int batchId)
{
    batchManager_->purgeBatch(batchId);
}

void TransferOrchestrator::emitBatchProgressAndComplete(int batchId, bool batchIsComplete,
                                                        bool includeFailed)
{
    batchManager_->emitBatchProgressAndComplete(batchId, batchIsComplete, includeFailed);
}

void TransferOrchestrator::markCurrentComplete(TransferItem::Status status)
{
    if (state_.currentIndex < 0 || state_.currentIndex >= state_.items.size()) {
        qCWarning(LogTransfer) << "markCurrentComplete: invalid index" << state_.currentIndex;
        return;
    }

    int completedIndex = state_.currentIndex;
    auto result = transfer::markItemComplete(state_, completedIndex, status);
    state_ = result.newState;

    if (modelCallbacks_.dataChangedRow)
        modelCallbacks_.dataChangedRow(completedIndex);

    if (result.batchId >= 0) {
        emitBatchProgressAndComplete(result.batchId, result.batchIsComplete);
    }
}

TransferBatch *TransferOrchestrator::findBatch(int batchId)
{
    return batchManager_->findBatch(batchId);
}

const TransferBatch *TransferOrchestrator::findBatch(int batchId) const
{
    return batchManager_->findBatch(batchId);
}

TransferBatch *TransferOrchestrator::activeBatch()
{
    return batchManager_->activeBatch();
}

int TransferOrchestrator::findItemIndex(const QString &localPath, const QString &remotePath) const
{
    return transfer::findItemIndex(state_, localPath, remotePath);
}

// ============================================================================
// Queue operations
// ============================================================================

void TransferOrchestrator::clear()
{
    if (modelCallbacks_.beginResetModel)
        modelCallbacks_.beginResetModel();
    state_ = transfer::clearAll(state_);
    if (modelCallbacks_.endResetModel)
        modelCallbacks_.endResetModel();

    folderCoordinator_->stopDebounce();
    emit queueChanged();
}

void TransferOrchestrator::cancelAll()
{
    if (ftpClient_ && (state_.queueState == QueueState::Transferring ||
                       state_.queueState == QueueState::Deleting)) {
        ftpClient_->abort();
    }

    state_ = transfer::cancelAllItems(state_);

    folderCoordinator_->stopDebounce();

    for (int i = 0; i < state_.items.size(); ++i) {
        if (modelCallbacks_.dataChangedRow)
            modelCallbacks_.dataChangedRow(i);
    }
    emit queueChanged();
    emit operationsCancelled();
}

void TransferOrchestrator::cancelBatch(int batchId)
{
    int batchIdx = transfer::findBatchIndex(state_, batchId);
    if (batchIdx < 0) {
        qCWarning(LogTransfer) << "cancelBatch: batch" << batchId << "not found";
        return;
    }

    bool isActive = (batchIdx == state_.activeBatchIndex);
    if (isActive && ftpClient_ &&
        (state_.queueState == QueueState::Transferring ||
         state_.queueState == QueueState::Deleting)) {
        ftpClient_->abort();
    }

    auto result = transfer::cancelBatch(state_, batchId);
    state_ = result.newState;

    for (int i = 0; i < state_.items.size(); ++i) {
        if (modelCallbacks_.dataChangedRow)
            modelCallbacks_.dataChangedRow(i);
    }
    emit queueChanged();

    if (result.wasActiveBatch) {
        scheduleProcessNext();
    }
}

// ============================================================================
// Query methods
// ============================================================================

int TransferOrchestrator::pendingCount() const
{
    return transfer::pendingCount(state_);
}

int TransferOrchestrator::activeCount() const
{
    return transfer::activeCount(state_);
}

int TransferOrchestrator::activeAndPendingCount() const
{
    return transfer::activeAndPendingCount(state_);
}

bool TransferOrchestrator::isScanningForDelete() const
{
    return state_.queueState == QueueState::Scanning && !state_.pendingDeleteScans.isEmpty();
}

bool TransferOrchestrator::hasActiveBatch() const
{
    return batchManager_->hasActiveBatch();
}

int TransferOrchestrator::queuedBatchCount() const
{
    return batchManager_->queuedBatchCount();
}

bool TransferOrchestrator::isPathBeingTransferred(const QString &path, OperationType type) const
{
    return transfer::isPathBeingTransferred(state_, path, type);
}

BatchProgress TransferOrchestrator::activeBatchProgress() const
{
    return batchManager_->activeBatchProgress();
}

BatchProgress TransferOrchestrator::batchProgress(int batchId) const
{
    return batchManager_->batchProgress(batchId);
}

QList<int> TransferOrchestrator::allBatchIds() const
{
    return batchManager_->allBatchIds();
}

// ============================================================================
// Timeout handling
// ============================================================================

void TransferOrchestrator::startOperationTimeout()
{
    timeoutManager_->start();
}

void TransferOrchestrator::stopOperationTimeout()
{
    timeoutManager_->stop();
}

void TransferOrchestrator::onOperationTimeout()
{
    qCDebug(LogTransfer) << "TransferOrchestrator: Operation timeout!";

    if (ftpClient_) {
        ftpClient_->abort();
    }

    int originalIndex = state_.currentIndex;
    state_ = transfer::handleOperationTimeout(state_);

    if (originalIndex >= 0 && originalIndex < state_.items.size()) {
        QString errorMessage = tr("Operation timed out after %1 minutes")
                                   .arg(TransferTimeoutManager::OperationTimeoutMs / 60000);
        state_.items[originalIndex].errorMessage = errorMessage;

        QString fileName = QFileInfo(state_.items[originalIndex].localPath.isEmpty()
                                         ? state_.items[originalIndex].remotePath
                                         : state_.items[originalIndex].localPath)
                               .fileName();
        if (modelCallbacks_.dataChangedRow)
            modelCallbacks_.dataChangedRow(originalIndex);
        emit operationFailed(fileName, errorMessage);
    }

    scheduleProcessNext();
}
