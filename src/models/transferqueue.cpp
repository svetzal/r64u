#include "transferqueue.h"
#include "../services/iftpclient.h"

#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QDebug>
#include <algorithm>
#include <memory>

TransferQueue::TransferQueue(QObject *parent)
    : QAbstractListModel(parent)
    , operationTimeoutTimer_(new QTimer(this))
{
    operationTimeoutTimer_->setSingleShot(true);
    connect(operationTimeoutTimer_, &QTimer::timeout,
            this, &TransferQueue::onOperationTimeout);
}

TransferQueue::~TransferQueue()
{
    // Disconnect from FTP client BEFORE this object is destroyed to prevent
    // signals from being delivered to slots that access invalid memory.
    // Qt's automatic disconnection happens in QObject::~QObject() which
    // runs AFTER this destructor body, by which time our member variables
    // (like items_) may already be destroyed.
    if (ftpClient_) {
        disconnect(ftpClient_, nullptr, this, nullptr);
    }
}

void TransferQueue::scheduleProcessNext()
{
    // Queue the processNext() call for deferred execution.
    // This prevents re-entrancy issues where signal handlers
    // calling processNext() could cause nested state changes.
    eventQueue_.enqueue([this]() { processNext(); });

    // Schedule event processing if not already scheduled
    if (!eventProcessingScheduled_) {
        eventProcessingScheduled_ = true;
        QTimer::singleShot(0, this, &TransferQueue::processEventQueue);
    }
}

void TransferQueue::processEventQueue()
{
    eventProcessingScheduled_ = false;

    // Re-entrancy guard: if we're already processing, let the outer call finish
    if (processingEvents_) {
        // Reschedule for later if there are still events
        if (!eventQueue_.isEmpty() && !eventProcessingScheduled_) {
            eventProcessingScheduled_ = true;
            QTimer::singleShot(0, this, &TransferQueue::processEventQueue);
        }
        return;
    }

    processingEvents_ = true;

    // Process all queued events
    while (!eventQueue_.isEmpty()) {
        auto event = eventQueue_.dequeue();
        event();
    }

    processingEvents_ = false;
}

void TransferQueue::flushEventQueue()
{
    // Re-entrancy guard: don't flush if we're already processing
    if (processingEvents_) {
        return;
    }

    // Cancel any pending timer and process immediately
    eventProcessingScheduled_ = false;
    processingEvents_ = true;

    // Process all events synchronously
    while (!eventQueue_.isEmpty()) {
        auto event = eventQueue_.dequeue();
        event();
    }

    processingEvents_ = false;
}

void TransferQueue::transitionTo(QueueState newState)
{
    if (state_ == newState) {
        return;  // No change
    }

    qDebug() << "TransferQueue: State transition"
             << queueStateToString(state_) << "->" << queueStateToString(newState);

    state_ = newState;

    // Validate that the new state is consistent with boolean flags
    validateStateConsistency();
}

void TransferQueue::validateStateConsistency() const
{
    // This method validates that the QueueState enum matches the boolean flags.
    // During the transition period (Phase 2), we just log mismatches as warnings.
    // In Phase 3, once flags are replaced, this can use assertions.

#ifdef QT_DEBUG
    auto warn = [this](const char* msg) {
        qWarning() << "TransferQueue: State consistency warning -" << msg
                   << "| state:" << queueStateToString(state_)
                   << "| processing:" << processing_
                   << "| scanning:" << scanningDirectories_
                   << "| creating:" << creatingDirectory_
                   << "| waitOverwrite:" << waitingForOverwriteResponse_
                   << "| waitFolder:" << waitingForFolderExistsResponse_
                   << "| checkUpload:" << checkingUploadFileExists_
                   << "| checkFolder:" << checkingFolderExists_
                   << "| deleting:" << processingDelete_;
    };

    switch (state_) {
    case QueueState::Idle:
        // Idle means nothing is happening
        if (processing_ && !processingDelete_) {
            warn("Idle state but processing_ is true");
        }
        if (scanningDirectories_) {
            warn("Idle state but scanningDirectories_ is true");
        }
        if (creatingDirectory_) {
            warn("Idle state but creatingDirectory_ is true");
        }
        if (waitingForOverwriteResponse_) {
            warn("Idle state but waitingForOverwriteResponse_ is true");
        }
        if (waitingForFolderExistsResponse_) {
            warn("Idle state but waitingForFolderExistsResponse_ is true");
        }
        if (checkingUploadFileExists_) {
            warn("Idle state but checkingUploadFileExists_ is true");
        }
        if (checkingFolderExists_) {
            warn("Idle state but checkingFolderExists_ is true");
        }
        break;

    case QueueState::Scanning:
        if (!scanningDirectories_) {
            warn("Scanning state but scanningDirectories_ is false");
        }
        break;

    case QueueState::CreatingDirectories:
        if (!creatingDirectory_ && pendingMkdirs_.isEmpty()) {
            warn("CreatingDirectories state but no directory creation in progress");
        }
        break;

    case QueueState::CheckingExists:
        if (!checkingUploadFileExists_ && !checkingFolderExists_) {
            warn("CheckingExists state but no existence check in progress");
        }
        break;

    case QueueState::AwaitingConfirmation:
        if (!waitingForOverwriteResponse_ && !waitingForFolderExistsResponse_) {
            warn("AwaitingConfirmation state but no confirmation pending");
        }
        break;

    case QueueState::Transferring:
        if (!processing_) {
            warn("Transferring state but processing_ is false");
        }
        break;

    case QueueState::Deleting:
        if (!processingDelete_) {
            warn("Deleting state but processingDelete_ is false");
        }
        break;
    }
#else
    Q_UNUSED(this)
#endif
}

void TransferQueue::setFtpClient(IFtpClient *client)
{
    if (ftpClient_) {
        disconnect(ftpClient_, nullptr, this, nullptr);
    }

    ftpClient_ = client;

    if (ftpClient_) {
        connect(ftpClient_, &IFtpClient::uploadProgress,
                this, &TransferQueue::onUploadProgress);
        connect(ftpClient_, &IFtpClient::uploadFinished,
                this, &TransferQueue::onUploadFinished);
        connect(ftpClient_, &IFtpClient::downloadProgress,
                this, &TransferQueue::onDownloadProgress);
        connect(ftpClient_, &IFtpClient::downloadFinished,
                this, &TransferQueue::onDownloadFinished);
        connect(ftpClient_, &IFtpClient::error,
                this, &TransferQueue::onFtpError);
        connect(ftpClient_, &IFtpClient::directoryCreated,
                this, &TransferQueue::onDirectoryCreated);
        connect(ftpClient_, &IFtpClient::directoryListed,
                this, &TransferQueue::onDirectoryListed);
        connect(ftpClient_, &IFtpClient::fileRemoved,
                this, &TransferQueue::onFileRemoved);
    }
}

void TransferQueue::enqueueUpload(const QString &localPath, const QString &remotePath, int targetBatchId)
{
    // Use the target batch if specified, otherwise find/create one
    int batchIdx = -1;
    if (targetBatchId >= 0) {
        for (int i = 0; i < batches_.size(); ++i) {
            if (batches_[i].batchId == targetBatchId) {
                batchIdx = i;
                break;
            }
        }
    }

    // Fall back to finding/creating a batch if target not found
    if (batchIdx < 0) {
        batchIdx = activeBatchIndex_;
        if (batchIdx < 0 || batches_[batchIdx].operationType != OperationType::Upload) {
            QString fileName = QFileInfo(localPath).fileName();
            // For recursive uploads, use the base path as source for duplicate detection
            QString sourcePath = !currentFolderUpload_.localDir.isEmpty() ? currentFolderUpload_.localDir : QString();
            int batchId = createBatch(OperationType::Upload, tr("Uploading %1").arg(fileName), sourcePath);
            // Find the batch we just created
            for (int i = 0; i < batches_.size(); ++i) {
                if (batches_[i].batchId == batchId) {
                    batchIdx = i;
                    break;
                }
            }
        }
    }

    if (batchIdx < 0 || batchIdx >= batches_.size()) {
        qWarning() << "TransferQueue::enqueueUpload - no valid batch";
        return;
    }

    TransferItem item;
    item.localPath = localPath;
    item.remotePath = remotePath;
    item.operationType = OperationType::Upload;
    item.status = TransferItem::Status::Pending;
    item.totalBytes = QFileInfo(localPath).size();
    item.batchId = batches_[batchIdx].batchId;

    beginInsertRows(QModelIndex(), items_.size(), items_.size());
    items_.append(item);
    endInsertRows();

    // Add to batch
    batches_[batchIdx].items.append(item);

    // Activate the batch if not already active
    if (activeBatchIndex_ < 0) {
        activeBatchIndex_ = batchIdx;
        batches_[batchIdx].isActive = true;
        batches_[batchIdx].hasBeenProcessed = true;
        emit batchStarted(batches_[batchIdx].batchId);
    }

    emit queueChanged();

    if (!processing_) {
        scheduleProcessNext();
    }
}

void TransferQueue::enqueueDownload(const QString &remotePath, const QString &localPath, int targetBatchId)
{
    // Use the target batch if specified, otherwise find/create one
    int batchIdx = -1;

    if (targetBatchId >= 0) {
        // Use the specified batch
        for (int i = 0; i < batches_.size(); ++i) {
            if (batches_[i].batchId == targetBatchId) {
                batchIdx = i;
                break;
            }
        }
    }

    // If no target batch specified or not found, use active batch or create new
    if (batchIdx < 0) {
        batchIdx = activeBatchIndex_;
        if (batchIdx < 0 || batches_[batchIdx].operationType != OperationType::Download) {
            QString fileName = QFileInfo(remotePath).fileName();
            // For recursive downloads, use the base path as source for duplicate detection
            QString sourcePath = (state_ == QueueState::Scanning) ? recursiveRemoteBase_ : QString();
            int batchId = createBatch(OperationType::Download, tr("Downloading %1").arg(fileName), sourcePath);
            // Find the batch we just created
            for (int i = 0; i < batches_.size(); ++i) {
                if (batches_[i].batchId == batchId) {
                    batchIdx = i;
                    break;
                }
            }
        }
    }

    if (batchIdx < 0 || batchIdx >= batches_.size()) {
        qWarning() << "TransferQueue::enqueueDownload - no valid batch";
        return;
    }

    TransferItem item;
    item.localPath = localPath;
    item.remotePath = remotePath;
    item.operationType = OperationType::Download;
    item.status = TransferItem::Status::Pending;
    item.batchId = batches_[batchIdx].batchId;

    beginInsertRows(QModelIndex(), items_.size(), items_.size());
    items_.append(item);
    endInsertRows();

    // Add to batch
    batches_[batchIdx].items.append(item);

    // Activate the batch if not already active
    if (activeBatchIndex_ < 0) {
        activeBatchIndex_ = batchIdx;
        batches_[batchIdx].isActive = true;
        batches_[batchIdx].hasBeenProcessed = true;
        emit batchStarted(batches_[batchIdx].batchId);
    }

    emit queueChanged();

    if (!processing_) {
        scheduleProcessNext();
    }
}

void TransferQueue::enqueueRecursiveUpload(const QString &localDir, const QString &remoteDir)
{
    if (!ftpClient_ || !ftpClient_->isConnected()) return;

    QDir baseDir(localDir);
    if (!baseDir.exists()) return;

    // Check if this path is already being uploaded
    if (isPathBeingTransferred(localDir, OperationType::Upload)) {
        qDebug() << "TransferQueue: Ignoring duplicate upload request for" << localDir;
        emit statusMessage(tr("'%1' is already being uploaded").arg(QFileInfo(localDir).fileName()), 3000);
        return;
    }

    QString baseName = QFileInfo(localDir).fileName();
    QString targetDir = remoteDir;
    if (!targetDir.endsWith('/')) targetDir += '/';
    targetDir += baseName;

    // Create pending upload entry
    PendingFolderUpload pending;
    pending.localDir = localDir;
    pending.remoteDir = remoteDir;
    pending.targetDir = targetDir;

    // If auto-merge is enabled, skip the existence check
    if (autoMerge_) {
        currentFolderUpload_ = pending;
        startRecursiveUpload();
        return;
    }

    // Add to queue of folders waiting to be checked
    pendingFolderUploads_.enqueue(pending);

    // Check if we're already listing this parent directory
    // If so, the pending folder will be processed when the listing arrives
    if (requestedFolderCheckListings_.contains(remoteDir)) {
        qDebug() << "TransferQueue: Folder" << targetDir << "queued, waiting for existing LIST of" << remoteDir;
        return;
    }

    // Check if the target folder already exists by listing the parent directory
    checkingFolderExists_ = true;
    transitionTo(QueueState::CheckingExists);
    requestedFolderCheckListings_.insert(remoteDir);

    qDebug() << "TransferQueue: Checking if folder exists:" << targetDir << "by listing" << remoteDir;
    ftpClient_->list(remoteDir);
}

void TransferQueue::startRecursiveUpload()
{
    QString localDir = currentFolderUpload_.localDir;
    QString targetDir = currentFolderUpload_.targetDir;

    qDebug() << "TransferQueue: Starting recursive upload from" << localDir << "to" << targetDir;

    // Create the batch immediately so the progress widget appears during directory creation
    QString folderName = QFileInfo(localDir).fileName();
    int batchId = createBatch(OperationType::Upload, tr("Uploading %1").arg(folderName), localDir);
    currentFolderUpload_.batchId = batchId;
    qDebug() << "TransferQueue: Created batch" << batchId << "for recursive upload of" << folderName;

    // Emit operationStarted to keep refresh suppressed during directory creation and file uploads
    emit operationStarted(folderName, OperationType::Upload);

    // Collect all directories that need to be created first
    QDir baseDir(localDir);

    // Queue the root directory creation
    PendingMkdir rootMkdir;
    rootMkdir.remotePath = targetDir;
    rootMkdir.localDir = localDir;
    rootMkdir.remoteBase = targetDir;
    pendingMkdirs_.enqueue(rootMkdir);

    // Recursively find all subdirectories and queue them
    QDirIterator it(localDir, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString subDir = it.next();
        QString relativePath = baseDir.relativeFilePath(subDir);
        QString remotePath = targetDir + '/' + relativePath;

        PendingMkdir mkdir;
        mkdir.remotePath = remotePath;
        mkdir.localDir = subDir;
        mkdir.remoteBase = targetDir;
        pendingMkdirs_.enqueue(mkdir);
    }

    // Initialize directory creation progress tracking
    directoriesCreated_ = 0;
    totalDirectoriesToCreate_ = pendingMkdirs_.size();

    // Emit initial progress - the batch was created above so the widget should show this
    emit directoryCreationProgress(0, totalDirectoriesToCreate_);

    // Start creating directories
    processPendingDirectoryCreation();
}

void TransferQueue::startNextFolderUpload()
{
    // Start uploading the next folder from foldersToUpload_
    // This is called after confirmation (or when no confirmation needed)

    if (foldersToUpload_.isEmpty()) {
        qDebug() << "TransferQueue: No more folders to upload";
        // Check if there are more pending folders that need a different listing
        if (!pendingFolderUploads_.isEmpty()) {
            const PendingFolderUpload &next = pendingFolderUploads_.head();
            checkingFolderExists_ = true;
            transitionTo(QueueState::CheckingExists);
            requestedFolderCheckListings_.insert(next.remoteDir);
            qDebug() << "TransferQueue: Checking next folder:" << next.targetDir << "by listing" << next.remoteDir;
            ftpClient_->list(next.remoteDir);
        }
        return;
    }

    // Don't start a new upload if one is already in progress
    if (folderUploadInProgress_) {
        qDebug() << "TransferQueue: Folder upload already in progress, waiting...";
        return;
    }

    // Take the first folder
    currentFolderUpload_ = foldersToUpload_.takeFirst();

    // Check if this folder needs to be deleted first (Replace operation)
    // This happens when user chose Replace and the folder exists
    if (currentFolderUpload_.exists && replaceExistingFolders_) {
        qDebug() << "TransferQueue: Folder" << currentFolderUpload_.targetDir
                 << "needs deletion before upload (Replace)";
        pendingUploadAfterDelete_ = true;
        compoundOp_.phase = CompoundOperation::Deleting;
        enqueueRecursiveDelete(currentFolderUpload_.targetDir);
        return;
    }

    folderUploadInProgress_ = true;

    qDebug() << "TransferQueue: Starting folder upload:" << currentFolderUpload_.targetDir
             << "(" << foldersToUpload_.size() << "more waiting)";

    startRecursiveUpload();
}

void TransferQueue::onFolderUploadComplete()
{
    // Called when a folder upload finishes (batch complete)
    qDebug() << "TransferQueue: Folder upload complete:" << currentFolderUpload_.targetDir;

    folderUploadInProgress_ = false;
    currentFolderUpload_ = {};
    compoundOp_.clear();  // Compound operation complete (if any)

    // Start the next folder if any
    if (!foldersToUpload_.isEmpty()) {
        startNextFolderUpload();
    } else {
        // All folders done - reset flags and emit completion
        qDebug() << "TransferQueue: All folder uploads complete";
        overwriteAll_ = false;
        replaceExistingFolders_ = false;
        emit allOperationsCompleted();
    }
}

void TransferQueue::processRecursiveUpload(const QString &localDir, const QString &remoteDir)
{
    // Queue all files for upload to the batch created in startRecursiveUpload
    int batchId = currentFolderUpload_.batchId;
    QDir dir(localDir);
    QDirIterator it(localDir, QDir::Files, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        QString filePath = it.next();
        QString relativePath = dir.relativeFilePath(filePath);
        QString remotePath = remoteDir + '/' + relativePath;

        enqueueUpload(filePath, remotePath, batchId);
    }
}

void TransferQueue::processPendingDirectoryCreation()
{
    if (state_ == QueueState::CreatingDirectories || pendingMkdirs_.isEmpty()) {
        return;
    }

    creatingDirectory_ = true;
    transitionTo(QueueState::CreatingDirectories);
    PendingMkdir mkdir = pendingMkdirs_.head();
    ftpClient_->makeDirectory(mkdir.remotePath);
}

void TransferQueue::enqueueRecursiveDownload(const QString &remoteDir, const QString &localDir)
{
    if (!ftpClient_ || !ftpClient_->isConnected()) return;

    qDebug() << "TransferQueue: enqueueRecursiveDownload" << remoteDir << "->" << localDir
             << "current state:" << queueStateToString(state_)
             << "scanning:" << scanningDirectories_
             << "batches:" << batches_.size();

    // Normalize remote path - remove trailing slashes
    QString normalizedRemote = remoteDir;
    while (normalizedRemote.endsWith('/') && normalizedRemote.length() > 1) {
        normalizedRemote.chop(1);
    }

    // Check if this path is already being downloaded
    if (isPathBeingTransferred(normalizedRemote, OperationType::Download)) {
        qDebug() << "TransferQueue: Ignoring duplicate download request for" << normalizedRemote;
        emit statusMessage(tr("'%1' is already being downloaded").arg(QFileInfo(normalizedRemote).fileName()), 3000);
        return;
    }

    qDebug() << "TransferQueue: Passed duplicate check for" << normalizedRemote;

    // Set scanning mode - this prevents processNext() from starting downloads
    // until all directories have been scanned
    scanningDirectories_ = true;
    transitionTo(QueueState::Scanning);

    // Store the base paths for path calculation
    recursiveRemoteBase_ = normalizedRemote;
    recursiveLocalBase_ = localDir;

    // Create local base directory with the remote folder's name
    QString folderName = QFileInfo(normalizedRemote).fileName();
    QString targetDir = localDir;
    if (!targetDir.endsWith('/')) targetDir += '/';
    targetDir += folderName;

    qDebug() << "TransferQueue: Creating local dir:" << targetDir;
    QDir().mkpath(targetDir);
    recursiveLocalBase_ = targetDir;

    // Create the batch immediately for this recursive download operation
    // This ensures each recursive operation gets its own batch
    int batchId = createBatch(OperationType::Download, tr("Downloading %1").arg(folderName), normalizedRemote);
    qDebug() << "TransferQueue: Created batch" << batchId << "for recursive download of" << folderName;

    // Initialize scanning progress tracking
    scanningFolderName_ = folderName;
    directoriesScanned_ = 0;
    filesDiscovered_ = 0;

    // Queue the initial directory scan with the batch ID
    PendingScan scan;
    scan.remotePath = normalizedRemote;
    scan.localBasePath = targetDir;
    scan.batchId = batchId;
    pendingScans_.enqueue(scan);

    // Track that we're requesting this listing (to avoid conflict with RemoteFileModel)
    requestedListings_.insert(normalizedRemote);
    qDebug() << "TransferQueue: Requesting listing for:" << normalizedRemote;

    // Emit scanning started signal - this will show the progress widget immediately
    emit scanningStarted(folderName, OperationType::Download);
    emit scanningProgress(0, 1, 0);  // Initial: 0 scanned, 1 remaining, 0 files found

    // Start scanning
    ftpClient_->list(normalizedRemote);
}

void TransferQueue::clear()
{
    beginResetModel();
    items_.clear();
    endResetModel();

    processing_ = false;
    currentIndex_ = -1;

    // Clear batch state
    batches_.clear();
    activeBatchIndex_ = -1;

    // Clear recursive operation state
    pendingScans_.clear();
    requestedListings_.clear();
    scanningDirectories_ = false;
    scanningFolderName_.clear();
    directoriesScanned_ = 0;
    filesDiscovered_ = 0;
    pendingMkdirs_.clear();
    creatingDirectory_ = false;
    directoriesCreated_ = 0;
    totalDirectoriesToCreate_ = 0;

    // Clear recursive delete state
    pendingDeleteScans_.clear();
    requestedDeleteListings_.clear();
    recursiveDeleteBase_.clear();
    deleteQueue_.clear();
    currentDeleteIndex_ = 0;
    totalDeleteItems_ = 0;
    deletedCount_ = 0;
    processingDelete_ = false;

    // Clear upload file check state
    checkingUploadFileExists_ = false;
    requestedUploadFileCheckListings_.clear();

    // Clear confirmation context
    pendingConfirmation_.clear();
    waitingForOverwriteResponse_ = false;
    waitingForFolderExistsResponse_ = false;
    overwriteAll_ = false;

    // Clear compound operation
    compoundOp_.clear();
    pendingUploadAfterDelete_ = false;

    // Reset state machine to idle
    transitionTo(QueueState::Idle);

    emit queueChanged();
}

void TransferQueue::removeCompleted()
{
    for (int i = items_.size() - 1; i >= 0; --i) {
        if (items_[i].status == TransferItem::Status::Completed ||
            items_[i].status == TransferItem::Status::Failed) {
            beginRemoveRows(QModelIndex(), i, i);
            items_.removeAt(i);
            endRemoveRows();

            if (currentIndex_ > i) {
                currentIndex_--;
            } else if (currentIndex_ == i) {
                currentIndex_ = -1;
            }
        }
    }
    emit queueChanged();
}

void TransferQueue::cancelAll()
{
    if (ftpClient_ && (state_ == QueueState::Transferring || state_ == QueueState::Deleting)) {
        ftpClient_->abort();
    }

    for (int i = 0; i < items_.size(); ++i) {
        if (items_[i].status == TransferItem::Status::Pending ||
            items_[i].status == TransferItem::Status::InProgress) {
            items_[i].status = TransferItem::Status::Failed;
            items_[i].errorMessage = tr("Cancelled");
        }
    }

    processing_ = false;
    currentIndex_ = -1;

    // Clear batch state
    batches_.clear();
    activeBatchIndex_ = -1;

    // Clear recursive operation state
    pendingScans_.clear();
    requestedListings_.clear();
    scanningDirectories_ = false;
    scanningFolderName_.clear();
    directoriesScanned_ = 0;
    filesDiscovered_ = 0;
    pendingMkdirs_.clear();
    creatingDirectory_ = false;
    directoriesCreated_ = 0;
    totalDirectoriesToCreate_ = 0;

    // Clear recursive delete state
    pendingDeleteScans_.clear();
    requestedDeleteListings_.clear();
    recursiveDeleteBase_.clear();
    deleteQueue_.clear();
    currentDeleteIndex_ = 0;
    totalDeleteItems_ = 0;
    deletedCount_ = 0;
    processingDelete_ = false;

    // Clear upload file check state
    checkingUploadFileExists_ = false;
    requestedUploadFileCheckListings_.clear();

    // Clear confirmation context
    pendingConfirmation_.clear();
    waitingForOverwriteResponse_ = false;
    waitingForFolderExistsResponse_ = false;
    overwriteAll_ = false;

    // Clear compound operation
    compoundOp_.clear();
    pendingUploadAfterDelete_ = false;

    // Reset state machine to idle
    transitionTo(QueueState::Idle);

    emit dataChanged(index(0), index(items_.size() - 1));
    emit queueChanged();
    emit operationsCancelled();
}

void TransferQueue::cancelBatch(int batchId)
{
    // Find the batch
    int batchIdx = -1;
    for (int i = 0; i < batches_.size(); ++i) {
        if (batches_[i].batchId == batchId) {
            batchIdx = i;
            break;
        }
    }

    if (batchIdx < 0) {
        return;  // Batch not found
    }

    const TransferBatch &batch = batches_[batchIdx];

    // If this is the active batch, cancel current operation
    if (batchIdx == activeBatchIndex_) {
        if (ftpClient_ && (state_ == QueueState::Transferring || state_ == QueueState::Deleting)) {
            ftpClient_->abort();
        }

        // Reset state for active batch
        processing_ = false;
        currentIndex_ = -1;

        // Clear scanning state if we were scanning for this batch
        if (state_ == QueueState::Scanning) {
            pendingScans_.clear();
            pendingDeleteScans_.clear();
            requestedListings_.clear();
            requestedDeleteListings_.clear();
            scanningDirectories_ = false;
        }

        // Clear directory creation state if applicable
        if (state_ == QueueState::CreatingDirectories) {
            pendingMkdirs_.clear();
            creatingDirectory_ = false;
        }

        // Clear delete state if applicable
        if (state_ == QueueState::Deleting) {
            deleteQueue_.clear();
            currentDeleteIndex_ = 0;
            totalDeleteItems_ = 0;
            deletedCount_ = 0;
            processingDelete_ = false;
        }

        transitionTo(QueueState::Idle);
    }

    // Mark all items in the batch as failed
    for (int i = 0; i < items_.size(); ++i) {
        if (items_[i].batchId == batchId &&
            (items_[i].status == TransferItem::Status::Pending ||
             items_[i].status == TransferItem::Status::InProgress)) {
            items_[i].status = TransferItem::Status::Failed;
            items_[i].errorMessage = tr("Cancelled");
        }
    }

    // Remove the batch
    bool wasActiveBatch = (batchIdx == activeBatchIndex_);
    purgeBatch(batchId);

    emit dataChanged(index(0), index(items_.size() - 1));
    emit queueChanged();

    // If we cancelled the active batch, activate the next one
    if (wasActiveBatch) {
        activateNextBatch();
        scheduleProcessNext();
    }
}

int TransferQueue::pendingCount() const
{
    int count = 0;
    for (const auto &item : items_) {
        if (item.status == TransferItem::Status::Pending) {
            count++;
        }
    }
    return count;
}

int TransferQueue::activeCount() const
{
    int count = 0;
    for (const auto &item : items_) {
        if (item.status == TransferItem::Status::InProgress) {
            count++;
        }
    }
    return count;
}

int TransferQueue::activeAndPendingCount() const
{
    int count = 0;
    for (const auto &item : items_) {
        if (item.status == TransferItem::Status::Pending ||
            item.status == TransferItem::Status::InProgress) {
            count++;
        }
    }
    return count;
}

int TransferQueue::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return items_.size();
}

QVariant TransferQueue::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= items_.size()) {
        return QVariant();
    }

    const TransferItem &item = items_[index.row()];

    switch (role) {
    case Qt::DisplayRole:
    case FileNameRole: {
        QString path = (item.operationType == OperationType::Upload)
            ? item.localPath : item.remotePath;
        return QFileInfo(path).fileName();
    }
    case LocalPathRole:
        return item.localPath;
    case RemotePathRole:
        return item.remotePath;
    case OperationTypeRole:
        return static_cast<int>(item.operationType);
    case StatusRole:
        return static_cast<int>(item.status);
    case ProgressRole:
        if (item.totalBytes > 0) {
            return static_cast<int>((item.bytesTransferred * 100) / item.totalBytes);
        }
        return 0;
    case BytesTransferredRole:
        return item.bytesTransferred;
    case TotalBytesRole:
        return item.totalBytes;
    case ErrorMessageRole:
        return item.errorMessage;
    }

    return QVariant();
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

void TransferQueue::processNext()
{
    qDebug() << "TransferQueue: processNext called, ftpClient_:" << (ftpClient_ != nullptr)
             << "isConnected:" << (ftpClient_ ? ftpClient_->isConnected() : false)
             << "state:" << queueStateToString(state_)
             << "processing_:" << processing_;

    // Use state machine for most guards - processing_ is checked separately
    // because it can be true during Transferring state
    if (processing_) {
        qDebug() << "TransferQueue: processNext - already processing, skipping";
        return;
    }

    if (!ftpClient_ || !ftpClient_->isConnected()) {
        qDebug() << "TransferQueue: processNext - FTP client not ready, stopping";
        processing_ = false;
        return;
    }

    // Don't start transfers while in non-idle states that block transfers
    switch (state_) {
    case QueueState::Scanning:
        qDebug() << "TransferQueue: processNext - waiting for directory scanning to complete";
        return;

    case QueueState::CreatingDirectories:
        qDebug() << "TransferQueue: processNext - waiting for directory creation to complete";
        return;

    case QueueState::AwaitingConfirmation:
        qDebug() << "TransferQueue: processNext - waiting for user confirmation";
        return;

    case QueueState::CheckingExists:
        qDebug() << "TransferQueue: processNext - waiting for existence check";
        return;

    case QueueState::Deleting:
        qDebug() << "TransferQueue: processNext - delete operation in progress";
        return;

    case QueueState::Idle:
    case QueueState::Transferring:
        // These states allow processing to continue
        break;
    }

    // Find next pending item
    for (int i = 0; i < items_.size(); ++i) {
        if (items_[i].status == TransferItem::Status::Pending) {
            currentIndex_ = i;

            QString fileName = QFileInfo(
                items_[i].operationType == OperationType::Upload
                    ? items_[i].localPath : items_[i].remotePath
            ).fileName();

            // Check for file existence and ask for overwrite confirmation
            // Skip if overwriteAll_ is set, or if this specific file was already confirmed
            if (items_[i].operationType == OperationType::Download &&
                !overwriteAll_ && !items_[i].overwriteConfirmed) {
                QFileInfo localFile(items_[i].localPath);
                if (localFile.exists()) {
                    qDebug() << "TransferQueue: File exists, asking for confirmation:" << items_[i].localPath;
                    waitingForOverwriteResponse_ = true;
                    pendingConfirmation_.type = ConfirmationContext::FileOverwrite;
                    pendingConfirmation_.operationType = OperationType::Download;
                    pendingConfirmation_.itemIndex = i;
                    transitionTo(QueueState::AwaitingConfirmation);
                    emit overwriteConfirmationNeeded(fileName, OperationType::Download);
                    return;
                }
            }

            // Check for remote file existence before uploading
            // This prevents accidental overwriting of remote files
            if (items_[i].operationType == OperationType::Upload &&
                !overwriteAll_ && !items_[i].overwriteConfirmed) {
                // Get the parent directory of the remote path
                QString parentDir = QFileInfo(items_[i].remotePath).path();
                if (parentDir.isEmpty()) {
                    parentDir = "/";
                }

                qDebug() << "TransferQueue: Checking if remote file exists:" << items_[i].remotePath
                         << "by listing" << parentDir;

                checkingUploadFileExists_ = true;
                transitionTo(QueueState::CheckingExists);
                requestedUploadFileCheckListings_.insert(parentDir);
                ftpClient_->list(parentDir);
                return;
            }

            items_[i].status = TransferItem::Status::InProgress;
            processing_ = true;

            // Use appropriate state based on operation type
            if (items_[i].operationType == OperationType::Delete) {
                transitionTo(QueueState::Deleting);
            } else {
                transitionTo(QueueState::Transferring);
            }

            emit dataChanged(index(i), index(i));
            emit operationStarted(fileName, items_[i].operationType);

            // Start operation timeout timer
            startOperationTimeout();

            qDebug() << "[QUEUE] Starting transfer for item" << i
                     << "batchId:" << items_[i].batchId
                     << "remote:" << items_[i].remotePath
                     << "local:" << items_[i].localPath
                     << "operationType:" << static_cast<int>(items_[i].operationType);

            if (items_[i].operationType == OperationType::Upload) {
                ftpClient_->upload(items_[i].localPath, items_[i].remotePath);
            } else if (items_[i].operationType == OperationType::Download) {
                ftpClient_->download(items_[i].remotePath, items_[i].localPath);
            } else if (items_[i].operationType == OperationType::Delete) {
                if (items_[i].isDirectory) {
                    ftpClient_->removeDirectory(items_[i].remotePath);
                } else {
                    ftpClient_->remove(items_[i].remotePath);
                }
            }
            return;
        }
    }

    // No more pending items in current batch
    qDebug() << "TransferQueue: processNext - no more pending items";
    stopOperationTimeout();
    processing_ = false;
    currentIndex_ = -1;
    // Note: overwriteAll_ is preserved across batches and only reset when all operations complete

    // Note: allOperationsCompleted is now emitted by completeBatch()
    // when all batches are done. If we get here without any batches,
    // it means we have legacy items without batch association.
    if (batches_.isEmpty()) {
        emit allOperationsCompleted();
    }
}

void TransferQueue::respondToOverwrite(OverwriteResponse response)
{
    if (state_ != QueueState::AwaitingConfirmation ||
        pendingConfirmation_.type != ConfirmationContext::FileOverwrite) {
        return;
    }

    // Get the item index from confirmation context
    int itemIdx = pendingConfirmation_.itemIndex;

    // Clear confirmation state
    waitingForOverwriteResponse_ = false;
    pendingConfirmation_.clear();
    transitionTo(QueueState::Idle);

    switch (response) {
    case OverwriteResponse::Overwrite:
        // Proceed with just this file - mark it so we don't ask again
        qDebug() << "TransferQueue: User chose to overwrite this file";
        if (itemIdx >= 0 && itemIdx < items_.size()) {
            items_[itemIdx].overwriteConfirmed = true;
        }
        scheduleProcessNext();
        break;

    case OverwriteResponse::OverwriteAll:
        // Set flag and proceed
        qDebug() << "TransferQueue: User chose to overwrite all files";
        overwriteAll_ = true;
        scheduleProcessNext();
        break;

    case OverwriteResponse::Skip:
        // Skip this file and continue
        qDebug() << "TransferQueue: User chose to skip this file";
        if (itemIdx >= 0 && itemIdx < items_.size()) {
            items_[itemIdx].status = TransferItem::Status::Completed;
            items_[itemIdx].errorMessage = tr("Skipped");
            emit dataChanged(index(itemIdx), index(itemIdx));

            // Update batch progress for skipped file
            int batchId = items_[itemIdx].batchId;
            if (TransferBatch *batch = findBatch(batchId)) {
                batch->completedCount++;
                emit batchProgressUpdate(batchId, batch->completedCount, batch->totalCount());

                // Check if batch is complete
                if (batch->isComplete()) {
                    currentIndex_ = -1;
                    completeBatch(batchId);
                    return;
                }
            }
        }
        currentIndex_ = -1;
        scheduleProcessNext();
        break;

    case OverwriteResponse::Cancel:
        // Cancel all remaining operations
        qDebug() << "TransferQueue: User cancelled operations";
        cancelAll();
        break;
    }
}

int TransferQueue::findItemIndex(const QString &localPath, const QString &remotePath) const
{
    qDebug() << "[FIND] Looking for local:" << localPath << "remote:" << remotePath;
    for (int i = 0; i < items_.size(); ++i) {
        if (items_[i].localPath == localPath && items_[i].remotePath == remotePath) {
            qDebug() << "[FIND] Found match at index" << i;
            return i;
        }
    }
    qDebug() << "[FIND] NO MATCH FOUND - queue contents:";
    for (int i = 0; i < items_.size(); ++i) {
        qDebug() << "  [FIND] Item" << i
                 << "local:" << items_[i].localPath
                 << "remote:" << items_[i].remotePath
                 << "status:" << static_cast<int>(items_[i].status);
    }
    return -1;
}

void TransferQueue::onUploadProgress(const QString &file, qint64 sent, qint64 total)
{
    Q_UNUSED(file)
    // Reset timeout on activity
    startOperationTimeout();

    if (currentIndex_ >= 0 && currentIndex_ < items_.size()) {
        items_[currentIndex_].bytesTransferred = sent;
        items_[currentIndex_].totalBytes = total;
        emit dataChanged(index(currentIndex_), index(currentIndex_));
    }
}

void TransferQueue::onUploadFinished(const QString &localPath, const QString &remotePath)
{
    stopOperationTimeout();

    int idx = findItemIndex(localPath, remotePath);
    if (idx >= 0) {
        items_[idx].status = TransferItem::Status::Completed;
        items_[idx].bytesTransferred = items_[idx].totalBytes;
        emit dataChanged(index(idx), index(idx));

        QString fileName = QFileInfo(localPath).fileName();
        emit operationCompleted(fileName);

        // Update batch progress
        int batchId = items_[idx].batchId;
        if (TransferBatch *batch = findBatch(batchId)) {
            batch->completedCount++;
            emit batchProgressUpdate(batchId, batch->completedCount, batch->totalCount());

            // Check if batch is complete
            if (batch->isComplete()) {
                completeBatch(batchId);
                return;  // processNext will be called after batch cleanup
            }
        }
    }

    // Reset processing state before scheduling next operation
    processing_ = false;
    transitionTo(QueueState::Idle);
    emit queueChanged();
    scheduleProcessNext();
}

void TransferQueue::onDownloadProgress(const QString &file, qint64 received, qint64 total)
{
    Q_UNUSED(file)
    // Reset timeout on activity
    startOperationTimeout();

    if (currentIndex_ >= 0 && currentIndex_ < items_.size()) {
        items_[currentIndex_].bytesTransferred = received;
        items_[currentIndex_].totalBytes = total;
        emit dataChanged(index(currentIndex_), index(currentIndex_));
    }
}

void TransferQueue::onDownloadFinished(const QString &remotePath, const QString &localPath)
{
    stopOperationTimeout();

    qDebug() << "[QUEUE] onDownloadFinished received"
             << "remotePath:" << remotePath
             << "localPath:" << localPath
             << "items_.size():" << items_.size()
             << "processing_:" << processing_
             << "state_:" << static_cast<int>(state_)
             << "currentIndex_:" << currentIndex_;

    int idx = findItemIndex(localPath, remotePath);
    qDebug() << "[QUEUE] findItemIndex returned:" << idx;

    if (idx >= 0) {
        items_[idx].status = TransferItem::Status::Completed;
        items_[idx].bytesTransferred = items_[idx].totalBytes;
        emit dataChanged(index(idx), index(idx));

        QString fileName = QFileInfo(remotePath).fileName();
        emit operationCompleted(fileName);

        // Update batch progress
        int batchId = items_[idx].batchId;
        if (TransferBatch *batch = findBatch(batchId)) {
            batch->completedCount++;
            emit batchProgressUpdate(batchId, batch->completedCount, batch->totalCount());

            // Check if batch is complete
            if (batch->isComplete()) {
                completeBatch(batchId);
                return;  // processNext will be called after batch cleanup
            }
        }
    } else {
        // Debug: show what we were looking for vs what's in the queue
        qDebug() << "TransferQueue: ERROR - item not found! Queue contents:";
        for (int i = 0; i < items_.size(); ++i) {
            qDebug() << "  Item" << i << "remote:" << items_[i].remotePath << "local:" << items_[i].localPath;
        }
    }

    // Reset processing state before scheduling next operation
    processing_ = false;
    transitionTo(QueueState::Idle);
    emit queueChanged();
    scheduleProcessNext();
}

void TransferQueue::onFtpError(const QString &message)
{
    stopOperationTimeout();

    // Check if this error is from a recursive delete operation
    // If so, skip the failed item and continue with the next delete
    if (state_ == QueueState::Deleting && currentDeleteIndex_ < deleteQueue_.size()) {
        qDebug() << "TransferQueue: Delete operation failed, skipping item:"
                 << deleteQueue_[currentDeleteIndex_].path << "-" << message;

        // Log the failure but continue with remaining items
        QString fileName = QFileInfo(deleteQueue_[currentDeleteIndex_].path).fileName();
        emit operationFailed(fileName, message);

        // Skip this item and continue
        currentDeleteIndex_++;
        emit queueChanged();
        processNextDelete();
        return;
    }

    // Clear any pending listing requests to prevent stale entries from being processed later
    // This is defensive - if a listing was in progress and failed, we need to reset
    requestedListings_.clear();
    requestedDeleteListings_.clear();
    requestedFolderCheckListings_.clear();
    requestedUploadFileCheckListings_.clear();

    // Also clear pending scans since they may have been interrupted
    pendingScans_.clear();
    pendingDeleteScans_.clear();
    scanningDirectories_ = false;

    // Clear upload file existence check state
    checkingUploadFileExists_ = false;

    // Clear directory creation state to prevent queue from getting stuck
    creatingDirectory_ = false;
    pendingMkdirs_.clear();

    if (currentIndex_ >= 0 && currentIndex_ < items_.size()) {
        items_[currentIndex_].status = TransferItem::Status::Failed;
        items_[currentIndex_].errorMessage = message;
        emit dataChanged(index(currentIndex_), index(currentIndex_));

        QString fileName = QFileInfo(
            items_[currentIndex_].operationType == OperationType::Upload
                ? items_[currentIndex_].localPath
                : items_[currentIndex_].remotePath
        ).fileName();
        emit operationFailed(fileName, message);

        // Update batch progress
        int batchId = items_[currentIndex_].batchId;
        if (TransferBatch *batch = findBatch(batchId)) {
            batch->failedCount++;
            emit batchProgressUpdate(batchId, batch->completedCount + batch->failedCount, batch->totalCount());

            // Check if batch is complete (all items processed, even if some failed)
            if (batch->isComplete()) {
                completeBatch(batchId);
                return;  // processNext will be called after batch cleanup
            }
        }
    }

    // Reset processing state before scheduling next operation
    processing_ = false;
    transitionTo(QueueState::Idle);
    emit queueChanged();
    scheduleProcessNext();
}

void TransferQueue::onDirectoryCreated(const QString &path)
{
    Q_UNUSED(path)
    if (state_ != QueueState::CreatingDirectories) return;

    creatingDirectory_ = false;
    transitionTo(QueueState::Idle);

    // Check if this was for our recursive upload
    if (!pendingMkdirs_.isEmpty()) {
        pendingMkdirs_.dequeue();
        directoriesCreated_++;

        // Emit progress update
        emit directoryCreationProgress(directoriesCreated_, totalDirectoriesToCreate_);

        // If this was the last directory, queue all files for upload
        if (pendingMkdirs_.isEmpty()) {
            // Use currentFolderUpload_ which has the root paths
            // (the dequeued item would have the LAST subdirectory's paths)
            processRecursiveUpload(currentFolderUpload_.localDir, currentFolderUpload_.targetDir);
        } else {
            // Continue creating directories
            processPendingDirectoryCreation();
        }
    }
}

void TransferQueue::onDirectoryListed(const QString &path, const QList<FtpEntry> &entries)
{
    qDebug() << "TransferQueue: onDirectoryListed path:" << path << "entries:" << entries.size();
    qDebug() << "TransferQueue: requestedListings_ contains path:" << requestedListings_.contains(path);
    qDebug() << "TransferQueue: requestedDeleteListings_ contains path:" << requestedDeleteListings_.contains(path);
    qDebug() << "TransferQueue: requestedFolderCheckListings_ contains path:" << requestedFolderCheckListings_.contains(path);
    qDebug() << "TransferQueue: requestedUploadFileCheckListings_ contains path:" << requestedUploadFileCheckListings_.contains(path);

    // Check if this is for folder existence check (recursive upload)
    if (requestedFolderCheckListings_.contains(path)) {
        onDirectoryListedForFolderCheck(path, entries);
        return;
    }

    // Check if this is for upload file existence check
    if (requestedUploadFileCheckListings_.contains(path)) {
        onDirectoryListedForUploadCheck(path, entries);
        return;
    }

    // Check if this is for a recursive delete operation
    if (requestedDeleteListings_.contains(path)) {
        onDirectoryListedForDelete(path, entries);
        return;
    }

    // Only process listings we explicitly requested for recursive download
    if (!requestedListings_.contains(path)) {
        qDebug() << "TransferQueue: IGNORING - not our listing";
        return;  // This listing is for RemoteFileModel, not us
    }

    // Remove from our tracking set
    requestedListings_.remove(path);

    // Find the matching pending scan
    PendingScan currentScan;
    bool found = false;
    for (int i = 0; i < pendingScans_.size(); ++i) {
        if (pendingScans_[i].remotePath == path) {
            currentScan = pendingScans_.takeAt(i);
            found = true;
            break;
        }
    }

    if (!found) {
        qDebug() << "TransferQueue: ERROR - no matching pending scan found!";
        return;  // Shouldn't happen, but be safe
    }

    // Update scanning progress
    directoriesScanned_++;

    qDebug() << "TransferQueue: Processing scan for" << path << "-> local base:" << currentScan.localBasePath;

    // Calculate the local directory for this scan
    // The localBasePath in the scan is the root destination (e.g., ~/OneDrive/C64/BBS)
    // We need to calculate the subdirectory path relative to the remote base
    QString localTargetDir;
    if (path == recursiveRemoteBase_) {
        localTargetDir = currentScan.localBasePath;
    } else {
        // Calculate relative path from remote base
        QString relativePath = path.mid(recursiveRemoteBase_.length());
        if (relativePath.startsWith('/')) relativePath = relativePath.mid(1);
        localTargetDir = currentScan.localBasePath + '/' + relativePath;
    }

    qDebug() << "TransferQueue: localTargetDir:" << localTargetDir;

    // Process entries
    for (const FtpEntry &entry : entries) {
        QString entryRemotePath = path;
        if (!entryRemotePath.endsWith('/')) entryRemotePath += '/';
        entryRemotePath += entry.name;

        if (entry.isDirectory) {
            // Create local directory
            QString localDirPath = localTargetDir + '/' + entry.name;
            qDebug() << "TransferQueue: Creating subdir:" << localDirPath;
            QDir().mkpath(localDirPath);

            // Queue subdirectory for scanning with the same batch ID
            PendingScan subScan;
            subScan.remotePath = entryRemotePath;
            subScan.localBasePath = currentScan.localBasePath;  // Keep the same base
            subScan.batchId = currentScan.batchId;  // Propagate batch ID
            pendingScans_.enqueue(subScan);

            // Track that we'll request this listing
            requestedListings_.insert(entryRemotePath);
            qDebug() << "TransferQueue: Queued subdir scan:" << entryRemotePath << "batch:" << currentScan.batchId;
        } else {
            // Queue file for download to the scan's batch
            QString localFilePath = localTargetDir + '/' + entry.name;
            qDebug() << "TransferQueue: Queuing download:" << entryRemotePath << "->" << localFilePath << "batch:" << currentScan.batchId;
            filesDiscovered_++;
            enqueueDownload(entryRemotePath, localFilePath, currentScan.batchId);
        }
    }

    // Emit scanning progress update
    emit scanningProgress(directoriesScanned_, pendingScans_.size(), filesDiscovered_);

    // Continue scanning if there are more directories
    if (!pendingScans_.isEmpty()) {
        PendingScan next = pendingScans_.head();
        qDebug() << "TransferQueue: Next scan:" << next.remotePath;
        ftpClient_->list(next.remotePath);
    } else {
        qDebug() << "TransferQueue: All scans complete, filesDiscovered:" << filesDiscovered_;
        // All directories scanned - now start processing the download queue
        scanningDirectories_ = false;

        // If no files were discovered (empty folder), complete the batch immediately
        if (filesDiscovered_ == 0 && hasActiveBatch()) {
            int batchId = batches_[activeBatchIndex_].batchId;
            qDebug() << "TransferQueue: Empty folder - completing batch" << batchId << "immediately";
            emit statusMessage(tr("Folder is empty - nothing to download"), 3000);
            completeBatch(batchId);
        } else {
            transitionTo(QueueState::Idle);
            scheduleProcessNext();
        }
    }
}

void TransferQueue::enqueueDelete(const QString &remotePath, bool isDirectory)
{
    // Ensure we have an active batch for deletes
    int batchIdx = activeBatchIndex_;
    if (batchIdx < 0 || batches_[batchIdx].operationType != OperationType::Delete) {
        QString fileName = QFileInfo(remotePath).fileName();
        // For recursive deletes, use the base path as source for duplicate detection
        QString sourcePath = !recursiveDeleteBase_.isEmpty() ? recursiveDeleteBase_ : QString();
        int batchId = createBatch(OperationType::Delete, tr("Deleting %1").arg(fileName), sourcePath);
        // Find the batch we just created
        for (int i = 0; i < batches_.size(); ++i) {
            if (batches_[i].batchId == batchId) {
                batchIdx = i;
                break;
            }
        }
    }

    if (batchIdx < 0 || batchIdx >= batches_.size()) {
        qWarning() << "TransferQueue::enqueueDelete - no valid batch";
        return;
    }

    TransferItem item;
    item.remotePath = remotePath;
    item.operationType = OperationType::Delete;
    item.status = TransferItem::Status::Pending;
    item.isDirectory = isDirectory;
    item.batchId = batches_[batchIdx].batchId;

    beginInsertRows(QModelIndex(), items_.size(), items_.size());
    items_.append(item);
    endInsertRows();

    // Add to batch
    batches_[batchIdx].items.append(item);

    // Activate the batch if not already active
    if (activeBatchIndex_ < 0) {
        activeBatchIndex_ = batchIdx;
        batches_[batchIdx].isActive = true;
        batches_[batchIdx].hasBeenProcessed = true;
        emit batchStarted(batches_[batchIdx].batchId);
    }

    emit queueChanged();

    if (state_ == QueueState::Idle) {
        scheduleProcessNext();
    }
}

void TransferQueue::enqueueRecursiveDelete(const QString &remotePath)
{
    if (!ftpClient_ || !ftpClient_->isConnected()) {
        return;
    }

    qDebug() << "TransferQueue: enqueueRecursiveDelete" << remotePath;

    // Normalize remote path - remove trailing slashes
    QString normalizedPath = remotePath;
    while (normalizedPath.endsWith('/') && normalizedPath.length() > 1) {
        normalizedPath.chop(1);
    }

    // Check if this path is already being deleted
    if (isPathBeingTransferred(normalizedPath, OperationType::Delete)) {
        qDebug() << "TransferQueue: Ignoring duplicate delete request for" << normalizedPath;
        emit statusMessage(tr("'%1' is already being deleted").arg(QFileInfo(normalizedPath).fileName()), 3000);
        return;
    }

    // Clear any previous delete state
    deleteQueue_.clear();
    currentDeleteIndex_ = 0;
    totalDeleteItems_ = 0;
    deletedCount_ = 0;
    recursiveDeleteBase_ = normalizedPath;  // Track base path for duplicate detection

    // Initialize scanning progress tracking
    QString folderName = QFileInfo(normalizedPath).fileName();
    scanningFolderName_ = folderName;
    directoriesScanned_ = 0;
    filesDiscovered_ = 0;

    // Queue the initial directory scan
    PendingDeleteScan scan;
    scan.remotePath = normalizedPath;
    pendingDeleteScans_.enqueue(scan);

    // Track that we're requesting this listing
    requestedDeleteListings_.insert(normalizedPath);
    qDebug() << "TransferQueue: Requesting delete listing for:" << normalizedPath;

    // Signal that scanning has started - this will show the progress widget immediately
    emit scanningStarted(folderName, OperationType::Delete);
    emit scanningProgress(0, 1, 0);  // Initial: 0 scanned, 1 remaining, 0 files found
    emit queueChanged();

    // Start scanning
    ftpClient_->list(normalizedPath);
}

void TransferQueue::onDirectoryListedForDelete(const QString &path, const QList<FtpEntry> &entries)
{
    qDebug() << "TransferQueue: onDirectoryListedForDelete path:" << path << "entries:" << entries.size();

    // Remove from our tracking set
    requestedDeleteListings_.remove(path);

    // Find the matching pending scan
    bool found = false;
    for (int i = 0; i < pendingDeleteScans_.size(); ++i) {
        if (pendingDeleteScans_[i].remotePath == path) {
            pendingDeleteScans_.removeAt(i);
            found = true;
            break;
        }
    }

    if (!found) {
        qDebug() << "TransferQueue: ERROR - no matching pending delete scan found!";
        return;
    }

    // Update scanning progress
    directoriesScanned_++;

    // Process entries - files first, then queue subdirectories for scanning
    for (const FtpEntry &entry : entries) {
        QString entryPath = path;
        if (!entryPath.endsWith('/')) {
            entryPath += '/';
        }
        entryPath += entry.name;

        if (entry.isDirectory) {
            // Queue subdirectory for scanning
            PendingDeleteScan subScan;
            subScan.remotePath = entryPath;
            pendingDeleteScans_.enqueue(subScan);
            requestedDeleteListings_.insert(entryPath);
            qDebug() << "TransferQueue: Queued subdir for delete scan:" << entryPath;
        } else {
            // Add file to delete queue
            DeleteItem item;
            item.path = entryPath;
            item.isDirectory = false;
            deleteQueue_.append(item);
            filesDiscovered_++;
            qDebug() << "TransferQueue: Queued file for delete:" << entryPath;
        }
    }

    // Add the directory itself to the queue AFTER its contents (depth-first)
    DeleteItem dirItem;
    dirItem.path = path;
    dirItem.isDirectory = true;
    deleteQueue_.append(dirItem);
    qDebug() << "TransferQueue: Queued directory for delete:" << path;

    // Emit scanning progress update
    emit scanningProgress(directoriesScanned_, pendingDeleteScans_.size(), filesDiscovered_);

    // Continue scanning if there are more directories
    if (!pendingDeleteScans_.isEmpty()) {
        PendingDeleteScan next = pendingDeleteScans_.head();
        qDebug() << "TransferQueue: Next delete scan:" << next.remotePath;
        ftpClient_->list(next.remotePath);
    } else {
        qDebug() << "TransferQueue: All delete scans complete, sorting and starting deletes";

        // Sort the queue: files first, then directories by depth (deepest first)
        std::sort(deleteQueue_.begin(), deleteQueue_.end(),
            [](const DeleteItem &a, const DeleteItem &b) {
                // Files before directories
                if (!a.isDirectory && b.isDirectory) {
                    return true;
                }
                if (a.isDirectory && !b.isDirectory) {
                    return false;
                }
                // For directories, sort by depth (more slashes = deeper = comes first)
                if (a.isDirectory && b.isDirectory) {
                    int depthA = a.path.count('/');
                    int depthB = b.path.count('/');
                    return depthA > depthB;  // Deeper directories first
                }
                // Files can stay in any order
                return false;
            });

        qDebug() << "TransferQueue: Delete queue sorted, first item:"
                 << (deleteQueue_.isEmpty() ? "empty" : deleteQueue_.first().path);

        // All directories scanned - now start processing the delete queue
        totalDeleteItems_ = deleteQueue_.size();
        currentDeleteIndex_ = 0;
        deletedCount_ = 0;
        processingDelete_ = true;
        transitionTo(QueueState::Deleting);

        // Notify that scanning is complete and we're starting actual deletes
        emit queueChanged();

        processNextDelete();
    }
}

void TransferQueue::processNextDelete()
{
    if (!ftpClient_ || !ftpClient_->isConnected()) {
        qDebug() << "TransferQueue: processNextDelete - FTP client not ready";
        processingDelete_ = false;
        transitionTo(QueueState::Idle);
        return;
    }

    if (currentDeleteIndex_ >= deleteQueue_.size()) {
        qDebug() << "TransferQueue: All deletes complete";
        processingDelete_ = false;
        transitionTo(QueueState::Idle);
        deleteQueue_.clear();
        recursiveDeleteBase_.clear();
        emit operationCompleted(tr("Deleted %1 items").arg(deletedCount_));

        // Check if we have a pending upload after delete (Replace operation)
        if (compoundOp_.phase == CompoundOperation::Deleting) {
            qDebug() << "TransferQueue: Delete completed, starting pending upload";
            pendingUploadAfterDelete_ = false;  // Keep legacy flag in sync
            compoundOp_.phase = CompoundOperation::Uploading;
            startRecursiveUpload();
        } else {
            compoundOp_.clear();
            emit allOperationsCompleted();
        }
        return;
    }

    const DeleteItem &item = deleteQueue_[currentDeleteIndex_];
    qDebug() << "TransferQueue: Deleting" << (currentDeleteIndex_ + 1) << "of" << totalDeleteItems_
             << ":" << item.path << (item.isDirectory ? "(dir)" : "(file)");

    if (item.isDirectory) {
        ftpClient_->removeDirectory(item.path);
    } else {
        ftpClient_->remove(item.path);
    }
}

void TransferQueue::onFileRemoved(const QString &path)
{
    qDebug() << "TransferQueue: onFileRemoved" << path;

    // Check if this is part of a recursive delete operation
    if (state_ == QueueState::Deleting && currentDeleteIndex_ < deleteQueue_.size()) {
        if (deleteQueue_[currentDeleteIndex_].path == path) {
            deletedCount_++;
            currentDeleteIndex_++;

            // Emit progress update with filename
            QString fileName = QFileInfo(path).fileName();
            emit deleteProgressUpdate(fileName, deletedCount_, totalDeleteItems_);

            emit queueChanged();
            processNextDelete();
            return;
        }
    }

    // Check if this is a single delete operation in the regular queue
    for (int i = 0; i < items_.size(); ++i) {
        if (items_[i].operationType == OperationType::Delete &&
            items_[i].remotePath == path &&
            items_[i].status == TransferItem::Status::InProgress) {

            stopOperationTimeout();
            items_[i].status = TransferItem::Status::Completed;
            emit dataChanged(index(i), index(i));

            QString fileName = QFileInfo(path).fileName();
            emit operationCompleted(fileName);

            // Update batch progress
            int batchId = items_[i].batchId;
            if (TransferBatch *batch = findBatch(batchId)) {
                batch->completedCount++;
                emit batchProgressUpdate(batchId, batch->completedCount, batch->totalCount());

                // Check if batch is complete
                if (batch->isComplete()) {
                    completeBatch(batchId);
                    return;
                }
            }

            // Reset processing state before scheduling next
            processing_ = false;
            transitionTo(QueueState::Idle);
            emit queueChanged();
            scheduleProcessNext();
            return;
        }
    }
}

void TransferQueue::onDirectoryListedForFolderCheck(const QString &path, const QList<FtpEntry> &entries)
{
    qDebug() << "TransferQueue: onDirectoryListedForFolderCheck path:" << path << "entries:" << entries.size();

    // Remove from tracking set
    requestedFolderCheckListings_.remove(path);
    checkingFolderExists_ = false;
    transitionTo(QueueState::Idle);

    // Build a set of existing directory names for quick lookup
    QSet<QString> existingDirs;
    for (const FtpEntry &entry : entries) {
        if (entry.isDirectory) {
            existingDirs.insert(entry.name);
        }
    }

    // Process ALL pending folder uploads that need this parent directory listing
    // Separate them into: folders that exist (need confirmation) and folders that don't (can upload)
    QStringList existingFolderNames;

    while (!pendingFolderUploads_.isEmpty()) {
        // Peek at the front of the queue
        const PendingFolderUpload &pending = pendingFolderUploads_.head();

        // Check if this folder needs this listing (same parent directory)
        if (pending.remoteDir != path) {
            // This folder needs a different listing - stop processing
            break;
        }

        // Dequeue this folder
        PendingFolderUpload folder = pendingFolderUploads_.dequeue();

        // Check if the target folder exists in the listing
        QString targetFolderName = QFileInfo(folder.targetDir).fileName();
        folder.exists = existingDirs.contains(targetFolderName);

        qDebug() << "TransferQueue: Target folder" << targetFolderName << "exists:" << folder.exists;

        if (folder.exists) {
            // Folder exists - add to confirmation list
            existingFolderNames.append(targetFolderName);
        }

        // Add to the list of folders to upload (will be processed after any confirmation)
        foldersToUpload_.append(folder);
    }

    // If any folders exist, show ONE confirmation dialog for all of them
    if (!existingFolderNames.isEmpty()) {
        waitingForFolderExistsResponse_ = true;
        pendingConfirmation_.type = ConfirmationContext::FolderExists;
        pendingConfirmation_.operationType = OperationType::Upload;
        transitionTo(QueueState::AwaitingConfirmation);
        emit folderExistsConfirmationNeeded(existingFolderNames);
        return;  // Wait for user response
    }

    // No folders exist - start uploading the first one
    startNextFolderUpload();
}

void TransferQueue::onDirectoryListedForUploadCheck(const QString &path, const QList<FtpEntry> &entries)
{
    qDebug() << "TransferQueue: onDirectoryListedForUploadCheck path:" << path << "entries:" << entries.size();

    // Remove from tracking set
    requestedUploadFileCheckListings_.remove(path);
    checkingUploadFileExists_ = false;
    transitionTo(QueueState::Idle);

    // Get the filename we're checking for
    if (currentIndex_ < 0 || currentIndex_ >= items_.size()) {
        qDebug() << "TransferQueue: onDirectoryListedForUploadCheck - invalid currentIndex_";
        return;
    }

    QString targetFileName = QFileInfo(items_[currentIndex_].remotePath).fileName();
    bool fileExists = false;

    for (const FtpEntry &entry : entries) {
        if (!entry.isDirectory && entry.name == targetFileName) {
            fileExists = true;
            break;
        }
    }

    qDebug() << "TransferQueue: Target file" << targetFileName << "exists:" << fileExists;

    if (fileExists) {
        // File exists - ask user what to do
        waitingForOverwriteResponse_ = true;
        pendingConfirmation_.type = ConfirmationContext::FileOverwrite;
        pendingConfirmation_.operationType = OperationType::Upload;
        pendingConfirmation_.itemIndex = currentIndex_;
        transitionTo(QueueState::AwaitingConfirmation);
        emit overwriteConfirmationNeeded(targetFileName, OperationType::Upload);
    } else {
        // File doesn't exist - proceed with upload
        // Mark as confirmed to skip the check on the next scheduleProcessNext() call
        items_[currentIndex_].overwriteConfirmed = true;
        scheduleProcessNext();
    }
}

void TransferQueue::respondToFolderExists(FolderExistsResponse response)
{
    if (state_ != QueueState::AwaitingConfirmation ||
        pendingConfirmation_.type != ConfirmationContext::FolderExists) {
        return;
    }

    // Clear confirmation state
    waitingForFolderExistsResponse_ = false;
    pendingConfirmation_.clear();
    transitionTo(QueueState::Idle);

    switch (response) {
    case FolderExistsResponse::Merge:
        // Proceed with uploads for all folders - files will merge into existing folders
        qDebug() << "TransferQueue: User chose to merge" << foldersToUpload_.size() << "folders";
        replaceExistingFolders_ = false;
        startNextFolderUpload();
        break;

    case FolderExistsResponse::Replace:
        // Delete existing folders first, then upload
        qDebug() << "TransferQueue: User chose to replace" << foldersToUpload_.size() << "folders";
        replaceExistingFolders_ = true;
        startNextFolderUpload();
        break;

    case FolderExistsResponse::Cancel:
        // Cancel all uploads
        qDebug() << "TransferQueue: User cancelled folder uploads";
        foldersToUpload_.clear();
        currentFolderUpload_ = {};
        emit operationsCancelled();
        break;
    }
}

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
    qDebug() << "TransferQueue: Operation timeout! Current operation has stalled.";

    // Abort the FTP operation to prevent late completions from firing signals.
    // This must be done BEFORE marking the item as failed, otherwise the abort
    // might trigger completion signals that double-count the item.
    if (ftpClient_) {
        qDebug() << "TransferQueue: Aborting stalled FTP operation";
        ftpClient_->abort();
    }

    // Find the current in-progress item
    for (int i = 0; i < items_.size(); ++i) {
        if (items_[i].status == TransferItem::Status::InProgress) {
            QString fileName = QFileInfo(items_[i].localPath.isEmpty()
                                        ? items_[i].remotePath
                                        : items_[i].localPath).fileName();

            items_[i].status = TransferItem::Status::Failed;
            items_[i].errorMessage = tr("Operation timed out after %1 minutes")
                                     .arg(OperationTimeoutMs / 60000);
            emit dataChanged(index(i), index(i));
            emit operationFailed(fileName, items_[i].errorMessage);

            qDebug() << "TransferQueue: Marked item as failed:" << fileName;
            break;
        }
    }

    // Reset processing state
    processing_ = false;
    currentIndex_ = -1;

    // Try to process remaining items
    scheduleProcessNext();
}

// Batch management methods

int TransferQueue::createBatch(OperationType type, const QString &description, const QString &sourcePath)
{
    // Purge any completed batches before creating a new one
    // This is where the cleanup happens - completed items are removed
    // when a new operation starts, ensuring fresh progress counts
    for (int i = batches_.size() - 1; i >= 0; --i) {
        if (!batches_[i].isActive && batches_[i].isComplete()) {
            // Don't purge batches that are still being populated during scanning
            bool stillPopulating = false;
            for (const auto &scan : pendingScans_) {
                if (scan.batchId == batches_[i].batchId) {
                    stillPopulating = true;
                    break;
                }
            }
            if (!stillPopulating) {
                purgeBatch(batches_[i].batchId);
            }
        }
    }

    TransferBatch batch;
    batch.batchId = nextBatchId_++;
    batch.operationType = type;
    batch.description = description;
    batch.sourcePath = sourcePath;
    batch.isActive = false;

    batches_.append(batch);

    qDebug() << "TransferQueue: Created batch" << batch.batchId << ":" << description
             << (sourcePath.isEmpty() ? "" : QString(" (source: %1)").arg(sourcePath));

    // Notify listeners that a new batch was created
    emit queueChanged();

    return batch.batchId;
}

void TransferQueue::activateNextBatch()
{
    // Find first inactive batch
    for (int i = 0; i < batches_.size(); ++i) {
        if (!batches_[i].isActive && !batches_[i].isComplete()) {
            activeBatchIndex_ = i;
            batches_[i].isActive = true;
            batches_[i].hasBeenProcessed = true;
            qDebug() << "TransferQueue: Activated batch" << batches_[i].batchId;
            emit batchStarted(batches_[i].batchId);
            return;
        }
    }

    // No more batches to activate
    activeBatchIndex_ = -1;
    qDebug() << "TransferQueue: No more batches to activate";
}

void TransferQueue::completeBatch(int batchId)
{
    TransferBatch *batch = findBatch(batchId);
    if (!batch) {
        qDebug() << "TransferQueue: completeBatch - batch not found:" << batchId;
        return;
    }

    qDebug() << "TransferQueue: Completing batch" << batchId
             << "completed:" << batch->completedCount
             << "failed:" << batch->failedCount
             << "total:" << batch->totalCount();

    // Mark batch as inactive (completed)
    batch->isActive = false;
    activeBatchIndex_ = -1;

    // Reset processing state
    stopOperationTimeout();
    processing_ = false;
    currentIndex_ = -1;
    transitionTo(QueueState::Idle);

    emit batchCompleted(batchId);

    // Note: We don't purge immediately - items remain visible.
    // They will be purged when a new batch starts (via purgeCompletedBatches).

    // Check if a folder upload just finished
    if (folderUploadInProgress_) {
        onFolderUploadComplete();
        // onFolderUploadComplete will start the next folder if any,
        // which will create new batches, so we return here
        // Note: We preserve overwriteAll_ across folder uploads in a multi-folder session
        return;
    }

    // Activate next batch if available
    activateNextBatch();

    // If no more active batches, emit allOperationsCompleted and reset overwrite flag
    bool hasActiveBatches = false;
    for (const auto &b : batches_) {
        if (b.isActive || !b.isComplete()) {
            hasActiveBatches = true;
            break;
        }
    }

    if (!hasActiveBatches) {
        qDebug() << "TransferQueue: All batches complete";
        overwriteAll_ = false;  // Reset only when all operations are done
        emit allOperationsCompleted();
    } else if (activeBatchIndex_ >= 0) {
        // Start processing the next batch - preserve overwriteAll_ for continuity
        scheduleProcessNext();
    }
}

void TransferQueue::purgeBatch(int batchId)
{
    for (int i = 0; i < batches_.size(); ++i) {
        if (batches_[i].batchId == batchId) {
            qDebug() << "TransferQueue: Purging batch" << batchId
                     << "with" << batches_[i].items.size() << "items";

            // Remove items from the model that belong to this batch
            for (int j = items_.size() - 1; j >= 0; --j) {
                if (items_[j].batchId == batchId) {
                    beginRemoveRows(QModelIndex(), j, j);
                    items_.removeAt(j);
                    endRemoveRows();

                    if (currentIndex_ > j) {
                        currentIndex_--;
                    } else if (currentIndex_ == j) {
                        currentIndex_ = -1;
                    }
                }
            }

            // Remove the batch
            if (activeBatchIndex_ == i) {
                activeBatchIndex_ = -1;
            } else if (activeBatchIndex_ > i) {
                activeBatchIndex_--;
            }

            batches_.removeAt(i);
            emit queueChanged();
            return;
        }
    }
}

TransferBatch* TransferQueue::findBatch(int batchId)
{
    for (int i = 0; i < batches_.size(); ++i) {
        if (batches_[i].batchId == batchId) {
            return &batches_[i];
        }
    }
    return nullptr;
}

const TransferBatch* TransferQueue::findBatch(int batchId) const
{
    for (int i = 0; i < batches_.size(); ++i) {
        if (batches_[i].batchId == batchId) {
            return &batches_[i];
        }
    }
    return nullptr;
}

BatchProgress TransferQueue::activeBatchProgress() const
{
    BatchProgress progress;

    if (activeBatchIndex_ >= 0 && activeBatchIndex_ < batches_.size()) {
        const TransferBatch &batch = batches_[activeBatchIndex_];
        progress.batchId = batch.batchId;
        progress.description = batch.description;
        progress.operationType = batch.operationType;
        progress.totalItems = batch.totalCount();
        progress.completedItems = batch.completedCount;
        progress.failedItems = batch.failedCount;
    }

    // Include scanning/directory creation state
    progress.isScanning = isScanning();
    progress.isCreatingDirectories = isCreatingDirectories();
    progress.isProcessingDelete = (state_ == QueueState::Deleting);
    progress.deleteProgress = deletedCount_;
    progress.deleteTotalCount = totalDeleteItems_;

    // Include scanning progress details
    progress.scanningFolder = scanningFolderName_;
    progress.directoriesScanned = directoriesScanned_;
    progress.directoriesRemaining = pendingScans_.size() + pendingDeleteScans_.size();
    progress.filesDiscovered = filesDiscovered_;

    // Include directory creation progress
    progress.directoriesCreated = directoriesCreated_;
    progress.directoriesToCreate = totalDirectoriesToCreate_;

    return progress;
}

BatchProgress TransferQueue::batchProgress(int batchId) const
{
    BatchProgress progress;

    for (const auto &batch : batches_) {
        if (batch.batchId == batchId) {
            progress.batchId = batch.batchId;
            progress.description = batch.description;
            progress.operationType = batch.operationType;
            progress.totalItems = batch.totalCount();
            progress.completedItems = batch.completedCount;
            progress.failedItems = batch.failedCount;

            // If this is the active batch, include scanning state
            if (hasActiveBatch() && batches_[activeBatchIndex_].batchId == batchId) {
                progress.isScanning = isScanning();
                progress.isCreatingDirectories = isCreatingDirectories();
                progress.isProcessingDelete = (state_ == QueueState::Deleting);
                progress.deleteProgress = deletedCount_;
                progress.deleteTotalCount = totalDeleteItems_;
                progress.scanningFolder = scanningFolderName_;
                progress.directoriesScanned = directoriesScanned_;
                progress.directoriesRemaining = pendingScans_.size() + pendingDeleteScans_.size();
                progress.filesDiscovered = filesDiscovered_;
                progress.directoriesCreated = directoriesCreated_;
                progress.directoriesToCreate = totalDirectoriesToCreate_;
            }
            break;
        }
    }

    return progress;
}

QList<int> TransferQueue::allBatchIds() const
{
    QList<int> ids;
    for (const auto &batch : batches_) {
        ids.append(batch.batchId);
    }
    return ids;
}

bool TransferQueue::hasActiveBatch() const
{
    return activeBatchIndex_ >= 0 && activeBatchIndex_ < batches_.size();
}

int TransferQueue::queuedBatchCount() const
{
    int count = 0;
    for (const auto &batch : batches_) {
        if (!batch.isActive) {
            count++;
        }
    }
    return count;
}

bool TransferQueue::isPathBeingTransferred(const QString &path, OperationType type) const
{
    // Normalize path for comparison (remove trailing slashes)
    QString normalizedPath = path;
    while (normalizedPath.endsWith('/') && normalizedPath.length() > 1) {
        normalizedPath.chop(1);
    }

    // Check existing batches (both active and queued), but skip completed batches
    for (const auto &batch : batches_) {
        // Skip completed batches - they're no longer "in progress"
        if (batch.isComplete()) {
            continue;
        }

        if (batch.operationType == type && !batch.sourcePath.isEmpty()) {
            QString batchPath = batch.sourcePath;
            while (batchPath.endsWith('/') && batchPath.length() > 1) {
                batchPath.chop(1);
            }
            if (batchPath == normalizedPath) {
                qDebug() << "TransferQueue: Path" << path << "already has a"
                         << (type == OperationType::Download ? "download" : "upload")
                         << "batch (id:" << batch.batchId << ")";
                return true;
            }
        }
    }

    // Check pending scans (for recursive downloads in progress)
    if (type == OperationType::Download) {
        for (const auto &scan : pendingScans_) {
            QString scanPath = scan.remotePath;
            while (scanPath.endsWith('/') && scanPath.length() > 1) {
                scanPath.chop(1);
            }
            if (scanPath == normalizedPath || scanPath.startsWith(normalizedPath + '/')) {
                qDebug() << "TransferQueue: Path" << path << "already has a pending scan";
                return true;
            }
        }
    }

    // Check pending delete scans (for recursive deletes in progress)
    if (type == OperationType::Delete) {
        for (const auto &scan : pendingDeleteScans_) {
            QString scanPath = scan.remotePath;
            while (scanPath.endsWith('/') && scanPath.length() > 1) {
                scanPath.chop(1);
            }
            if (scanPath == normalizedPath || scanPath.startsWith(normalizedPath + '/')) {
                qDebug() << "TransferQueue: Path" << path << "already has a pending delete scan";
                return true;
            }
        }
    }

    return false;
}
