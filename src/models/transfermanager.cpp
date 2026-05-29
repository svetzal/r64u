#include "transfermanager.h"

#include "transferftphandler.h"

#include "../services/ftpclientmixin.h"
#include "../services/iftpclient.h"
#include "../services/localfilesystemservice.h"
#include "../utils/logging.h"

#include <QDebug>
#include <QFileInfo>

TransferManager::TransferManager(QObject *parent)
    : QObject(parent), localFs_(new LocalFileSystemService(this)),
      batchManager_(new BatchManager(state_, this)),
      timeoutManager_(new TransferTimeoutManager(TransferTimeoutManager::OperationTimeoutMs, this)),
      eventProcessor_(new TransferEventHandler(this)),
      scanCoordinator_(new RecursiveScanCoordinator(state_, nullptr, localFs_, this)),
      dirCreator_(new RemoteDirectoryCoordinator(state_, nullptr, localFs_, this)),
      folderCoordinator_(new FolderOperationCoordinator(state_, nullptr, localFs_, this))
{
    deleteHandler_ = new TransferDeleteHandler(state_, this);

    setupBatchManager();
    setupTimeoutManager();
    setupScanCoordinator();
    setupDirectoryCreator();
    setupFolderOperationCoordinator();
    setupFtpHandler();
    setupDeleteHandler();
}

void TransferManager::setupBatchManager()
{
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

    connect(batchManager_, &BatchManager::batchStarted, this, &TransferManager::batchStarted);
    connect(batchManager_, &BatchManager::batchProgressUpdate, this,
            &TransferManager::batchProgressUpdate);
    connect(batchManager_, &BatchManager::batchCompleted, this, &TransferManager::batchCompleted);
    connect(batchManager_, &BatchManager::allOperationsCompleted, this,
            &TransferManager::allOperationsCompleted);
    connect(batchManager_, &BatchManager::queueChanged, this, &TransferManager::queueChanged);
}

void TransferManager::setupTimeoutManager()
{
    connect(timeoutManager_, &TransferTimeoutManager::operationTimedOut, this,
            &TransferManager::onOperationTimeout);
}

void TransferManager::setupScanCoordinator()
{
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
            &TransferManager::statusMessage);
    connect(scanCoordinator_, &RecursiveScanCoordinator::scanningStarted, this,
            &TransferManager::scanningStarted);
    connect(scanCoordinator_, &RecursiveScanCoordinator::scanningProgress, this,
            &TransferManager::scanningProgress);
    connect(scanCoordinator_, &RecursiveScanCoordinator::deleteScanComplete, this, [this]() {
        transitionTo(QueueState::Deleting);
        emit queueChanged();
        deleteHandler_->processNextDelete();
    });
    connect(scanCoordinator_, &RecursiveScanCoordinator::folderCheckComplete, this,
            [this](const QString & /*path*/) { folderCoordinator_->resumeAfterFolderCheck(); });
    connect(scanCoordinator_, &RecursiveScanCoordinator::uploadCheckFileExists, this,
            [this](const QString &fileName) {
                emit overwriteConfirmationNeeded(fileName, OperationType::Upload);
            });
    connect(scanCoordinator_, &RecursiveScanCoordinator::uploadCheckNoConflict, this,
            [this]() { scheduleProcessNext(); });
}

void TransferManager::setupDirectoryCreator()
{
    connect(dirCreator_, &RemoteDirectoryCoordinator::directoryCreationProgress, this,
            &TransferManager::directoryCreationProgress);
    connect(dirCreator_, &RemoteDirectoryCoordinator::allDirectoriesCreated, this,
            [this]() { finishDirectoryCreation(); });
}

void TransferManager::setupFolderOperationCoordinator()
{
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
            &TransferManager::folderExistsConfirmationNeeded);
    connect(folderCoordinator_, &FolderOperationCoordinator::operationStarted, this,
            &TransferManager::operationStarted);
    connect(folderCoordinator_, &FolderOperationCoordinator::allOperationsCompleted, this,
            &TransferManager::allOperationsCompleted);
    connect(folderCoordinator_, &FolderOperationCoordinator::operationsCancelled, this,
            &TransferManager::operationsCancelled);
    connect(folderCoordinator_, &FolderOperationCoordinator::statusMessage, this,
            &TransferManager::statusMessage);
    connect(folderCoordinator_, &FolderOperationCoordinator::operationFailed, this,
            &TransferManager::operationFailed);
    connect(folderCoordinator_, &FolderOperationCoordinator::scheduleProcessNextRequested, this,
            [this]() { scheduleProcessNext(); });
}

void TransferManager::setupFtpHandler()
{
    ftpHandler_ = new TransferFtpHandler(state_, this);
    ftpHandler_->setTimeoutManager(timeoutManager_);
    ftpHandler_->setDirCreator(dirCreator_);
    ftpHandler_->setScanCoordinator(scanCoordinator_);

    connect(ftpHandler_, &TransferFtpHandler::itemDataChanged, this,
            [this](int row) { notifyDataChanged(row); });
    connect(ftpHandler_, &TransferFtpHandler::operationCompleted, this,
            &TransferManager::operationCompleted);
    connect(ftpHandler_, &TransferFtpHandler::operationFailed, this,
            &TransferManager::operationFailed);
    connect(ftpHandler_, &TransferFtpHandler::queueChanged, this, &TransferManager::queueChanged);
    connect(ftpHandler_, &TransferFtpHandler::deleteProgressUpdate, this,
            &TransferManager::deleteProgressUpdate);
    connect(ftpHandler_, &TransferFtpHandler::scheduleProcessNextRequested, this,
            [this]() { scheduleProcessNext(); });
    connect(ftpHandler_, &TransferFtpHandler::processNextDeleteRequested, this,
            [this]() { deleteHandler_->processNextDelete(); });
    connect(ftpHandler_, &TransferFtpHandler::completeBatchRequested, this,
            [this](int batchId) { completeBatch(batchId); });
    connect(ftpHandler_, &TransferFtpHandler::batchProgressRequested, this,
            [this](int batchId, bool isComplete, bool includeFailed) {
                emitBatchProgressAndComplete(batchId, isComplete, includeFailed);
            });
}

void TransferManager::setupDeleteHandler()
{
    deleteHandler_->setScanCoordinator(scanCoordinator_);
    deleteHandler_->setDirCreator(dirCreator_);
    deleteHandler_->setCreateBatchCallback([this](OperationType type, const QString &description,
                                                  const QString &folderName,
                                                  const QString &sourcePath) {
        return createBatch(type, description, folderName, sourcePath);
    });
    deleteHandler_->setBeginInsertCallback(
        [this](int first, int last) { notifyBeginInsert(first, last); });
    deleteHandler_->setEndInsertCallback([this]() { notifyEndInsert(); });
    deleteHandler_->setTransitionToCallback([this](QueueState s) { transitionTo(s); });
    deleteHandler_->setScheduleProcessNextCallback([this]() { scheduleProcessNext(); });

    connect(deleteHandler_, &TransferDeleteHandler::operationFailed, this,
            &TransferManager::operationFailed);
    connect(deleteHandler_, &TransferDeleteHandler::operationCompleted, this,
            &TransferManager::operationCompleted);
    connect(deleteHandler_, &TransferDeleteHandler::allOperationsCompleted, this,
            &TransferManager::allOperationsCompleted);
    connect(deleteHandler_, &TransferDeleteHandler::statusMessage, this,
            &TransferManager::statusMessage);
    connect(deleteHandler_, &TransferDeleteHandler::queueChanged, this,
            &TransferManager::queueChanged);
    connect(deleteHandler_, &TransferDeleteHandler::batchStarted, this,
            &TransferManager::batchStarted);
    connect(deleteHandler_, &TransferDeleteHandler::startDirectoryCreationAfterDeleteRequested,
            this, [this]() {
                dirCreator_->queueDirectoriesForUpload(state_.currentFolderOp.sourcePath,
                                                       state_.currentFolderOp.targetPath);
                if (!state_.pendingMkdirs.isEmpty()) {
                    transitionTo(QueueState::CreatingDirectories);
                    dirCreator_->createNextDirectory();
                } else {
                    finishDirectoryCreation();
                }
            });
}

TransferManager::~TransferManager()
{
    disconnectFtpClient(ftpClient_, this);
}

// ============================================================================
// Model notification helpers
// ============================================================================

void TransferManager::notifyBeginInsert(int first, int last)
{
    if (modelCallbacks_.beginInsertRows)
        modelCallbacks_.beginInsertRows(first, last);
}

void TransferManager::notifyEndInsert()
{
    if (modelCallbacks_.endInsertRows)
        modelCallbacks_.endInsertRows();
}

void TransferManager::notifyDataChanged(int row)
{
    if (modelCallbacks_.dataChangedRow)
        modelCallbacks_.dataChangedRow(row);
}

void TransferManager::notifyBeginReset()
{
    if (modelCallbacks_.beginResetModel)
        modelCallbacks_.beginResetModel();
}

void TransferManager::notifyEndReset()
{
    if (modelCallbacks_.endResetModel)
        modelCallbacks_.endResetModel();
}

void TransferManager::setModelCallbacks(const ModelCallbacks &callbacks)
{
    modelCallbacks_ = callbacks;
    if (!modelCallbacks_.beginResetModel)
        qCDebug(LogTransfer) << "TransferManager: model callbacks not set (headless mode)";
}

void TransferManager::setLocalFileSystem(ILocalFileSystemService *fs)
{
    localFs_ = fs;
    scanCoordinator_->setLocalFileSystem(fs);
    dirCreator_->setLocalFileSystem(fs);
    folderCoordinator_->setLocalFileSystem(fs);
}

// ============================================================================
// Event queue
// ============================================================================

void TransferManager::scheduleProcessNext()
{
    eventProcessor_->schedule([this]() { processNext(); });
}

void TransferManager::flushEventQueue()
{
    eventProcessor_->flush();
}

// ============================================================================
// State machine
// ============================================================================

void TransferManager::transitionTo(QueueState newState)
{
    if (state_.queueState == newState) {
        return;
    }

    qCDebug(LogTransfer) << "TransferManager: State transition"
                         << queueStateToString(state_.queueState) << "->"
                         << queueStateToString(newState);

    state_.queueState = newState;
}

// ============================================================================
// FTP client
// ============================================================================

void TransferManager::setFtpClient(IFtpClient *client)
{
    ftpClient_ = client;

    scanCoordinator_->setFtpClient(client);
    dirCreator_->setFtpClient(client);
    folderCoordinator_->setFtpClient(client);
    ftpHandler_->setFtpClient(client);
    deleteHandler_->setFtpClient(client);
}

// ============================================================================
// Enqueue helpers
// ============================================================================

int TransferManager::findBatchIndex(int batchId) const
{
    for (int i = 0; i < state_.batches.size(); ++i) {
        if (state_.batches[i].batchId == batchId)
            return i;
    }
    return -1;
}

void TransferManager::activateAndSchedule(int batchIdx)
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

void TransferManager::enqueueUpload(const QString &localPath, const QString &remotePath,
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
        qCWarning(LogTransfer) << "TransferManager::enqueueUpload - no valid batch";
        emit operationFailed(QFileInfo(localPath).fileName(), tr("Failed to create upload batch"));
        return;
    }

    TransferItem item;
    item.localPath = localPath;
    item.remotePath = remotePath;
    item.operationType = OperationType::Upload;
    item.status = TransferItem::Status::Pending;
    item.totalBytes = localFs_->fileSize(localPath);
    item.batchId = state_.batches[batchIdx].batchId;

    notifyBeginInsert(state_.items.size(), state_.items.size());
    auto enqResult = transfer::enqueueItem(state_, item, batchIdx);
    state_ = enqResult.newState;
    batchIdx = enqResult.batchIdx;
    notifyEndInsert();
    activateAndSchedule(batchIdx);
}

void TransferManager::enqueueDownload(const QString &remotePath, const QString &localPath,
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
        qCWarning(LogTransfer) << "TransferManager::enqueueDownload - no valid batch";
        emit operationFailed(QFileInfo(remotePath).fileName(),
                             tr("Failed to create download batch"));
        return;
    }

    TransferItem item;
    item.localPath = localPath;
    item.remotePath = remotePath;
    item.operationType = OperationType::Download;
    item.status = TransferItem::Status::Pending;
    item.batchId = state_.batches[batchIdx].batchId;

    notifyBeginInsert(state_.items.size(), state_.items.size());
    auto enqResult = transfer::enqueueItem(state_, item, batchIdx);
    state_ = enqResult.newState;
    batchIdx = enqResult.batchIdx;
    notifyEndInsert();
    activateAndSchedule(batchIdx);
}

// ============================================================================
// Recursive folder operations
// ============================================================================

void TransferManager::enqueueRecursiveUpload(const QString &localDir, const QString &remoteDir)
{
    if (!ftpClient_ || !ftpClient_->isConnected()) {
        qCWarning(LogTransfer) << "enqueueRecursiveUpload skipped: FTP not connected";
        emit operationFailed(QFileInfo(localDir).fileName(), tr("Not connected to device"));
        return;
    }

    if (!localFs_->directoryExists(localDir)) {
        emit operationFailed(QFileInfo(localDir).fileName(), tr("Local directory does not exist"));
        return;
    }

    folderCoordinator_->enqueueRecursive(OperationType::Upload, localDir, remoteDir);
}

void TransferManager::enqueueRecursiveDownload(const QString &remoteDir, const QString &localDir)
{
    if (!ftpClient_ || !ftpClient_->isConnected()) {
        qCWarning(LogTransfer) << "enqueueRecursiveDownload skipped: FTP not connected";
        emit operationFailed(QFileInfo(remoteDir).fileName(), tr("Not connected to device"));
        return;
    }

    QString normalizedRemote = transfer::normalizePath(remoteDir);

    folderCoordinator_->enqueueRecursive(OperationType::Download, normalizedRemote, localDir);
}

void TransferManager::respondToFolderExists(FolderExistsResponse response)
{
    folderCoordinator_->respondToFolderExists(response);
}

// ============================================================================
// Coordinator helpers
// ============================================================================

void TransferManager::finishDirectoryCreation()
{
    qCDebug(LogTransfer) << "TransferManager: All directories created, queueing files for upload";

    if (TransferBatch *batch = findBatch(state_.currentFolderOp.batchId)) {
        batch->scanned = true;
    }

    const QString &sourcePath = state_.currentFolderOp.sourcePath;
    if (!localFs_->directoryExists(sourcePath)) {
        qCWarning(LogTransfer) << "TransferManager: Source directory doesn't exist:" << sourcePath;
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

    qCDebug(LogTransfer) << "TransferManager: Queued" << fileCount << "files for upload";

    if (fileCount == 0) {
        if (findBatch(state_.currentFolderOp.batchId)) {
            qCDebug(LogTransfer) << "TransferManager: Empty folder upload batch"
                                 << state_.currentFolderOp.batchId;
            completeBatch(state_.currentFolderOp.batchId);
            return;
        }
    }

    transitionTo(QueueState::Idle);
    scheduleProcessNext();
}

// ============================================================================
// Delete operations (delegated to deleteHandler_)
// ============================================================================

void TransferManager::enqueueDelete(const QString &remotePath, bool isDirectory)
{
    deleteHandler_->enqueueDelete(remotePath, isDirectory);
}

void TransferManager::enqueueRecursiveDelete(const QString &remotePath)
{
    deleteHandler_->enqueueRecursiveDelete(remotePath);
}

// ============================================================================
// Core processing loop
// ============================================================================

void TransferManager::processNext()
{
    qCDebug(LogTransfer) << "TransferManager: processNext, state:"
                         << queueStateToString(state_.queueState);

    bool ftpReady = ftpClient_ && ftpClient_->isConnected();

    auto decision = transfer::decideNextAction(
        state_, ftpReady, [this](const QString &path) { return localFs_->fileExists(path); });

    switch (decision.action) {
    case transfer::ProcessNextAction::Blocked:
        qCDebug(LogTransfer) << "TransferManager: processNext blocked by state:"
                             << queueStateToString(state_.queueState);
        emit statusMessage(tr("Transfer queue is busy"));
        return;

    case transfer::ProcessNextAction::NoFtpClient:
        qCDebug(LogTransfer) << "TransferManager: FTP client not ready";
        emit operationFailed(QString(), tr("Not connected to device"));
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

        notifyDataChanged(i);
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
        qCDebug(LogTransfer) << "TransferManager: No more pending items";
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

void TransferManager::respondToOverwrite(OverwriteResponse response)
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
            notifyDataChanged(i);
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

int TransferManager::createBatch(OperationType type, const QString &description,
                                 const QString &folderName, const QString &sourcePath)
{
    return batchManager_->createBatch(type, description, folderName, sourcePath);
}

void TransferManager::activateNextBatch()
{
    batchManager_->activateNextBatch();
}

void TransferManager::completeBatch(int batchId)
{
    batchManager_->completeBatch(batchId);
}

void TransferManager::purgeBatch(int batchId)
{
    batchManager_->purgeBatch(batchId);
}

void TransferManager::emitBatchProgressAndComplete(int batchId, bool batchIsComplete,
                                                   bool includeFailed)
{
    batchManager_->emitBatchProgressAndComplete(batchId, batchIsComplete, includeFailed);
}

void TransferManager::markCurrentComplete(TransferItem::Status status)
{
    if (state_.currentIndex < 0 || state_.currentIndex >= state_.items.size()) {
        qCWarning(LogTransfer) << "markCurrentComplete: invalid index" << state_.currentIndex;
        return;
    }

    int completedIndex = state_.currentIndex;
    auto result = transfer::markItemComplete(state_, completedIndex, status);
    state_ = result.newState;

    notifyDataChanged(completedIndex);

    if (result.batchId >= 0) {
        emitBatchProgressAndComplete(result.batchId, result.batchIsComplete);
    }
}

TransferBatch *TransferManager::findBatch(int batchId)
{
    return batchManager_->findBatch(batchId);
}

const TransferBatch *TransferManager::findBatch(int batchId) const
{
    return batchManager_->findBatch(batchId);
}

TransferBatch *TransferManager::activeBatch()
{
    return batchManager_->activeBatch();
}

int TransferManager::findItemIndex(const QString &localPath, const QString &remotePath) const
{
    return transfer::findItemIndex(state_, localPath, remotePath);
}

// ============================================================================
// Queue operations
// ============================================================================

void TransferManager::clear()
{
    notifyBeginReset();
    state_ = transfer::clearAll(state_);
    notifyEndReset();

    folderCoordinator_->stopDebounce();
    emit queueChanged();
}

void TransferManager::cancelAll()
{
    if (ftpClient_ && (state_.queueState == QueueState::Transferring ||
                       state_.queueState == QueueState::Deleting)) {
        ftpClient_->abort();
    }

    state_ = transfer::cancelAllItems(state_);

    folderCoordinator_->stopDebounce();

    for (int i = 0; i < state_.items.size(); ++i) {
        notifyDataChanged(i);
    }
    emit queueChanged();
    emit operationsCancelled();
}

void TransferManager::cancelBatch(int batchId)
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
        notifyDataChanged(i);
    }
    emit queueChanged();

    if (result.wasActiveBatch) {
        scheduleProcessNext();
    }
}

// ============================================================================
// Query methods
// ============================================================================

int TransferManager::pendingCount() const
{
    return transfer::pendingCount(state_);
}

int TransferManager::activeCount() const
{
    return transfer::activeCount(state_);
}

int TransferManager::activeAndPendingCount() const
{
    return transfer::activeAndPendingCount(state_);
}

bool TransferManager::isScanningForDelete() const
{
    return state_.queueState == QueueState::Scanning && !state_.pendingDeleteScans.isEmpty();
}

bool TransferManager::hasActiveBatch() const
{
    return batchManager_->hasActiveBatch();
}

int TransferManager::queuedBatchCount() const
{
    return batchManager_->queuedBatchCount();
}

bool TransferManager::isPathBeingTransferred(const QString &path, OperationType type) const
{
    return transfer::isPathBeingTransferred(state_, path, type);
}

BatchProgress TransferManager::activeBatchProgress() const
{
    return batchManager_->activeBatchProgress();
}

BatchProgress TransferManager::batchProgress(int batchId) const
{
    return batchManager_->batchProgress(batchId);
}

QList<int> TransferManager::allBatchIds() const
{
    return batchManager_->allBatchIds();
}

// ============================================================================
// Timeout handling
// ============================================================================

void TransferManager::startOperationTimeout()
{
    timeoutManager_->start();
}

void TransferManager::stopOperationTimeout()
{
    timeoutManager_->stop();
}

void TransferManager::onOperationTimeout()
{
    qCDebug(LogTransfer) << "TransferManager: Operation timeout!";

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
        notifyDataChanged(originalIndex);
        emit operationFailed(fileName, errorMessage);
    }

    scheduleProcessNext();
}
