#include "transferqueue.h"

#include "../services/ftpclientmixin.h"
#include "../services/iftpclient.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

TransferQueue::TransferQueue(QObject *parent)
    : QAbstractListModel(parent), operationTimeoutTimer_(new QTimer(this)),
      scanCoordinator_(new RecursiveScanCoordinator(state_, nullptr, this)),
      dirCreator_(new RemoteDirectoryCreator(state_, nullptr, this)),
      folderCoordinator_(new FolderOperationCoordinator(state_, nullptr, this))
{
    operationTimeoutTimer_->setSingleShot(true);
    connect(operationTimeoutTimer_, &QTimer::timeout, this, &TransferQueue::onOperationTimeout);

    // --- RecursiveScanCoordinator connections ---
    connect(scanCoordinator_, &RecursiveScanCoordinator::downloadFileDiscovered, this,
            [this](const QString &remotePath, const QString &localPath, int batchId) {
                enqueueDownload(remotePath, localPath, batchId);
            });
    connect(scanCoordinator_, &RecursiveScanCoordinator::completeBatchRequested, this,
            [this](int batchId) { completeBatch(batchId); });
    connect(scanCoordinator_, &RecursiveScanCoordinator::scheduleProcessNextRequested, this,
            [this]() {
                // Mark batch as scanned before transitioning
                if (TransferBatch *batch = findBatch(state_.currentFolderOp.batchId)) {
                    batch->scanned = true;
                }
                transitionTo(QueueState::Idle);
                scheduleProcessNext();
            });
    connect(scanCoordinator_, &RecursiveScanCoordinator::statusMessage, this,
            &TransferQueue::statusMessage);
    connect(scanCoordinator_, &RecursiveScanCoordinator::scanningStarted, this,
            &TransferQueue::scanningStarted);
    connect(scanCoordinator_, &RecursiveScanCoordinator::scanningProgress, this,
            &TransferQueue::scanningProgress);
    connect(scanCoordinator_, &RecursiveScanCoordinator::deleteScanComplete, this,
            &TransferQueue::onDeleteScanComplete);
    connect(scanCoordinator_, &RecursiveScanCoordinator::folderCheckComplete, this,
            [this](const QString & /*path*/) {
                // After folder existence check, resume folder confirmation logic
                folderCoordinator_->resumeAfterFolderCheck();
            });
    connect(scanCoordinator_, &RecursiveScanCoordinator::uploadCheckFileExists, this,
            [this](const QString &fileName) {
                emit overwriteConfirmationNeeded(fileName, OperationType::Upload);
            });
    connect(scanCoordinator_, &RecursiveScanCoordinator::uploadCheckNoConflict, this,
            [this]() { scheduleProcessNext(); });

    // --- RemoteDirectoryCreator connections ---
    connect(dirCreator_, &RemoteDirectoryCreator::directoryCreationProgress, this,
            &TransferQueue::directoryCreationProgress);
    connect(dirCreator_, &RemoteDirectoryCreator::allDirectoriesCreated, this,
            &TransferQueue::onAllDirectoriesCreated);

    // --- FolderOperationCoordinator connections ---
    folderCoordinator_->setCreateBatchCallback(
        [this](transfer::OperationType type, const QString &description, const QString &folderName,
               const QString &sourcePath) {
            return createBatch(type, description, folderName, sourcePath);
        });
    connect(folderCoordinator_, &FolderOperationCoordinator::startDownloadScanRequested, this,
            &TransferQueue::onStartDownloadScanRequested);
    connect(folderCoordinator_, &FolderOperationCoordinator::startDirectoryCreationRequested, this,
            &TransferQueue::onStartDirectoryCreationRequested);
    connect(folderCoordinator_, &FolderOperationCoordinator::startDeleteRequested, this,
            &TransferQueue::onStartDeleteRequested);
    connect(folderCoordinator_, &FolderOperationCoordinator::pendingUploadAfterDeleteSet, this,
            &TransferQueue::onPendingUploadAfterDeleteSet);
    connect(folderCoordinator_, &FolderOperationCoordinator::folderConfirmationNeeded, this,
            &TransferQueue::folderExistsConfirmationNeeded);
    connect(folderCoordinator_, &FolderOperationCoordinator::operationStarted, this,
            &TransferQueue::operationStarted);
    connect(folderCoordinator_, &FolderOperationCoordinator::allOperationsCompleted, this,
            &TransferQueue::allOperationsCompleted);
    connect(folderCoordinator_, &FolderOperationCoordinator::operationsCancelled, this,
            &TransferQueue::operationsCancelled);
    connect(folderCoordinator_, &FolderOperationCoordinator::statusMessage, this,
            &TransferQueue::statusMessage);
    connect(folderCoordinator_, &FolderOperationCoordinator::scheduleProcessNextRequested, this,
            [this]() { scheduleProcessNext(); });
}

TransferQueue::~TransferQueue()
{
    disconnectFtpClient(ftpClient_, this);
}

void TransferQueue::scheduleProcessNext()
{
    eventQueue_.enqueue([this]() { processNext(); });

    if (!eventProcessingScheduled_) {
        eventProcessingScheduled_ = true;
        QTimer::singleShot(0, this, &TransferQueue::processEventQueue);
    }
}

void TransferQueue::processEventQueue()
{
    eventProcessingScheduled_ = false;

    if (processingEvents_) {
        if (!eventQueue_.isEmpty() && !eventProcessingScheduled_) {
            eventProcessingScheduled_ = true;
            QTimer::singleShot(0, this, &TransferQueue::processEventQueue);
        }
        return;
    }

    processingEvents_ = true;
    while (!eventQueue_.isEmpty()) {
        auto event = eventQueue_.dequeue();
        event();
    }
    processingEvents_ = false;
}

void TransferQueue::flushEventQueue()
{
    if (processingEvents_) {
        return;
    }

    eventProcessingScheduled_ = false;
    processingEvents_ = true;

    while (!eventQueue_.isEmpty()) {
        auto event = eventQueue_.dequeue();
        event();
    }

    processingEvents_ = false;
}

void TransferQueue::transitionTo(QueueState newState)
{
    if (state_.queueState == newState) {
        return;
    }

    qDebug() << "TransferQueue: State transition" << queueStateToString(state_.queueState) << "->"
             << queueStateToString(newState);

    state_.queueState = newState;
}

void TransferQueue::setFtpClient(IFtpClient *client)
{
    disconnectFtpClient(ftpClient_, this);

    ftpClient_ = client;

    scanCoordinator_->setFtpClient(client);
    dirCreator_->setFtpClient(client);
    folderCoordinator_->setFtpClient(client);

    if (ftpClient_) {
        connect(ftpClient_, &IFtpClient::uploadProgress, this, &TransferQueue::onUploadProgress);
        connect(ftpClient_, &IFtpClient::uploadFinished, this, &TransferQueue::onUploadFinished);
        connect(ftpClient_, &IFtpClient::downloadProgress, this,
                &TransferQueue::onDownloadProgress);
        connect(ftpClient_, &IFtpClient::downloadFinished, this,
                &TransferQueue::onDownloadFinished);
        connect(ftpClient_, &IFtpClient::error, this, &TransferQueue::onFtpError);
        connect(ftpClient_, &IFtpClient::directoryCreated, this,
                &TransferQueue::onFtpDirectoryCreated);
        connect(ftpClient_, &IFtpClient::directoryListed, this, &TransferQueue::onDirectoryListed);
        connect(ftpClient_, &IFtpClient::fileRemoved, this, &TransferQueue::onFileRemoved);
    }
}

// ============================================================================
// Single-file enqueue operations
// ============================================================================

void TransferQueue::enqueueUpload(const QString &localPath, const QString &remotePath,
                                  int targetBatchId)
{
    int batchIdx = -1;
    if (targetBatchId >= 0) {
        for (int i = 0; i < state_.batches.size(); ++i) {
            if (state_.batches[i].batchId == targetBatchId) {
                batchIdx = i;
                break;
            }
        }
    }

    if (batchIdx < 0) {
        batchIdx = state_.activeBatchIndex;
        if (batchIdx < 0 || state_.batches[batchIdx].operationType != OperationType::Upload) {
            QString fileName = QFileInfo(localPath).fileName();
            QString sourcePath = state_.currentFolderOp.sourcePath.isEmpty()
                                     ? QString()
                                     : state_.currentFolderOp.sourcePath;
            int batchId = createBatch(OperationType::Upload, tr("Uploading %1").arg(fileName),
                                      fileName, sourcePath);
            for (int i = 0; i < state_.batches.size(); ++i) {
                if (state_.batches[i].batchId == batchId) {
                    batchIdx = i;
                    break;
                }
            }
        }
    }

    if (batchIdx < 0 || batchIdx >= state_.batches.size()) {
        qWarning() << "TransferQueue::enqueueUpload - no valid batch";
        return;
    }

    TransferItem item;
    item.localPath = localPath;
    item.remotePath = remotePath;
    item.operationType = OperationType::Upload;
    item.status = TransferItem::Status::Pending;
    item.totalBytes = QFileInfo(localPath).size();
    item.batchId = state_.batches[batchIdx].batchId;

    beginInsertRows(QModelIndex(), state_.items.size(), state_.items.size());
    state_.items.append(item);
    endInsertRows();

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

void TransferQueue::enqueueDownload(const QString &remotePath, const QString &localPath,
                                    int targetBatchId)
{
    int batchIdx = -1;

    if (targetBatchId >= 0) {
        for (int i = 0; i < state_.batches.size(); ++i) {
            if (state_.batches[i].batchId == targetBatchId) {
                batchIdx = i;
                break;
            }
        }
    }

    if (batchIdx < 0) {
        batchIdx = state_.activeBatchIndex;
        if (batchIdx < 0 || state_.batches[batchIdx].operationType != OperationType::Download) {
            QString fileName = QFileInfo(remotePath).fileName();
            QString sourcePath = (state_.queueState == QueueState::Scanning)
                                     ? state_.currentFolderOp.sourcePath
                                     : QString();
            int batchId = createBatch(OperationType::Download, tr("Downloading %1").arg(fileName),
                                      fileName, sourcePath);
            for (int i = 0; i < state_.batches.size(); ++i) {
                if (state_.batches[i].batchId == batchId) {
                    batchIdx = i;
                    break;
                }
            }
        }
    }

    if (batchIdx < 0 || batchIdx >= state_.batches.size()) {
        qWarning() << "TransferQueue::enqueueDownload - no valid batch";
        return;
    }

    TransferItem item;
    item.localPath = localPath;
    item.remotePath = remotePath;
    item.operationType = OperationType::Download;
    item.status = TransferItem::Status::Pending;
    item.batchId = state_.batches[batchIdx].batchId;

    beginInsertRows(QModelIndex(), state_.items.size(), state_.items.size());
    state_.items.append(item);
    endInsertRows();

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

// ============================================================================
// Recursive folder operations
// ============================================================================

void TransferQueue::enqueueRecursiveUpload(const QString &localDir, const QString &remoteDir)
{
    if (!ftpClient_ || !ftpClient_->isConnected())
        return;

    if (!QDir(localDir).exists())
        return;

    folderCoordinator_->enqueueRecursive(OperationType::Upload, localDir, remoteDir);
}

void TransferQueue::enqueueRecursiveDownload(const QString &remoteDir, const QString &localDir)
{
    if (!ftpClient_ || !ftpClient_->isConnected())
        return;

    QString normalizedRemote = remoteDir;
    while (normalizedRemote.endsWith('/') && normalizedRemote.length() > 1) {
        normalizedRemote.chop(1);
    }

    folderCoordinator_->enqueueRecursive(OperationType::Download, normalizedRemote, localDir);
}

void TransferQueue::respondToFolderExists(FolderExistsResponse response)
{
    folderCoordinator_->respondToFolderExists(response);
}

// ============================================================================
// Coordinator slot implementations
// ============================================================================

void TransferQueue::onStartDownloadScanRequested(const QString &remotePath,
                                                 const QString &localBase,
                                                 const QString &remoteBase, int batchId)
{
    transitionTo(QueueState::Scanning);
    scanCoordinator_->startDownloadScan(remotePath, localBase, remoteBase, batchId);
}

void TransferQueue::onStartDirectoryCreationRequested(const QString &localDir,
                                                      const QString &remoteDir)
{
    dirCreator_->queueDirectoriesForUpload(localDir, remoteDir);

    if (!state_.pendingMkdirs.isEmpty()) {
        transitionTo(QueueState::CreatingDirectories);
        dirCreator_->createNextDirectory();
    } else {
        // No directories to create, mark batch as scanned and process files
        if (TransferBatch *batch = findBatch(state_.currentFolderOp.batchId)) {
            batch->scanned = true;
        }
        finishDirectoryCreation();
    }
}

void TransferQueue::onStartDeleteRequested(const QString &remotePath)
{
    enqueueRecursiveDelete(remotePath);
}

void TransferQueue::onPendingUploadAfterDeleteSet(const QString &targetPath)
{
    enqueueRecursiveDelete(targetPath);
}

void TransferQueue::onAllDirectoriesCreated()
{
    finishDirectoryCreation();
}

void TransferQueue::onDeleteScanComplete()
{
    transitionTo(QueueState::Deleting);
    emit queueChanged();
    processNextDelete();
}

void TransferQueue::finishDirectoryCreation()
{
    qDebug() << "TransferQueue: All directories created, queueing files for upload";

    // Mark batch as scanned
    if (TransferBatch *batch = findBatch(state_.currentFolderOp.batchId)) {
        batch->scanned = true;
    }

    // Queue all files for upload
    QDir dir(state_.currentFolderOp.sourcePath);
    if (!dir.exists()) {
        qWarning() << "TransferQueue: Source directory doesn't exist:"
                   << state_.currentFolderOp.sourcePath;
        return;
    }

    QDirIterator it(state_.currentFolderOp.sourcePath, QDir::Files, QDirIterator::Subdirectories);
    int fileCount = 0;
    while (it.hasNext()) {
        QString filePath = it.next();
        QString relativePath = dir.relativeFilePath(filePath);
        QString remotePath = state_.currentFolderOp.targetPath + '/' + relativePath;

        enqueueUpload(filePath, remotePath, state_.currentFolderOp.batchId);
        fileCount++;
    }

    qDebug() << "TransferQueue: Queued" << fileCount << "files for upload";

    // Check for empty folder (no files)
    if (fileCount == 0) {
        if (findBatch(state_.currentFolderOp.batchId)) {
            qDebug() << "TransferQueue: Empty folder upload batch"
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

void TransferQueue::enqueueDelete(const QString &remotePath, bool isDirectory)
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
        qWarning() << "TransferQueue::enqueueDelete - no valid batch";
        return;
    }

    TransferItem item;
    item.remotePath = remotePath;
    item.operationType = OperationType::Delete;
    item.status = TransferItem::Status::Pending;
    item.isDirectory = isDirectory;
    item.batchId = state_.batches[batchIdx].batchId;

    beginInsertRows(QModelIndex(), state_.items.size(), state_.items.size());
    state_.items.append(item);
    endInsertRows();

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

void TransferQueue::enqueueRecursiveDelete(const QString &remotePath)
{
    if (!ftpClient_ || !ftpClient_->isConnected())
        return;

    QString normalizedPath = remotePath;
    while (normalizedPath.endsWith('/') && normalizedPath.length() > 1) {
        normalizedPath.chop(1);
    }

    if (isPathBeingTransferred(normalizedPath, OperationType::Delete)) {
        qDebug() << "TransferQueue: Ignoring duplicate delete request for" << normalizedPath;
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

void TransferQueue::processNextDelete()
{
    if (!ftpClient_ || !ftpClient_->isConnected()) {
        transitionTo(QueueState::Idle);
        return;
    }

    if (state_.deletedCount >= state_.deleteQueue.size()) {
        qDebug() << "TransferQueue: All deletes complete";
        transitionTo(QueueState::Idle);
        state_.deleteQueue.clear();
        state_.recursiveDeleteBase.clear();
        emit operationCompleted(tr("Deleted %1 items").arg(state_.deletedCount));

        if (state_.pendingUploadAfterDelete) {
            qDebug() << "TransferQueue: Delete completed, starting pending upload";
            state_.pendingUploadAfterDelete = false;

            // Queue directories and start uploading via dirCreator
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
    qDebug() << "TransferQueue: Deleting" << (state_.deletedCount + 1) << "of"
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

void TransferQueue::processNext()
{
    qDebug() << "TransferQueue: processNext, state:" << queueStateToString(state_.queueState);

    bool ftpReady = ftpClient_ && ftpClient_->isConnected();

    auto decision = transfer::decideNextAction(
        state_, ftpReady, [](const QString &path) { return QFileInfo(path).exists(); });

    switch (decision.action) {
    case transfer::ProcessNextAction::Blocked:
        qDebug() << "TransferQueue: processNext blocked by state:"
                 << queueStateToString(state_.queueState);
        return;

    case transfer::ProcessNextAction::NoFtpClient:
        qDebug() << "TransferQueue: FTP client not ready";
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

        emit dataChanged(index(i), index(i));
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
        qDebug() << "TransferQueue: No more pending items";
        stopOperationTimeout();
        state_.currentIndex = -1;
        if (state_.batches.isEmpty()) {
            emit allOperationsCompleted();
        }
        return;
    }
}

// ============================================================================
// FTP callbacks
// ============================================================================

void TransferQueue::onUploadProgress(const QString &file, qint64 sent, qint64 total)
{
    Q_UNUSED(file)
    startOperationTimeout();

    if (state_.currentIndex >= 0 && state_.currentIndex < state_.items.size()) {
        state_.items[state_.currentIndex].bytesTransferred = sent;
        state_.items[state_.currentIndex].totalBytes = total;
        emit dataChanged(index(state_.currentIndex), index(state_.currentIndex));
    }
}

void TransferQueue::onUploadFinished(const QString &localPath, const QString &remotePath)
{
    stopOperationTimeout();

    int idx = findItemIndex(localPath, remotePath);
    if (idx >= 0) {
        // Use the found index, not currentIndex_, in case another operation started
        state_.currentIndex = idx;
        markCurrentComplete(TransferItem::Status::Completed);

        QString fileName = QFileInfo(localPath).fileName();
        emit operationCompleted(fileName);
    }

    // Only transition to Idle if we haven't already started a new operation
    // (e.g., when batch completion triggers a new folder operation)
    if (state_.queueState == QueueState::Transferring) {
        transitionTo(QueueState::Idle);
    }
    emit queueChanged();
    scheduleProcessNext();
}

void TransferQueue::onDownloadProgress(const QString &file, qint64 received, qint64 total)
{
    Q_UNUSED(file)
    startOperationTimeout();

    if (state_.currentIndex >= 0 && state_.currentIndex < state_.items.size()) {
        state_.items[state_.currentIndex].bytesTransferred = received;
        state_.items[state_.currentIndex].totalBytes = total;
        emit dataChanged(index(state_.currentIndex), index(state_.currentIndex));
    }
}

void TransferQueue::onDownloadFinished(const QString &remotePath, const QString &localPath)
{
    stopOperationTimeout();

    int idx = findItemIndex(localPath, remotePath);
    if (idx >= 0) {
        // Use the found index, not currentIndex_, in case another operation started
        state_.currentIndex = idx;
        markCurrentComplete(TransferItem::Status::Completed);

        QString fileName = QFileInfo(remotePath).fileName();
        emit operationCompleted(fileName);
    }

    // Only transition to Idle if we haven't already started a new operation
    // (e.g., when batch completion triggers a new folder operation)
    if (state_.queueState == QueueState::Transferring) {
        transitionTo(QueueState::Idle);
    }
    emit queueChanged();
    scheduleProcessNext();
}

void TransferQueue::onFtpError(const QString &message)
{
    qDebug() << "TransferQueue: onFtpError:" << message
             << "state:" << queueStateToString(state_.queueState);

    stopOperationTimeout();

    int originalIndex = state_.currentIndex;  // Save before pure function resets it

    auto result = transfer::handleFtpError(state_, message);
    state_ = result.newState;

    if (result.isDeleteError) {
        emit operationFailed(result.deleteFileName, message);
        emit queueChanged();
        processNextDelete();
        return;
    }

    if (result.isFolderCreationError) {
        emit operationFailed(result.folderName, message);
        completeBatch(result.folderBatchId);
        return;
    }

    if (result.hasCurrentItem && originalIndex >= 0 && originalIndex < state_.items.size()) {
        emit dataChanged(index(originalIndex), index(originalIndex));
        emit operationFailed(result.transferFileName, message);

        if (result.failedBatchId >= 0) {
            emitBatchProgressAndComplete(result.failedBatchId, result.batchIsComplete,
                                         /*includeFailed=*/true);
            if (result.batchIsComplete) {
                return;
            }
        }
    }

    emit queueChanged();
    if (result.shouldScheduleProcessNext) {
        scheduleProcessNext();
    }
}

void TransferQueue::onFtpDirectoryCreated(const QString &path)
{
    dirCreator_->onDirectoryCreated(path);
}

void TransferQueue::onDirectoryListed(const QString &path, const QList<FtpEntry> &entries)
{
    qDebug() << "TransferQueue: onDirectoryListed:" << path << "entries:" << entries.size();

    if (scanCoordinator_->handlesListing(path)) {
        scanCoordinator_->onDirectoryListed(path, entries);
        return;
    }

    qDebug() << "TransferQueue: Ignoring untracked listing for" << path;
}

void TransferQueue::onFileRemoved(const QString &path)
{
    qDebug() << "TransferQueue: onFileRemoved:" << path;

    if (state_.queueState == QueueState::Deleting &&
        state_.deletedCount < state_.deleteQueue.size()) {
        if (state_.deleteQueue[state_.deletedCount].path == path) {
            state_.deletedCount++;
            QString fileName = QFileInfo(path).fileName();
            emit deleteProgressUpdate(fileName, state_.deletedCount, state_.deleteQueue.size());
            emit queueChanged();
            processNextDelete();
            return;
        }
    }

    // Handle single delete in regular queue
    for (const auto &item : state_.items) {
        if (item.operationType == OperationType::Delete && item.remotePath == path &&
            item.status == TransferItem::Status::InProgress) {

            stopOperationTimeout();
            markCurrentComplete(TransferItem::Status::Completed);

            QString fileName = QFileInfo(path).fileName();
            emit operationCompleted(fileName);

            transitionTo(QueueState::Idle);
            emit queueChanged();
            scheduleProcessNext();
            return;
        }
    }
}

// ============================================================================
// Confirmation handling
// ============================================================================

void TransferQueue::respondToOverwrite(OverwriteResponse response)
{
    // Capture the affected batch ID before state_ update clears pendingConfirmation
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
        emit dataChanged(index(0), index(state_.items.size() - 1));
    }

    // Check batch completion for Skip case
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
// Batch management
// ============================================================================

int TransferQueue::createBatch(OperationType type, const QString &description,
                               const QString &folderName, const QString &sourcePath)
{
    // createBatch may purge completed batches — use reset model as a safe catch-all
    beginResetModel();
    auto result = transfer::createBatch(state_, type, description, folderName, sourcePath);
    state_ = result.newState;
    endResetModel();

    qDebug() << "TransferQueue: Created batch" << result.batchId << ":" << description;
    emit queueChanged();

    return result.batchId;
}

void TransferQueue::activateNextBatch()
{
    state_ = transfer::activateNextBatch(state_);

    if (state_.activeBatchIndex >= 0) {
        qDebug() << "TransferQueue: Activated batch"
                 << state_.batches[state_.activeBatchIndex].batchId;
        emit batchStarted(state_.batches[state_.activeBatchIndex].batchId);
    } else {
        qDebug() << "TransferQueue: No more batches to activate";
    }
}

void TransferQueue::completeBatch(int batchId)
{
    TransferBatch *batch = findBatch(batchId);
    if (!batch) {
        return;
    }

    qDebug() << "TransferQueue: Completing batch" << batchId
             << "completed:" << batch->completedCount << "failed:" << batch->failedCount
             << "total:" << batch->totalCount();

    state_.activeBatchIndex = -1;
    stopOperationTimeout();
    state_.currentIndex = -1;
    transitionTo(QueueState::Idle);

    emit batchCompleted(batchId);

    // Check if this is part of a folder operation
    if (state_.currentFolderOp.batchId == batchId) {
        folderCoordinator_->onFolderOperationComplete();
        return;
    }

    activateNextBatch();

    bool hasActiveBatches = false;
    for (const auto &b : state_.batches) {
        if (!b.isComplete()) {
            hasActiveBatches = true;
            break;
        }
    }

    if (!hasActiveBatches) {
        qDebug() << "TransferQueue: All batches complete";
        state_.overwriteAll = false;
        emit allOperationsCompleted();
    } else if (state_.activeBatchIndex >= 0) {
        scheduleProcessNext();
    }
}

void TransferQueue::purgeBatch(int batchId)
{
    for (int i = 0; i < state_.batches.size(); ++i) {
        if (state_.batches[i].batchId == batchId) {
            qDebug() << "TransferQueue: Purging batch" << batchId;

            for (int j = state_.items.size() - 1; j >= 0; --j) {
                if (state_.items[j].batchId == batchId) {
                    beginRemoveRows(QModelIndex(), j, j);
                    state_.items.removeAt(j);
                    endRemoveRows();

                    if (state_.currentIndex > j) {
                        state_.currentIndex--;
                    } else if (state_.currentIndex == j) {
                        state_.currentIndex = -1;
                    }
                }
            }

            if (state_.activeBatchIndex == i) {
                state_.activeBatchIndex = -1;
            } else if (state_.activeBatchIndex > i) {
                state_.activeBatchIndex--;
            }

            state_.batches.removeAt(i);
            emit queueChanged();
            return;
        }
    }
}

void TransferQueue::emitBatchProgressAndComplete(int batchId, bool batchIsComplete,
                                                 bool includeFailed)
{
    if (TransferBatch *batch = findBatch(batchId)) {
        int completed =
            includeFailed ? batch->completedCount + batch->failedCount : batch->completedCount;
        emit batchProgressUpdate(batchId, completed, batch->totalCount());
    }
    if (batchIsComplete) {
        completeBatch(batchId);
    }
}

void TransferQueue::markCurrentComplete(TransferItem::Status status)
{
    if (state_.currentIndex < 0 || state_.currentIndex >= state_.items.size()) {
        return;
    }

    int completedIndex = state_.currentIndex;
    auto result = transfer::markItemComplete(state_, completedIndex, status);
    state_ = result.newState;

    emit dataChanged(index(completedIndex), index(completedIndex));

    if (result.batchId >= 0) {
        emitBatchProgressAndComplete(result.batchId, result.batchIsComplete);
    }
}

TransferBatch *TransferQueue::findBatch(int batchId)
{
    for (auto &batch : state_.batches) {
        if (batch.batchId == batchId) {
            return &batch;
        }
    }
    return nullptr;
}

const TransferBatch *TransferQueue::findBatch(int batchId) const
{
    for (const auto &batch : state_.batches) {
        if (batch.batchId == batchId) {
            return &batch;
        }
    }
    return nullptr;
}

TransferBatch *TransferQueue::activeBatch()
{
    if (state_.activeBatchIndex >= 0 && state_.activeBatchIndex < state_.batches.size()) {
        return &state_.batches[state_.activeBatchIndex];
    }
    return nullptr;
}

int TransferQueue::findItemIndex(const QString &localPath, const QString &remotePath) const
{
    return transfer::findItemIndex(state_, localPath, remotePath);
}

// ============================================================================
// Queue operations
// ============================================================================

void TransferQueue::clear()
{
    beginResetModel();
    state_ = transfer::clearAll(state_);
    endResetModel();

    folderCoordinator_->stopDebounce();
    emit queueChanged();
}

void TransferQueue::removeCompleted()
{
    for (int i = state_.items.size() - 1; i >= 0; --i) {
        if (state_.items[i].status == TransferItem::Status::Completed ||
            state_.items[i].status == TransferItem::Status::Failed ||
            state_.items[i].status == TransferItem::Status::Skipped) {
            beginRemoveRows(QModelIndex(), i, i);
            state_.items.removeAt(i);
            endRemoveRows();

            if (state_.currentIndex > i) {
                state_.currentIndex--;
            } else if (state_.currentIndex == i) {
                state_.currentIndex = -1;
            }
        }
    }
    emit queueChanged();
}

void TransferQueue::cancelAll()
{
    if (ftpClient_ && (state_.queueState == QueueState::Transferring ||
                       state_.queueState == QueueState::Deleting)) {
        ftpClient_->abort();
    }

    state_ = transfer::cancelAllItems(state_);

    folderCoordinator_->stopDebounce();

    emit dataChanged(index(0), index(state_.items.size() - 1));
    emit queueChanged();
    emit operationsCancelled();
}

void TransferQueue::cancelBatch(int batchId)
{
    int batchIdx = transfer::findBatchIndex(state_, batchId);
    if (batchIdx < 0) {
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

    emit dataChanged(index(0), index(state_.items.size() - 1));
    emit queueChanged();

    if (result.wasActiveBatch) {
        scheduleProcessNext();
    }
}

// ============================================================================
// Query methods
// ============================================================================

int TransferQueue::pendingCount() const
{
    return transfer::pendingCount(state_);
}

int TransferQueue::activeCount() const
{
    return transfer::activeCount(state_);
}

int TransferQueue::activeAndPendingCount() const
{
    return transfer::activeAndPendingCount(state_);
}

bool TransferQueue::isScanningForDelete() const
{
    return state_.queueState == QueueState::Scanning && !state_.pendingDeleteScans.isEmpty();
}

bool TransferQueue::hasActiveBatch() const
{
    return transfer::hasActiveBatch(state_);
}

int TransferQueue::queuedBatchCount() const
{
    return transfer::queuedBatchCount(state_);
}

bool TransferQueue::isPathBeingTransferred(const QString &path, OperationType type) const
{
    return transfer::isPathBeingTransferred(state_, path, type);
}

// ============================================================================
// QAbstractListModel implementation
// ============================================================================

int TransferQueue::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return state_.items.size();
}

QVariant TransferQueue::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= state_.items.size()) {
        return {};
    }
    return transfer::itemData(state_.items[index.row()], role);
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
    return transfer::computeActiveBatchProgress(state_);
}

BatchProgress TransferQueue::batchProgress(int batchId) const
{
    return transfer::computeBatchProgress(state_, batchId);
}

QList<int> TransferQueue::allBatchIds() const
{
    return transfer::allBatchIds(state_);
}

// ============================================================================
// Timeout handling
// ============================================================================

void TransferQueue::startOperationTimeout()
{
    operationTimeoutTimer_->start(OperationTimeoutMs);
}

void TransferQueue::stopOperationTimeout()
{
    operationTimeoutTimer_->stop();
}

void TransferQueue::onOperationTimeout()
{
    qDebug() << "TransferQueue: Operation timeout!";

    if (ftpClient_) {
        ftpClient_->abort();
    }

    int originalIndex = state_.currentIndex;
    state_ = transfer::handleOperationTimeout(state_);

    if (originalIndex >= 0 && originalIndex < state_.items.size()) {
        QString errorMessage =
            tr("Operation timed out after %1 minutes").arg(OperationTimeoutMs / 60000);
        state_.items[originalIndex].errorMessage = errorMessage;

        QString fileName = QFileInfo(state_.items[originalIndex].localPath.isEmpty()
                                         ? state_.items[originalIndex].remotePath
                                         : state_.items[originalIndex].localPath)
                               .fileName();
        emit dataChanged(index(originalIndex), index(originalIndex));
        emit operationFailed(fileName, errorMessage);
    }

    scheduleProcessNext();
}
