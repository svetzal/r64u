#include "transfermanager.h"

#include "singlefileenqueuehandler.h"
#include "transferdispatchhandler.h"
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

    ftpHandler_ = new TransferFtpHandler(state_, this);
    ftpHandler_->setTimeoutManager(timeoutManager_);
    ftpHandler_->setDirCreator(dirCreator_);
    ftpHandler_->setScanCoordinator(scanCoordinator_);

    dispatchHandler_ = new TransferDispatchHandler(state_, this);
    dispatchHandler_->setLocalFileSystem(localFs_);
    dispatchHandler_->setFolderCoordinator(folderCoordinator_);

    deleteHandler_->setScanCoordinator(scanCoordinator_);
    deleteHandler_->setDirCreator(dirCreator_);
    deleteHandler_->setCreateBatchCallback([this](OperationType type, const QString &description,
                                                  const QString &folderName,
                                                  const QString &sourcePath) {
        return createBatch(type, description, folderName, sourcePath);
    });
    folderCoordinator_->setCreateBatchCallback(
        [this](transfer::OperationType type, const QString &description, const QString &folderName,
               const QString &sourcePath) {
            return createBatch(type, description, folderName, sourcePath);
        });

    enqueueHandler_ = new SingleFileEnqueueHandler(state_, localFs_, this);
    enqueueHandler_->setCreateBatchCallback([this](OperationType type, const QString &description,
                                                   const QString &folderName,
                                                   const QString &sourcePath) {
        return createBatch(type, description, folderName, sourcePath);
    });
    enqueueHandler_->setCompleteBatchCallback([this](int batchId) { completeBatch(batchId); });

    connectCollaborators();
}

void TransferManager::connectCollaborators()
{
    // --- BatchManager ---
    connect(batchManager_, &BatchManager::modelAboutToReset, this,
            &TransferManager::modelAboutToReset);
    connect(batchManager_, &BatchManager::modelReset, this, &TransferManager::modelReset);
    connect(batchManager_, &BatchManager::rowsAboutToBeRemoved, this,
            &TransferManager::itemsAboutToBeRemoved);
    connect(batchManager_, &BatchManager::rowsRemoved, this, &TransferManager::itemsRemoved);
    connect(batchManager_, &BatchManager::stopTimeoutRequested, this,
            &TransferManager::stopOperationTimeout);
    connect(batchManager_, &BatchManager::folderOpCompleteRequested, this,
            [this]() { folderCoordinator_->onFolderOperationComplete(); });
    connect(batchManager_, &BatchManager::scheduleProcessNextRequested, this,
            [this]() { scheduleProcessNext(); });
    connect(batchManager_, &BatchManager::batchStarted, this, &TransferManager::batchStarted);
    connect(batchManager_, &BatchManager::batchProgressUpdate, this,
            &TransferManager::batchProgressUpdate);
    connect(batchManager_, &BatchManager::batchCompleted, this, &TransferManager::batchCompleted);
    connect(batchManager_, &BatchManager::allOperationsCompleted, this,
            &TransferManager::allOperationsCompleted);
    connect(batchManager_, &BatchManager::queueChanged, this, &TransferManager::queueChanged);

    // --- TransferTimeoutManager ---
    connect(timeoutManager_, &TransferTimeoutManager::operationTimedOut, this,
            &TransferManager::onOperationTimeout);

    // --- RecursiveScanCoordinator ---
    connect(scanCoordinator_, &RecursiveScanCoordinator::downloadFileDiscovered, this,
            [this](const QString &remotePath, const QString &localPath, int batchId) {
                enqueueDownload(remotePath, localPath, batchId);
            });
    connect(scanCoordinator_, &RecursiveScanCoordinator::completeBatchRequested, this,
            [this](int batchId) { completeBatch(batchId); });
    connect(scanCoordinator_, &RecursiveScanCoordinator::scheduleProcessNextRequested, this,
            &TransferManager::onScanCompleted);
    connect(scanCoordinator_, &RecursiveScanCoordinator::statusMessage, this,
            &TransferManager::statusMessage);
    connect(scanCoordinator_, &RecursiveScanCoordinator::scanningStarted, this,
            &TransferManager::scanningStarted);
    connect(scanCoordinator_, &RecursiveScanCoordinator::scanningProgress, this,
            &TransferManager::scanningProgress);
    connect(scanCoordinator_, &RecursiveScanCoordinator::deleteScanComplete, this,
            &TransferManager::onDeleteScanComplete);
    connect(scanCoordinator_, &RecursiveScanCoordinator::folderCheckComplete, this,
            [this](const QString & /*path*/) { folderCoordinator_->resumeAfterFolderCheck(); });
    connect(scanCoordinator_, &RecursiveScanCoordinator::uploadCheckFileExists, this,
            [this](const QString &fileName) {
                emit overwriteConfirmationNeeded(fileName, OperationType::Upload);
            });
    connect(scanCoordinator_, &RecursiveScanCoordinator::uploadCheckNoConflict, this,
            [this]() { scheduleProcessNext(); });

    // --- RemoteDirectoryCoordinator ---
    connect(dirCreator_, &RemoteDirectoryCoordinator::directoryCreationProgress, this,
            &TransferManager::directoryCreationProgress);
    connect(dirCreator_, &RemoteDirectoryCoordinator::allDirectoriesCreated, this,
            [this]() { enqueueHandler_->finishDirectoryCreation(); });

    // --- FolderOperationCoordinator ---
    connect(folderCoordinator_, &FolderOperationCoordinator::startDownloadScanRequested, this,
            &TransferManager::onStartDownloadScanRequested);
    connect(folderCoordinator_, &FolderOperationCoordinator::startDirectoryCreationRequested, this,
            &TransferManager::onStartDirectoryCreationRequested);
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

    // --- TransferFtpHandler ---
    connect(ftpHandler_, &TransferFtpHandler::itemDataChanged, this,
            [this](int row) { emit itemDataChanged(row); });
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

    // --- TransferDispatchHandler ---
    connect(dispatchHandler_, &TransferDispatchHandler::operationStarted, this,
            &TransferManager::operationStarted);
    connect(dispatchHandler_, &TransferDispatchHandler::itemDataChanged, this,
            &TransferManager::itemDataChanged);
    connect(dispatchHandler_, &TransferDispatchHandler::statusMessage, this,
            &TransferManager::statusMessage);
    connect(dispatchHandler_, &TransferDispatchHandler::operationFailed, this,
            &TransferManager::operationFailed);
    connect(dispatchHandler_, &TransferDispatchHandler::overwriteConfirmationNeeded, this,
            &TransferManager::overwriteConfirmationNeeded);
    connect(dispatchHandler_, &TransferDispatchHandler::allOperationsCompleted, this,
            &TransferManager::allOperationsCompleted);
    connect(dispatchHandler_, &TransferDispatchHandler::startTimeoutRequested, this,
            [this]() { startOperationTimeout(); });
    connect(dispatchHandler_, &TransferDispatchHandler::stopTimeoutRequested, this,
            [this]() { stopOperationTimeout(); });
    connect(dispatchHandler_, &TransferDispatchHandler::scheduleProcessNextRequested, this,
            [this]() { scheduleProcessNext(); });

    // --- TransferDeleteHandler ---
    connect(deleteHandler_, &TransferDeleteHandler::rowsAboutToBeInserted, this,
            &TransferManager::itemsAboutToBeInserted);
    connect(deleteHandler_, &TransferDeleteHandler::rowsInserted, this,
            &TransferManager::itemsInserted);
    connect(deleteHandler_, &TransferDeleteHandler::transitionToRequested, this,
            [this](QueueState s) { transitionTo(s); });
    connect(deleteHandler_, &TransferDeleteHandler::scheduleProcessNextRequested, this,
            [this]() { scheduleProcessNext(); });
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
            this, &TransferManager::onStartDirectoryCreationAfterDeleteRequested);

    // --- SingleFileEnqueueHandler ---
    connect(enqueueHandler_, &SingleFileEnqueueHandler::itemsAboutToBeInserted, this,
            &TransferManager::itemsAboutToBeInserted);
    connect(enqueueHandler_, &SingleFileEnqueueHandler::itemsInserted, this,
            &TransferManager::itemsInserted);
    connect(enqueueHandler_, &SingleFileEnqueueHandler::queueChanged, this,
            &TransferManager::queueChanged);
    connect(enqueueHandler_, &SingleFileEnqueueHandler::batchStarted, this,
            &TransferManager::batchStarted);
    connect(enqueueHandler_, &SingleFileEnqueueHandler::operationFailed, this,
            &TransferManager::operationFailed);
    connect(enqueueHandler_, &SingleFileEnqueueHandler::scheduleProcessNextRequested, this,
            [this]() { scheduleProcessNext(); });
    connect(enqueueHandler_, &SingleFileEnqueueHandler::transitionToIdleRequested, this,
            [this]() { transitionTo(QueueState::Idle); });
}

TransferManager::~TransferManager()
{
    disconnectFtpClient(ftpClient_, this);
}

// ============================================================================
// Orchestration slots
// ============================================================================

void TransferManager::onScanCompleted()
{
    if (TransferBatch *batch = findBatch(state_.currentFolderOp.batchId)) {
        batch->scanned = true;
    }
    transitionTo(QueueState::Idle);
    scheduleProcessNext();
}

void TransferManager::onDeleteScanComplete()
{
    transitionTo(QueueState::Deleting);
    emit queueChanged();
    deleteHandler_->processNextDelete();
}

void TransferManager::onStartDownloadScanRequested(const QString &remotePath,
                                                   const QString &localBase,
                                                   const QString &remoteBase, int batchId)
{
    transitionTo(QueueState::Scanning);
    scanCoordinator_->startDownloadScan(remotePath, localBase, remoteBase, batchId);
}

void TransferManager::onStartDirectoryCreationRequested(const QString &localDir,
                                                        const QString &remoteDir)
{
    dirCreator_->queueDirectoriesForUpload(localDir, remoteDir);
    if (!state_.pendingMkdirs.isEmpty()) {
        transitionTo(QueueState::CreatingDirectories);
        dirCreator_->createNextDirectory();
    } else {
        enqueueHandler_->finishDirectoryCreation();
    }
}

void TransferManager::onStartDirectoryCreationAfterDeleteRequested()
{
    dirCreator_->queueDirectoriesForUpload(state_.currentFolderOp.sourcePath,
                                           state_.currentFolderOp.targetPath);
    if (!state_.pendingMkdirs.isEmpty()) {
        transitionTo(QueueState::CreatingDirectories);
        dirCreator_->createNextDirectory();
    } else {
        enqueueHandler_->finishDirectoryCreation();
    }
}

void TransferManager::setLocalFileSystem(ILocalFileSystemService *fs)
{
    localFs_ = fs;
    scanCoordinator_->setLocalFileSystem(fs);
    dirCreator_->setLocalFileSystem(fs);
    folderCoordinator_->setLocalFileSystem(fs);
    dispatchHandler_->setLocalFileSystem(fs);
    enqueueHandler_->setLocalFileSystem(fs);
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
    dispatchHandler_->transitionTo(newState);
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
    dispatchHandler_->setFtpClient(client);
}

// ============================================================================
// Single-file enqueue operations (delegated to enqueueHandler_)
// ============================================================================

void TransferManager::enqueueUpload(const QString &localPath, const QString &remotePath,
                                    int targetBatchId)
{
    enqueueHandler_->enqueueUpload(localPath, remotePath, targetBatchId);
}

void TransferManager::enqueueDownload(const QString &remotePath, const QString &localPath,
                                      int targetBatchId)
{
    enqueueHandler_->enqueueDownload(remotePath, localPath, targetBatchId);
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
    dispatchHandler_->processNext();
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
            emit itemDataChanged(i);
        }
    }

    if (response == OverwriteResponse::Skip && affectedBatchId >= 0) {
        if (batchManager_->handleSkipBatchCompletion(affectedBatchId)) {
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

    emit itemDataChanged(completedIndex);

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
    emit modelAboutToReset();
    state_ = transfer::clearAll(state_);
    emit modelReset();

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
        emit itemDataChanged(i);
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
        emit itemDataChanged(i);
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
        emit itemDataChanged(originalIndex);
        emit operationFailed(fileName, errorMessage);
    }

    scheduleProcessNext();
}
