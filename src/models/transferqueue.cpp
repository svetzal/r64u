#include "transferqueue.h"
#include "../services/iftpclient.h"

#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QDebug>
#include <algorithm>

TransferQueue::TransferQueue(QObject *parent)
    : QAbstractListModel(parent)
    , operationTimeoutTimer_(new QTimer(this))
    , debounceTimer_(new QTimer(this))
{
    operationTimeoutTimer_->setSingleShot(true);
    connect(operationTimeoutTimer_, &QTimer::timeout,
            this, &TransferQueue::onOperationTimeout);

    debounceTimer_->setSingleShot(true);
    connect(debounceTimer_, &QTimer::timeout,
            this, &TransferQueue::onDebounceTimeout);
}

TransferQueue::~TransferQueue()
{
    if (ftpClient_) {
        disconnect(ftpClient_, nullptr, this, nullptr);
    }
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
    if (state_ == newState) {
        return;
    }

    qDebug() << "TransferQueue: State transition"
             << queueStateToString(state_) << "->" << queueStateToString(newState);

    state_ = newState;
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

// ============================================================================
// Single-file enqueue operations
// ============================================================================

void TransferQueue::enqueueUpload(const QString &localPath, const QString &remotePath, int targetBatchId)
{
    int batchIdx = -1;
    if (targetBatchId >= 0) {
        for (int i = 0; i < batches_.size(); ++i) {
            if (batches_[i].batchId == targetBatchId) {
                batchIdx = i;
                break;
            }
        }
    }

    if (batchIdx < 0) {
        batchIdx = activeBatchIndex_;
        if (batchIdx < 0 || batches_[batchIdx].operationType != OperationType::Upload) {
            QString fileName = QFileInfo(localPath).fileName();
            QString sourcePath = currentFolderOp_.sourcePath.isEmpty() ? QString() : currentFolderOp_.sourcePath;
            int batchId = createBatch(OperationType::Upload, tr("Uploading %1").arg(fileName), fileName, sourcePath);
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

    batches_[batchIdx].items.append(item);

    if (activeBatchIndex_ < 0) {
        activeBatchIndex_ = batchIdx;
        batches_[batchIdx].scanned = true;
        batches_[batchIdx].folderConfirmed = true;
        emit batchStarted(batches_[batchIdx].batchId);
    }

    emit queueChanged();

    if (state_ == QueueState::Idle) {
        scheduleProcessNext();
    }
}

void TransferQueue::enqueueDownload(const QString &remotePath, const QString &localPath, int targetBatchId)
{
    int batchIdx = -1;

    if (targetBatchId >= 0) {
        for (int i = 0; i < batches_.size(); ++i) {
            if (batches_[i].batchId == targetBatchId) {
                batchIdx = i;
                break;
            }
        }
    }

    if (batchIdx < 0) {
        batchIdx = activeBatchIndex_;
        if (batchIdx < 0 || batches_[batchIdx].operationType != OperationType::Download) {
            QString fileName = QFileInfo(remotePath).fileName();
            QString sourcePath = (state_ == QueueState::Scanning) ? currentFolderOp_.sourcePath : QString();
            int batchId = createBatch(OperationType::Download, tr("Downloading %1").arg(fileName), fileName, sourcePath);
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

    batches_[batchIdx].items.append(item);

    if (activeBatchIndex_ < 0) {
        activeBatchIndex_ = batchIdx;
        batches_[batchIdx].scanned = true;
        batches_[batchIdx].folderConfirmed = true;
        emit batchStarted(batches_[batchIdx].batchId);
    }

    emit queueChanged();

    if (state_ == QueueState::Idle) {
        scheduleProcessNext();
    }
}

// ============================================================================
// Recursive folder operations
// ============================================================================

void TransferQueue::enqueueRecursiveUpload(const QString &localDir, const QString &remoteDir)
{
    if (!ftpClient_ || !ftpClient_->isConnected()) return;

    QDir baseDir(localDir);
    if (!baseDir.exists()) return;

    if (isPathBeingTransferred(localDir, OperationType::Upload)) {
        qDebug() << "TransferQueue: Ignoring duplicate upload request for" << localDir;
        emit statusMessage(tr("'%1' is already being uploaded").arg(QFileInfo(localDir).fileName()), 3000);
        return;
    }

    QString baseName = QFileInfo(localDir).fileName();
    QString targetDir = remoteDir;
    if (!targetDir.endsWith('/')) targetDir += '/';
    targetDir += baseName;

    PendingFolderOp op;
    op.operationType = OperationType::Upload;
    op.sourcePath = localDir;
    op.destPath = remoteDir;
    op.targetPath = targetDir;
    op.destExists = false;

    if (autoMerge_) {
        // Skip confirmation, go straight to processing
        if (state_ == QueueState::Idle) {
            startFolderOperation(op);
        } else {
            pendingFolderOps_.enqueue(op);
        }
        return;
    }

    // Queue for debounce and folder existence check
    pendingFolderOps_.enqueue(op);

    if (state_ == QueueState::Idle) {
        transitionTo(QueueState::CollectingItems);
        debounceTimer_->start(DebounceMs);
    }
}

void TransferQueue::enqueueRecursiveDownload(const QString &remoteDir, const QString &localDir)
{
    if (!ftpClient_ || !ftpClient_->isConnected()) return;

    QString normalizedRemote = remoteDir;
    while (normalizedRemote.endsWith('/') && normalizedRemote.length() > 1) {
        normalizedRemote.chop(1);
    }

    if (isPathBeingTransferred(normalizedRemote, OperationType::Download)) {
        qDebug() << "TransferQueue: Ignoring duplicate download request for" << normalizedRemote;
        emit statusMessage(tr("'%1' is already being downloaded").arg(QFileInfo(normalizedRemote).fileName()), 3000);
        return;
    }

    QString folderName = QFileInfo(normalizedRemote).fileName();
    QString targetDir = localDir;
    if (!targetDir.endsWith('/')) targetDir += '/';
    targetDir += folderName;

    PendingFolderOp op;
    op.operationType = OperationType::Download;
    op.sourcePath = normalizedRemote;
    op.destPath = localDir;
    op.targetPath = targetDir;
    op.destExists = QDir(targetDir).exists();

    if (autoMerge_ || !op.destExists) {
        // Skip confirmation, go straight to processing
        if (state_ == QueueState::Idle) {
            startFolderOperation(op);
        } else {
            pendingFolderOps_.enqueue(op);
        }
        return;
    }

    // Queue for debounce and confirmation
    pendingFolderOps_.enqueue(op);

    if (state_ == QueueState::Idle) {
        transitionTo(QueueState::CollectingItems);
        debounceTimer_->start(DebounceMs);
    }
}

void TransferQueue::onDebounceTimeout()
{
    qDebug() << "TransferQueue: Debounce timeout, processing" << pendingFolderOps_.size() << "pending folder ops";

    if (pendingFolderOps_.isEmpty()) {
        transitionTo(QueueState::Idle);
        return;
    }

    // For uploads, we need to check remote folder existence
    // For downloads, we already know local folder existence
    PendingFolderOp &firstOp = pendingFolderOps_.head();

    if (firstOp.operationType == OperationType::Upload) {
        // Need to list the remote directory to check if target exists
        requestedFolderCheckListings_.insert(firstOp.destPath);
        ftpClient_->list(firstOp.destPath);
    } else {
        // Downloads: check if any folders exist and need confirmation
        checkFolderConfirmation();
    }
}

void TransferQueue::checkFolderConfirmation()
{
    QStringList existingFolders;

    for (const PendingFolderOp &op : pendingFolderOps_) {
        if (op.destExists) {
            existingFolders.append(QFileInfo(op.targetPath).fileName());
        }
    }

    if (existingFolders.isEmpty()) {
        // No folders need confirmation, start processing
        if (!pendingFolderOps_.isEmpty()) {
            PendingFolderOp op = pendingFolderOps_.dequeue();
            startFolderOperation(op);
        } else {
            transitionTo(QueueState::Idle);
        }
        return;
    }

    // Show confirmation dialog
    transitionTo(QueueState::AwaitingFolderConfirm);
    pendingConfirmation_.folderNames = existingFolders;
    pendingConfirmation_.opType = pendingFolderOps_.head().operationType;

    qDebug() << "TransferQueue: Asking user about existing folders:" << existingFolders;
    emit folderExistsConfirmationNeeded(existingFolders);
}

void TransferQueue::startFolderOperation(const PendingFolderOp &op)
{
    currentFolderOp_ = op;

    QString folderName = QFileInfo(op.sourcePath).fileName();
    qDebug() << "TransferQueue: Starting folder operation" << folderName
             << "type:" << static_cast<int>(op.operationType);

    // Create batch for this operation
    int batchId = createBatch(op.operationType,
                              op.operationType == OperationType::Upload
                                  ? tr("Uploading %1").arg(folderName)
                                  : tr("Downloading %1").arg(folderName),
                              folderName,
                              op.sourcePath);
    currentFolderOp_.batchId = batchId;

    // Mark batch as not yet scanned
    if (TransferBatch *batch = findBatch(batchId)) {
        batch->scanned = false;
        batch->folderConfirmed = true;  // Already confirmed at this point
    }

    emit operationStarted(folderName, op.operationType);

    if (op.operationType == OperationType::Upload) {
        // Handle Replace: delete existing folder first
        if (op.destExists && replaceExisting_) {
            qDebug() << "TransferQueue: Folder" << op.targetPath << "needs deletion before upload (Replace)";
            pendingUploadAfterDelete_ = true;
            enqueueRecursiveDelete(op.targetPath);
            return;
        }

        // Queue all directories for creation, then upload files
        queueDirectoriesForUpload(op.sourcePath, op.targetPath);

        if (!pendingMkdirs_.isEmpty()) {
            transitionTo(QueueState::CreatingDirectories);
            createNextDirectory();
        } else {
            // No directories to create, mark batch as scanned and process files
            if (TransferBatch *batch = findBatch(currentFolderOp_.batchId)) {
                batch->scanned = true;
            }
            finishDirectoryCreation();
        }
    } else {
        // Download: Handle Replace - delete existing local folder first
        if (op.destExists && replaceExisting_) {
            qDebug() << "TransferQueue: Local folder" << op.targetPath << "needs deletion before download (Replace)";
            QDir localDir(op.targetPath);
            if (localDir.exists() && !localDir.removeRecursively()) {
                qDebug() << "TransferQueue: Failed to delete local folder" << op.targetPath;
                emit statusMessage(tr("Failed to delete local folder '%1'").arg(op.targetPath), 5000);
            }
        }

        // Create local base directory
        QDir().mkpath(op.targetPath);

        // Start scanning remote directory
        startScan(op.sourcePath, op.targetPath, op.sourcePath, currentFolderOp_.batchId);
    }
}

void TransferQueue::onFolderOperationComplete()
{
    qDebug() << "TransferQueue: Folder operation complete:" << currentFolderOp_.targetPath;

    currentFolderOp_ = PendingFolderOp();

    // Process next pending folder operation if any
    if (!pendingFolderOps_.isEmpty()) {
        PendingFolderOp op = pendingFolderOps_.dequeue();
        startFolderOperation(op);
        return;
    }

    // All folders done
    qDebug() << "TransferQueue: All folder operations complete";
    replaceExisting_ = false;
    emit allOperationsCompleted();
}

// ============================================================================
// Directory creation (for uploads)
// ============================================================================

void TransferQueue::queueDirectoriesForUpload(const QString &localDir, const QString &remoteDir)
{
    QDir baseDir(localDir);

    // Queue root directory
    PendingMkdir rootMkdir;
    rootMkdir.remotePath = remoteDir;
    rootMkdir.localDir = localDir;
    rootMkdir.remoteBase = remoteDir;
    pendingMkdirs_.enqueue(rootMkdir);

    // Queue all subdirectories
    QDirIterator it(localDir, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString subDir = it.next();
        QString relativePath = baseDir.relativeFilePath(subDir);
        QString remotePath = remoteDir + '/' + relativePath;

        PendingMkdir mkdir;
        mkdir.remotePath = remotePath;
        mkdir.localDir = subDir;
        mkdir.remoteBase = remoteDir;
        pendingMkdirs_.enqueue(mkdir);
    }

    directoriesCreated_ = 0;
    totalDirectoriesToCreate_ = pendingMkdirs_.size();
    emit directoryCreationProgress(0, totalDirectoriesToCreate_);
}

void TransferQueue::createNextDirectory()
{
    if (pendingMkdirs_.isEmpty()) {
        finishDirectoryCreation();
        return;
    }

    PendingMkdir mkdir = pendingMkdirs_.head();
    ftpClient_->makeDirectory(mkdir.remotePath);
}

void TransferQueue::finishDirectoryCreation()
{
    qDebug() << "TransferQueue: All directories created, queueing files for upload";

    // Mark batch as scanned
    if (TransferBatch *batch = findBatch(currentFolderOp_.batchId)) {
        batch->scanned = true;
    }

    // Queue all files for upload
    QDir dir(currentFolderOp_.sourcePath);
    if (!dir.exists()) {
        qWarning() << "TransferQueue: Source directory doesn't exist:" << currentFolderOp_.sourcePath;
        return;
    }

    QDirIterator it(currentFolderOp_.sourcePath, QDir::Files, QDirIterator::Subdirectories);
    int fileCount = 0;
    while (it.hasNext()) {
        QString filePath = it.next();
        QString relativePath = dir.relativeFilePath(filePath);
        QString remotePath = currentFolderOp_.targetPath + '/' + relativePath;

        enqueueUpload(filePath, remotePath, currentFolderOp_.batchId);
        fileCount++;
    }

    qDebug() << "TransferQueue: Queued" << fileCount << "files for upload";

    // Check for empty folder (no files)
    if (fileCount == 0) {
        if (TransferBatch *batch = findBatch(currentFolderOp_.batchId)) {
            qDebug() << "TransferQueue: Empty folder upload batch" << currentFolderOp_.batchId;
            completeBatch(currentFolderOp_.batchId);
            return;
        }
    }

    transitionTo(QueueState::Idle);
    scheduleProcessNext();
}

// ============================================================================
// Scanning (for downloads and deletes)
// ============================================================================

void TransferQueue::startScan(const QString &remotePath, const QString &localBase, const QString &remoteBase, int batchId)
{
    transitionTo(QueueState::Scanning);

    scanningFolderName_ = QFileInfo(remotePath).fileName();
    directoriesScanned_ = 0;
    filesDiscovered_ = 0;

    PendingScan scan;
    scan.remotePath = remotePath;
    scan.localBasePath = localBase;
    scan.remoteBasePath = remoteBase;
    scan.batchId = batchId;
    pendingScans_.enqueue(scan);

    requestedListings_.insert(remotePath);

    emit scanningStarted(scanningFolderName_, OperationType::Download);
    emit scanningProgress(0, 1, 0);

    ftpClient_->list(remotePath);
}

void TransferQueue::handleDirectoryListing(const QString &path, const QList<FtpEntry> &entries)
{
    requestedListings_.remove(path);

    // Find matching scan
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
        qDebug() << "TransferQueue: No matching pending scan for" << path;
        return;
    }

    directoriesScanned_++;

    // Calculate local directory for this path
    QString localTargetDir;
    if (path == currentScan.remoteBasePath) {
        localTargetDir = currentScan.localBasePath;
    } else {
        QString relativePath = path.mid(currentScan.remoteBasePath.length());
        if (relativePath.startsWith('/')) relativePath = relativePath.mid(1);
        localTargetDir = currentScan.localBasePath + '/' + relativePath;
    }

    // Process entries
    for (const FtpEntry &entry : entries) {
        QString entryRemotePath = path;
        if (!entryRemotePath.endsWith('/')) entryRemotePath += '/';
        entryRemotePath += entry.name;

        if (entry.isDirectory) {
            QString localDirPath = localTargetDir + '/' + entry.name;
            QDir().mkpath(localDirPath);

            PendingScan subScan;
            subScan.remotePath = entryRemotePath;
            subScan.localBasePath = currentScan.localBasePath;
            subScan.remoteBasePath = currentScan.remoteBasePath;
            subScan.batchId = currentScan.batchId;
            pendingScans_.enqueue(subScan);
            requestedListings_.insert(entryRemotePath);
        } else {
            QString localFilePath = localTargetDir + '/' + entry.name;
            filesDiscovered_++;
            enqueueDownload(entryRemotePath, localFilePath, currentScan.batchId);
        }
    }

    emit scanningProgress(directoriesScanned_, pendingScans_.size(), filesDiscovered_);

    if (!pendingScans_.isEmpty()) {
        PendingScan next = pendingScans_.head();
        ftpClient_->list(next.remotePath);
    } else {
        finishScanning();
    }
}

void TransferQueue::finishScanning()
{
    qDebug() << "TransferQueue: Scanning complete, filesDiscovered:" << filesDiscovered_;

    // Mark batch as scanned
    if (TransferBatch *batch = findBatch(currentFolderOp_.batchId)) {
        batch->scanned = true;
    }

    // Check for empty batches
    QList<int> emptyBatchIds;
    for (const TransferBatch &batch : batches_) {
        if (batch.operationType == OperationType::Download && batch.scanned && batch.totalCount() == 0) {
            emptyBatchIds.append(batch.batchId);
        }
    }

    for (int batchId : emptyBatchIds) {
        qDebug() << "TransferQueue: Completing empty batch" << batchId;
        completeBatch(batchId);
    }

    if (!emptyBatchIds.isEmpty()) {
        emit statusMessage(tr("%n empty folder(s) - nothing to download", "", emptyBatchIds.size()), 3000);
    }

    transitionTo(QueueState::Idle);
    scheduleProcessNext();
}

// ============================================================================
// Delete operations
// ============================================================================

void TransferQueue::enqueueDelete(const QString &remotePath, bool isDirectory)
{
    int batchIdx = activeBatchIndex_;
    if (batchIdx < 0 || batches_[batchIdx].operationType != OperationType::Delete) {
        QString fileName = QFileInfo(remotePath).fileName();
        QString sourcePath = !recursiveDeleteBase_.isEmpty() ? recursiveDeleteBase_ : QString();
        int batchId = createBatch(OperationType::Delete, tr("Deleting %1").arg(fileName), fileName, sourcePath);
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

    batches_[batchIdx].items.append(item);

    if (activeBatchIndex_ < 0) {
        activeBatchIndex_ = batchIdx;
        batches_[batchIdx].scanned = true;
        batches_[batchIdx].folderConfirmed = true;
        emit batchStarted(batches_[batchIdx].batchId);
    }

    emit queueChanged();

    if (state_ == QueueState::Idle) {
        scheduleProcessNext();
    }
}

void TransferQueue::enqueueRecursiveDelete(const QString &remotePath)
{
    if (!ftpClient_ || !ftpClient_->isConnected()) return;

    QString normalizedPath = remotePath;
    while (normalizedPath.endsWith('/') && normalizedPath.length() > 1) {
        normalizedPath.chop(1);
    }

    if (isPathBeingTransferred(normalizedPath, OperationType::Delete)) {
        qDebug() << "TransferQueue: Ignoring duplicate delete request for" << normalizedPath;
        emit statusMessage(tr("'%1' is already being deleted").arg(QFileInfo(normalizedPath).fileName()), 3000);
        return;
    }

    deleteQueue_.clear();
    deletedCount_ = 0;
    recursiveDeleteBase_ = normalizedPath;

    QString folderName = QFileInfo(normalizedPath).fileName();
    scanningFolderName_ = folderName;
    directoriesScanned_ = 0;
    filesDiscovered_ = 0;

    PendingScan scan;
    scan.remotePath = normalizedPath;
    pendingDeleteScans_.enqueue(scan);
    requestedDeleteListings_.insert(normalizedPath);

    emit scanningStarted(folderName, OperationType::Delete);
    emit scanningProgress(0, 1, 0);
    emit queueChanged();

    transitionTo(QueueState::Scanning);
    ftpClient_->list(normalizedPath);
}

void TransferQueue::handleDirectoryListingForDelete(const QString &path, const QList<FtpEntry> &entries)
{
    requestedDeleteListings_.remove(path);

    bool found = false;
    for (int i = 0; i < pendingDeleteScans_.size(); ++i) {
        if (pendingDeleteScans_[i].remotePath == path) {
            pendingDeleteScans_.removeAt(i);
            found = true;
            break;
        }
    }

    if (!found) return;

    directoriesScanned_++;

    for (const FtpEntry &entry : entries) {
        QString entryPath = path;
        if (!entryPath.endsWith('/')) entryPath += '/';
        entryPath += entry.name;

        if (entry.isDirectory) {
            PendingScan subScan;
            subScan.remotePath = entryPath;
            pendingDeleteScans_.enqueue(subScan);
            requestedDeleteListings_.insert(entryPath);
        } else {
            DeleteItem item;
            item.path = entryPath;
            item.isDirectory = false;
            deleteQueue_.append(item);
            filesDiscovered_++;
        }
    }

    // Add this directory to delete queue (after contents)
    DeleteItem dirItem;
    dirItem.path = path;
    dirItem.isDirectory = true;
    deleteQueue_.append(dirItem);

    emit scanningProgress(directoriesScanned_, pendingDeleteScans_.size(), filesDiscovered_);

    if (!pendingDeleteScans_.isEmpty()) {
        PendingScan next = pendingDeleteScans_.head();
        ftpClient_->list(next.remotePath);
    } else {
        // Sort: files first, then directories deepest-first
        std::sort(deleteQueue_.begin(), deleteQueue_.end(),
            [](const DeleteItem &a, const DeleteItem &b) {
                if (!a.isDirectory && b.isDirectory) return true;
                if (a.isDirectory && !b.isDirectory) return false;
                if (a.isDirectory && b.isDirectory) {
                    return a.path.count('/') > b.path.count('/');
                }
                return false;
            });

        transitionTo(QueueState::Deleting);
        emit queueChanged();
        processNextDelete();
    }
}

void TransferQueue::processNextDelete()
{
    if (!ftpClient_ || !ftpClient_->isConnected()) {
        transitionTo(QueueState::Idle);
        return;
    }

    if (deletedCount_ >= deleteQueue_.size()) {
        qDebug() << "TransferQueue: All deletes complete";
        transitionTo(QueueState::Idle);
        deleteQueue_.clear();
        recursiveDeleteBase_.clear();
        emit operationCompleted(tr("Deleted %1 items").arg(deletedCount_));

        if (pendingUploadAfterDelete_) {
            qDebug() << "TransferQueue: Delete completed, starting pending upload";
            pendingUploadAfterDelete_ = false;

            // Queue directories and start uploading
            queueDirectoriesForUpload(currentFolderOp_.sourcePath, currentFolderOp_.targetPath);

            if (!pendingMkdirs_.isEmpty()) {
                transitionTo(QueueState::CreatingDirectories);
                createNextDirectory();
            } else {
                finishDirectoryCreation();
            }
        } else {
            emit allOperationsCompleted();
        }
        return;
    }

    const DeleteItem &item = deleteQueue_[deletedCount_];
    qDebug() << "TransferQueue: Deleting" << (deletedCount_ + 1) << "of" << deleteQueue_.size()
             << ":" << item.path;

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
    qDebug() << "TransferQueue: processNext, state:" << queueStateToString(state_);

    if (!ftpClient_ || !ftpClient_->isConnected()) {
        qDebug() << "TransferQueue: FTP client not ready";
        return;
    }

    // State machine gates processing
    switch (state_) {
    case QueueState::CollectingItems:
    case QueueState::AwaitingFolderConfirm:
    case QueueState::AwaitingFileConfirm:
    case QueueState::Scanning:
    case QueueState::CreatingDirectories:
    case QueueState::Transferring:
    case QueueState::Deleting:
    case QueueState::BatchComplete:
        qDebug() << "TransferQueue: processNext blocked by state:" << queueStateToString(state_);
        return;

    case QueueState::Idle:
        break;
    }

    // Check for pending folder operations
    if (!pendingFolderOps_.isEmpty() && currentFolderOp_.batchId < 0) {
        PendingFolderOp op = pendingFolderOps_.dequeue();
        startFolderOperation(op);
        return;
    }

    // Find next pending item
    for (int i = 0; i < items_.size(); ++i) {
        if (items_[i].status == TransferItem::Status::Pending) {
            currentIndex_ = i;

            QString fileName = QFileInfo(
                items_[i].operationType == OperationType::Upload
                    ? items_[i].localPath : items_[i].remotePath
            ).fileName();

            // Check for file existence confirmation (downloads)
            if (items_[i].operationType == OperationType::Download &&
                !overwriteAll_ && !items_[i].confirmed) {
                QFileInfo localFile(items_[i].localPath);
                if (localFile.exists()) {
                    transitionTo(QueueState::AwaitingFileConfirm);
                    pendingConfirmation_.itemIndex = i;
                    pendingConfirmation_.opType = OperationType::Download;
                    emit overwriteConfirmationNeeded(fileName, OperationType::Download);
                    return;
                }
            }

            // Check for remote file existence (uploads)
            if (items_[i].operationType == OperationType::Upload &&
                !overwriteAll_ && !items_[i].confirmed) {
                QString parentDir = QFileInfo(items_[i].remotePath).path();
                if (parentDir.isEmpty()) parentDir = "/";

                requestedUploadFileCheckListings_.insert(parentDir);
                ftpClient_->list(parentDir);
                return;
            }

            // Start the transfer
            items_[i].status = TransferItem::Status::InProgress;
            transitionTo(QueueState::Transferring);

            emit dataChanged(index(i), index(i));
            emit operationStarted(fileName, items_[i].operationType);

            startOperationTimeout();

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

    // No more pending items
    qDebug() << "TransferQueue: No more pending items";
    stopOperationTimeout();
    currentIndex_ = -1;

    if (batches_.isEmpty()) {
        emit allOperationsCompleted();
    }
}

// ============================================================================
// FTP callbacks
// ============================================================================

void TransferQueue::onUploadProgress(const QString &file, qint64 sent, qint64 total)
{
    Q_UNUSED(file)
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
        markCurrentComplete(TransferItem::Status::Completed);

        QString fileName = QFileInfo(localPath).fileName();
        emit operationCompleted(fileName);
    }

    // Only transition to Idle if we haven't already started a new operation
    // (e.g., when batch completion triggers a new folder operation)
    if (state_ == QueueState::Transferring) {
        transitionTo(QueueState::Idle);
    }
    emit queueChanged();
    scheduleProcessNext();
}

void TransferQueue::onDownloadProgress(const QString &file, qint64 received, qint64 total)
{
    Q_UNUSED(file)
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

    int idx = findItemIndex(localPath, remotePath);
    if (idx >= 0) {
        markCurrentComplete(TransferItem::Status::Completed);

        QString fileName = QFileInfo(remotePath).fileName();
        emit operationCompleted(fileName);
    }

    // Only transition to Idle if we haven't already started a new operation
    // (e.g., when batch completion triggers a new folder operation)
    if (state_ == QueueState::Transferring) {
        transitionTo(QueueState::Idle);
    }
    emit queueChanged();
    scheduleProcessNext();
}

void TransferQueue::onFtpError(const QString &message)
{
    qDebug() << "TransferQueue: onFtpError:" << message << "state:" << queueStateToString(state_);

    stopOperationTimeout();

    // Handle delete errors - skip and continue
    if (state_ == QueueState::Deleting && deletedCount_ < deleteQueue_.size()) {
        QString fileName = QFileInfo(deleteQueue_[deletedCount_].path).fileName();
        emit operationFailed(fileName, message);

        deletedCount_++;
        emit queueChanged();
        processNextDelete();
        return;
    }

    // Clear pending requests
    requestedListings_.clear();
    requestedDeleteListings_.clear();
    requestedFolderCheckListings_.clear();
    requestedUploadFileCheckListings_.clear();
    pendingScans_.clear();
    pendingDeleteScans_.clear();
    pendingMkdirs_.clear();

    // Handle folder upload failure during directory creation
    if (currentFolderOp_.batchId > 0 && state_ == QueueState::CreatingDirectories) {
        QString folderName = QFileInfo(currentFolderOp_.sourcePath).fileName();
        emit operationFailed(folderName, message);

        completeBatch(currentFolderOp_.batchId);
        return;
    }

    // Handle transfer error
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

        int batchId = items_[currentIndex_].batchId;
        if (TransferBatch *batch = findBatch(batchId)) {
            batch->failedCount++;
            emit batchProgressUpdate(batchId, batch->completedCount + batch->failedCount, batch->totalCount());

            if (batch->isComplete()) {
                completeBatch(batchId);
                return;
            }
        }
    }

    transitionTo(QueueState::Idle);
    currentIndex_ = -1;
    emit queueChanged();
    scheduleProcessNext();
}

void TransferQueue::onDirectoryCreated(const QString &path)
{
    qDebug() << "TransferQueue: onDirectoryCreated:" << path;

    if (state_ != QueueState::CreatingDirectories) {
        return;
    }

    if (!pendingMkdirs_.isEmpty()) {
        pendingMkdirs_.dequeue();
        directoriesCreated_++;

        emit directoryCreationProgress(directoriesCreated_, totalDirectoriesToCreate_);

        if (pendingMkdirs_.isEmpty()) {
            finishDirectoryCreation();
        } else {
            createNextDirectory();
        }
    }
}

void TransferQueue::onDirectoryListed(const QString &path, const QList<FtpEntry> &entries)
{
    qDebug() << "TransferQueue: onDirectoryListed:" << path << "entries:" << entries.size();

    // Check for folder existence check (uploads)
    if (requestedFolderCheckListings_.contains(path)) {
        handleDirectoryListingForFolderCheck(path, entries);
        return;
    }

    // Check for upload file existence check
    if (requestedUploadFileCheckListings_.contains(path)) {
        handleDirectoryListingForUploadCheck(path, entries);
        return;
    }

    // Check for delete scan
    if (requestedDeleteListings_.contains(path)) {
        handleDirectoryListingForDelete(path, entries);
        return;
    }

    // Check for download scan
    if (requestedListings_.contains(path)) {
        handleDirectoryListing(path, entries);
        return;
    }

    qDebug() << "TransferQueue: Ignoring untracked listing for" << path;
}

void TransferQueue::handleDirectoryListingForFolderCheck(const QString &path, const QList<FtpEntry> &entries)
{
    requestedFolderCheckListings_.remove(path);

    QSet<QString> existingDirs;
    for (const FtpEntry &entry : entries) {
        if (entry.isDirectory) {
            existingDirs.insert(entry.name);
        }
    }

    // Update destExists for all pending folder ops with this parent
    QQueue<PendingFolderOp> updatedOps;
    while (!pendingFolderOps_.isEmpty()) {
        PendingFolderOp op = pendingFolderOps_.dequeue();
        if (op.destPath == path) {
            QString targetFolderName = QFileInfo(op.targetPath).fileName();
            op.destExists = existingDirs.contains(targetFolderName);
            qDebug() << "TransferQueue: Folder" << targetFolderName << "exists:" << op.destExists;
        }
        updatedOps.enqueue(op);
    }
    pendingFolderOps_ = updatedOps;

    checkFolderConfirmation();
}

void TransferQueue::handleDirectoryListingForUploadCheck(const QString &path, const QList<FtpEntry> &entries)
{
    requestedUploadFileCheckListings_.remove(path);

    if (currentIndex_ < 0 || currentIndex_ >= items_.size()) {
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

    if (fileExists) {
        transitionTo(QueueState::AwaitingFileConfirm);
        pendingConfirmation_.itemIndex = currentIndex_;
        pendingConfirmation_.opType = OperationType::Upload;
        emit overwriteConfirmationNeeded(targetFileName, OperationType::Upload);
    } else {
        items_[currentIndex_].confirmed = true;
        scheduleProcessNext();
    }
}

void TransferQueue::onFileRemoved(const QString &path)
{
    qDebug() << "TransferQueue: onFileRemoved:" << path;

    if (state_ == QueueState::Deleting && deletedCount_ < deleteQueue_.size()) {
        if (deleteQueue_[deletedCount_].path == path) {
            deletedCount_++;
            QString fileName = QFileInfo(path).fileName();
            emit deleteProgressUpdate(fileName, deletedCount_, deleteQueue_.size());
            emit queueChanged();
            processNextDelete();
            return;
        }
    }

    // Handle single delete in regular queue
    for (const auto &item : items_) {
        if (item.operationType == OperationType::Delete &&
            item.remotePath == path &&
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
    if (state_ != QueueState::AwaitingFileConfirm) {
        return;
    }

    int itemIdx = pendingConfirmation_.itemIndex;
    pendingConfirmation_.clear();
    transitionTo(QueueState::Idle);

    switch (response) {
    case OverwriteResponse::Overwrite:
        if (itemIdx >= 0 && itemIdx < items_.size()) {
            items_[itemIdx].confirmed = true;
        }
        scheduleProcessNext();
        break;

    case OverwriteResponse::OverwriteAll:
        overwriteAll_ = true;
        scheduleProcessNext();
        break;

    case OverwriteResponse::Skip:
        if (itemIdx >= 0 && itemIdx < items_.size()) {
            items_[itemIdx].status = TransferItem::Status::Skipped;
            items_[itemIdx].errorMessage = tr("Skipped");
            emit dataChanged(index(itemIdx), index(itemIdx));

            int batchId = items_[itemIdx].batchId;
            if (TransferBatch *batch = findBatch(batchId)) {
                batch->completedCount++;
                emit batchProgressUpdate(batchId, batch->completedCount, batch->totalCount());

                if (batch->isComplete()) {
                    completeBatch(batchId);
                    return;
                }
            }
        }
        currentIndex_ = -1;
        scheduleProcessNext();
        break;

    case OverwriteResponse::Cancel:
        cancelAll();
        break;
    }
}

void TransferQueue::respondToFolderExists(FolderExistsResponse response)
{
    if (state_ != QueueState::AwaitingFolderConfirm) {
        return;
    }

    OperationType opType = pendingConfirmation_.opType;
    pendingConfirmation_.clear();
    transitionTo(QueueState::Idle);

    switch (response) {
    case FolderExistsResponse::Merge:
        replaceExisting_ = false;
        if (!pendingFolderOps_.isEmpty()) {
            PendingFolderOp op = pendingFolderOps_.dequeue();
            startFolderOperation(op);
        }
        break;

    case FolderExistsResponse::Replace:
        replaceExisting_ = true;
        if (!pendingFolderOps_.isEmpty()) {
            PendingFolderOp op = pendingFolderOps_.dequeue();
            startFolderOperation(op);
        }
        break;

    case FolderExistsResponse::Cancel:
        pendingFolderOps_.clear();
        currentFolderOp_ = PendingFolderOp();
        emit operationsCancelled();
        break;
    }
}

// ============================================================================
// Batch management
// ============================================================================

int TransferQueue::createBatch(OperationType type, const QString &description, const QString &folderName, const QString &sourcePath)
{
    // Purge completed batches
    for (int i = batches_.size() - 1; i >= 0; --i) {
        if (batches_[i].isComplete()) {
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
    batch.folderName = folderName;
    batch.sourcePath = sourcePath;
    batch.scanned = false;
    batch.folderConfirmed = false;

    batches_.append(batch);

    qDebug() << "TransferQueue: Created batch" << batch.batchId << ":" << description;
    emit queueChanged();

    return batch.batchId;
}

void TransferQueue::activateNextBatch()
{
    for (int i = 0; i < batches_.size(); ++i) {
        if (!batches_[i].isComplete() && batches_[i].pendingCount() > 0) {
            activeBatchIndex_ = i;
            qDebug() << "TransferQueue: Activated batch" << batches_[i].batchId;
            emit batchStarted(batches_[i].batchId);
            return;
        }
    }

    activeBatchIndex_ = -1;
    qDebug() << "TransferQueue: No more batches to activate";
}

void TransferQueue::completeBatch(int batchId)
{
    TransferBatch *batch = findBatch(batchId);
    if (!batch) {
        return;
    }

    qDebug() << "TransferQueue: Completing batch" << batchId
             << "completed:" << batch->completedCount
             << "failed:" << batch->failedCount
             << "total:" << batch->totalCount();

    activeBatchIndex_ = -1;
    stopOperationTimeout();
    currentIndex_ = -1;
    transitionTo(QueueState::Idle);

    emit batchCompleted(batchId);

    // Check if this is part of a folder operation
    if (currentFolderOp_.batchId == batchId) {
        onFolderOperationComplete();
        return;
    }

    activateNextBatch();

    bool hasActiveBatches = false;
    for (const auto &b : batches_) {
        if (!b.isComplete()) {
            hasActiveBatches = true;
            break;
        }
    }

    if (!hasActiveBatches) {
        qDebug() << "TransferQueue: All batches complete";
        overwriteAll_ = false;
        emit allOperationsCompleted();
    } else if (activeBatchIndex_ >= 0) {
        scheduleProcessNext();
    }
}

void TransferQueue::purgeBatch(int batchId)
{
    for (int i = 0; i < batches_.size(); ++i) {
        if (batches_[i].batchId == batchId) {
            qDebug() << "TransferQueue: Purging batch" << batchId;

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

void TransferQueue::markCurrentComplete(TransferItem::Status status)
{
    if (currentIndex_ < 0 || currentIndex_ >= items_.size()) {
        return;
    }

    items_[currentIndex_].status = status;
    if (status == TransferItem::Status::Completed) {
        items_[currentIndex_].bytesTransferred = items_[currentIndex_].totalBytes;
    }
    emit dataChanged(index(currentIndex_), index(currentIndex_));

    int batchId = items_[currentIndex_].batchId;
    if (TransferBatch *batch = findBatch(batchId)) {
        if (status == TransferItem::Status::Completed || status == TransferItem::Status::Skipped) {
            batch->completedCount++;
        } else if (status == TransferItem::Status::Failed) {
            batch->failedCount++;
        }
        emit batchProgressUpdate(batchId, batch->completedCount, batch->totalCount());

        if (batch->isComplete()) {
            completeBatch(batchId);
        }
    }

    currentIndex_ = -1;
}

TransferBatch* TransferQueue::findBatch(int batchId)
{
    for (auto &batch : batches_) {
        if (batch.batchId == batchId) {
            return &batch;
        }
    }
    return nullptr;
}

const TransferBatch* TransferQueue::findBatch(int batchId) const
{
    for (const auto &batch : batches_) {
        if (batch.batchId == batchId) {
            return &batch;
        }
    }
    return nullptr;
}

TransferBatch* TransferQueue::activeBatch()
{
    if (activeBatchIndex_ >= 0 && activeBatchIndex_ < batches_.size()) {
        return &batches_[activeBatchIndex_];
    }
    return nullptr;
}

int TransferQueue::findItemIndex(const QString &localPath, const QString &remotePath) const
{
    for (int i = 0; i < items_.size(); ++i) {
        if (items_[i].localPath == localPath && items_[i].remotePath == remotePath) {
            return i;
        }
    }
    return -1;
}

// ============================================================================
// Queue operations
// ============================================================================

void TransferQueue::clear()
{
    beginResetModel();
    items_.clear();
    endResetModel();

    currentIndex_ = -1;
    batches_.clear();
    activeBatchIndex_ = -1;

    pendingScans_.clear();
    requestedListings_.clear();
    scanningFolderName_.clear();
    directoriesScanned_ = 0;
    filesDiscovered_ = 0;

    pendingMkdirs_.clear();
    directoriesCreated_ = 0;
    totalDirectoriesToCreate_ = 0;

    pendingDeleteScans_.clear();
    requestedDeleteListings_.clear();
    recursiveDeleteBase_.clear();
    deleteQueue_.clear();
    deletedCount_ = 0;

    requestedUploadFileCheckListings_.clear();
    requestedFolderCheckListings_.clear();

    pendingConfirmation_.clear();
    overwriteAll_ = false;
    replaceExisting_ = false;

    pendingFolderOps_.clear();
    currentFolderOp_ = PendingFolderOp();
    pendingUploadAfterDelete_ = false;

    debounceTimer_->stop();
    transitionTo(QueueState::Idle);

    emit queueChanged();
}

void TransferQueue::removeCompleted()
{
    for (int i = items_.size() - 1; i >= 0; --i) {
        if (items_[i].status == TransferItem::Status::Completed ||
            items_[i].status == TransferItem::Status::Failed ||
            items_[i].status == TransferItem::Status::Skipped) {
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

    for (auto &item : items_) {
        if (item.status == TransferItem::Status::Pending ||
            item.status == TransferItem::Status::InProgress) {
            item.status = TransferItem::Status::Failed;
            item.errorMessage = tr("Cancelled");
        }
    }

    currentIndex_ = -1;
    batches_.clear();
    activeBatchIndex_ = -1;

    pendingScans_.clear();
    requestedListings_.clear();
    pendingMkdirs_.clear();
    pendingDeleteScans_.clear();
    requestedDeleteListings_.clear();
    deleteQueue_.clear();
    deletedCount_ = 0;

    pendingConfirmation_.clear();
    pendingFolderOps_.clear();
    currentFolderOp_ = PendingFolderOp();
    replaceExisting_ = false;

    debounceTimer_->stop();
    transitionTo(QueueState::Idle);

    emit dataChanged(index(0), index(items_.size() - 1));
    emit queueChanged();
    emit operationsCancelled();
}

void TransferQueue::cancelBatch(int batchId)
{
    int batchIdx = -1;
    for (int i = 0; i < batches_.size(); ++i) {
        if (batches_[i].batchId == batchId) {
            batchIdx = i;
            break;
        }
    }

    if (batchIdx < 0) {
        return;
    }

    if (batchIdx == activeBatchIndex_) {
        if (ftpClient_ && (state_ == QueueState::Transferring || state_ == QueueState::Deleting)) {
            ftpClient_->abort();
        }
        currentIndex_ = -1;

        if (state_ == QueueState::Scanning) {
            pendingScans_.clear();
            pendingDeleteScans_.clear();
            requestedListings_.clear();
            requestedDeleteListings_.clear();
        }

        if (state_ == QueueState::CreatingDirectories) {
            pendingMkdirs_.clear();
        }

        if (state_ == QueueState::Deleting) {
            deleteQueue_.clear();
            deletedCount_ = 0;
        }

        transitionTo(QueueState::Idle);
    }

    for (auto &item : items_) {
        if (item.batchId == batchId &&
            (item.status == TransferItem::Status::Pending ||
             item.status == TransferItem::Status::InProgress)) {
            item.status = TransferItem::Status::Failed;
            item.errorMessage = tr("Cancelled");
        }
    }

    bool wasActiveBatch = (batchIdx == activeBatchIndex_);
    purgeBatch(batchId);

    emit dataChanged(index(0), index(items_.size() - 1));
    emit queueChanged();

    if (wasActiveBatch) {
        activateNextBatch();
        scheduleProcessNext();
    }
}

// ============================================================================
// Query methods
// ============================================================================

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

bool TransferQueue::isScanningForDelete() const
{
    return state_ == QueueState::Scanning && !pendingDeleteScans_.isEmpty();
}

bool TransferQueue::hasActiveBatch() const
{
    return activeBatchIndex_ >= 0 && activeBatchIndex_ < batches_.size();
}

int TransferQueue::queuedBatchCount() const
{
    int count = 0;
    for (const auto &batch : batches_) {
        if (!batch.isComplete()) {
            count++;
        }
    }
    return count;
}

bool TransferQueue::isPathBeingTransferred(const QString &path, OperationType type) const
{
    QString normalizedPath = path;
    while (normalizedPath.endsWith('/') && normalizedPath.length() > 1) {
        normalizedPath.chop(1);
    }

    for (const auto &batch : batches_) {
        if (batch.isComplete()) {
            continue;
        }

        if (batch.operationType == type && !batch.sourcePath.isEmpty()) {
            QString batchPath = batch.sourcePath;
            while (batchPath.endsWith('/') && batchPath.length() > 1) {
                batchPath.chop(1);
            }
            if (batchPath == normalizedPath) {
                return true;
            }
        }
    }

    for (const auto &scan : pendingScans_) {
        QString scanPath = scan.remotePath;
        while (scanPath.endsWith('/') && scanPath.length() > 1) {
            scanPath.chop(1);
        }
        if (scanPath == normalizedPath) {
            return true;
        }
    }

    for (const auto &op : pendingFolderOps_) {
        QString opPath = op.sourcePath;
        while (opPath.endsWith('/') && opPath.length() > 1) {
            opPath.chop(1);
        }
        if (opPath == normalizedPath && op.operationType == type) {
            return true;
        }
    }

    return false;
}

// ============================================================================
// QAbstractListModel implementation
// ============================================================================

int TransferQueue::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return items_.size();
}

QVariant TransferQueue::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= items_.size()) {
        return {};
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
    default:
        return {};
    }
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
    BatchProgress progress;

    if (activeBatchIndex_ >= 0 && activeBatchIndex_ < batches_.size()) {
        const TransferBatch &batch = batches_[activeBatchIndex_];
        progress.batchId = batch.batchId;
        progress.description = batch.description;
        progress.folderName = batch.folderName;
        progress.operationType = batch.operationType;
        progress.totalItems = batch.totalCount();
        progress.completedItems = batch.completedCount;
        progress.failedItems = batch.failedCount;
    }

    progress.isScanning = isScanning();
    progress.isCreatingDirectories = isCreatingDirectories();
    progress.isProcessingDelete = (state_ == QueueState::Deleting);
    progress.deleteProgress = deletedCount_;
    progress.deleteTotalCount = deleteQueue_.size();

    progress.scanningFolder = scanningFolderName_;
    progress.directoriesScanned = directoriesScanned_;
    progress.directoriesRemaining = pendingScans_.size() + pendingDeleteScans_.size();
    progress.filesDiscovered = filesDiscovered_;

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
            progress.folderName = batch.folderName;
            progress.operationType = batch.operationType;
            progress.totalItems = batch.totalCount();
            progress.completedItems = batch.completedCount;
            progress.failedItems = batch.failedCount;

            if (hasActiveBatch() && batches_[activeBatchIndex_].batchId == batchId) {
                progress.isScanning = isScanning();
                progress.isCreatingDirectories = isCreatingDirectories();
                progress.isProcessingDelete = (state_ == QueueState::Deleting);
                progress.deleteProgress = deletedCount_;
                progress.deleteTotalCount = deleteQueue_.size();
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
        if (!batch.isComplete()) {
            ids.append(batch.batchId);
        }
    }
    return ids;
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
            break;
        }
    }

    currentIndex_ = -1;
    transitionTo(QueueState::Idle);
    scheduleProcessNext();
}
