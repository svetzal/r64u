#include "transferqueue.h"

#include "../services/iftpclient.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

#include <algorithm>

TransferQueue::TransferQueue(QObject *parent)
    : QAbstractListModel(parent), operationTimeoutTimer_(new QTimer(this)),
      debounceTimer_(new QTimer(this))
{
    operationTimeoutTimer_->setSingleShot(true);
    connect(operationTimeoutTimer_, &QTimer::timeout, this, &TransferQueue::onOperationTimeout);

    debounceTimer_->setSingleShot(true);
    connect(debounceTimer_, &QTimer::timeout, this, &TransferQueue::onDebounceTimeout);
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

    qDebug() << "TransferQueue: State transition" << queueStateToString(state_) << "->"
             << queueStateToString(newState);

    state_ = newState;
}

transfer::State TransferQueue::toState() const
{
    transfer::State s;
    s.items = items_;
    s.currentIndex = currentIndex_;
    s.queueState = state_;
    s.batches = batches_;
    s.nextBatchId = nextBatchId_;
    s.activeBatchIndex = activeBatchIndex_;
    s.overwriteAll = overwriteAll_;
    s.autoMerge = autoMerge_;
    s.replaceExisting = replaceExisting_;
    s.pendingFolderOps = pendingFolderOps_;
    s.currentFolderOp = currentFolderOp_;
    s.pendingUploadAfterDelete = pendingUploadAfterDelete_;
    s.pendingConfirmation = pendingConfirmation_;
    s.pendingScans = pendingScans_;
    s.requestedListings = requestedListings_;
    s.scanningFolderName = scanningFolderName_;
    s.directoriesScanned = directoriesScanned_;
    s.filesDiscovered = filesDiscovered_;
    s.pendingMkdirs = pendingMkdirs_;
    s.directoriesCreated = directoriesCreated_;
    s.totalDirectoriesToCreate = totalDirectoriesToCreate_;
    s.deleteQueue = deleteQueue_;
    s.pendingDeleteScans = pendingDeleteScans_;
    s.requestedDeleteListings = requestedDeleteListings_;
    s.recursiveDeleteBase = recursiveDeleteBase_;
    s.deletedCount = deletedCount_;
    s.requestedUploadFileCheckListings = requestedUploadFileCheckListings_;
    s.requestedFolderCheckListings = requestedFolderCheckListings_;
    return s;
}

void TransferQueue::applyState(const transfer::State &s)
{
    items_ = s.items;
    currentIndex_ = s.currentIndex;
    state_ = s.queueState;
    batches_ = s.batches;
    nextBatchId_ = s.nextBatchId;
    activeBatchIndex_ = s.activeBatchIndex;
    overwriteAll_ = s.overwriteAll;
    autoMerge_ = s.autoMerge;
    replaceExisting_ = s.replaceExisting;
    pendingFolderOps_ = s.pendingFolderOps;
    currentFolderOp_ = s.currentFolderOp;
    pendingUploadAfterDelete_ = s.pendingUploadAfterDelete;
    pendingConfirmation_ = s.pendingConfirmation;
    pendingScans_ = s.pendingScans;
    requestedListings_ = s.requestedListings;
    scanningFolderName_ = s.scanningFolderName;
    directoriesScanned_ = s.directoriesScanned;
    filesDiscovered_ = s.filesDiscovered;
    pendingMkdirs_ = s.pendingMkdirs;
    directoriesCreated_ = s.directoriesCreated;
    totalDirectoriesToCreate_ = s.totalDirectoriesToCreate;
    deleteQueue_ = s.deleteQueue;
    pendingDeleteScans_ = s.pendingDeleteScans;
    requestedDeleteListings_ = s.requestedDeleteListings;
    recursiveDeleteBase_ = s.recursiveDeleteBase;
    deletedCount_ = s.deletedCount;
    requestedUploadFileCheckListings_ = s.requestedUploadFileCheckListings;
    requestedFolderCheckListings_ = s.requestedFolderCheckListings;
}

void TransferQueue::setFtpClient(IFtpClient *client)
{
    if (ftpClient_) {
        disconnect(ftpClient_, nullptr, this, nullptr);
    }

    ftpClient_ = client;

    if (ftpClient_) {
        connect(ftpClient_, &IFtpClient::uploadProgress, this, &TransferQueue::onUploadProgress);
        connect(ftpClient_, &IFtpClient::uploadFinished, this, &TransferQueue::onUploadFinished);
        connect(ftpClient_, &IFtpClient::downloadProgress, this,
                &TransferQueue::onDownloadProgress);
        connect(ftpClient_, &IFtpClient::downloadFinished, this,
                &TransferQueue::onDownloadFinished);
        connect(ftpClient_, &IFtpClient::error, this, &TransferQueue::onFtpError);
        connect(ftpClient_, &IFtpClient::directoryCreated, this,
                &TransferQueue::onDirectoryCreated);
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
            QString sourcePath =
                currentFolderOp_.sourcePath.isEmpty() ? QString() : currentFolderOp_.sourcePath;
            int batchId = createBatch(OperationType::Upload, tr("Uploading %1").arg(fileName),
                                      fileName, sourcePath);
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

void TransferQueue::enqueueDownload(const QString &remotePath, const QString &localPath,
                                    int targetBatchId)
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
            QString sourcePath =
                (state_ == QueueState::Scanning) ? currentFolderOp_.sourcePath : QString();
            int batchId = createBatch(OperationType::Download, tr("Downloading %1").arg(fileName),
                                      fileName, sourcePath);
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
    if (!ftpClient_ || !ftpClient_->isConnected())
        return;

    QDir baseDir(localDir);
    if (!baseDir.exists())
        return;

    if (isPathBeingTransferred(localDir, OperationType::Upload)) {
        qDebug() << "TransferQueue: Ignoring duplicate upload request for" << localDir;
        emit statusMessage(tr("'%1' is already being uploaded").arg(QFileInfo(localDir).fileName()),
                           3000);
        return;
    }

    QString baseName = QFileInfo(localDir).fileName();
    QString targetDir = remoteDir;
    if (!targetDir.endsWith('/'))
        targetDir += '/';
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
            // New user operation starting - reset overwrite preference
            overwriteAll_ = false;
            startFolderOperation(op);
        } else {
            pendingFolderOps_.enqueue(op);
        }
        return;
    }

    // Queue for debounce and folder existence check
    pendingFolderOps_.enqueue(op);

    if (state_ == QueueState::Idle) {
        // New user operation starting - reset overwrite preference
        overwriteAll_ = false;
        transitionTo(QueueState::CollectingItems);
        debounceTimer_->start(DebounceMs);
    }
}

void TransferQueue::enqueueRecursiveDownload(const QString &remoteDir, const QString &localDir)
{
    if (!ftpClient_ || !ftpClient_->isConnected())
        return;

    QString normalizedRemote = remoteDir;
    while (normalizedRemote.endsWith('/') && normalizedRemote.length() > 1) {
        normalizedRemote.chop(1);
    }

    if (isPathBeingTransferred(normalizedRemote, OperationType::Download)) {
        qDebug() << "TransferQueue: Ignoring duplicate download request for" << normalizedRemote;
        emit statusMessage(
            tr("'%1' is already being downloaded").arg(QFileInfo(normalizedRemote).fileName()),
            3000);
        return;
    }

    QString folderName = QFileInfo(normalizedRemote).fileName();
    QString targetDir = localDir;
    if (!targetDir.endsWith('/'))
        targetDir += '/';
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
            // New user operation starting - reset overwrite preference
            overwriteAll_ = false;
            startFolderOperation(op);
        } else {
            pendingFolderOps_.enqueue(op);
        }
        return;
    }

    // Queue for debounce and confirmation
    pendingFolderOps_.enqueue(op);

    if (state_ == QueueState::Idle) {
        // New user operation starting - reset overwrite preference
        overwriteAll_ = false;
        transitionTo(QueueState::CollectingItems);
        debounceTimer_->start(DebounceMs);
    }
}

void TransferQueue::onDebounceTimeout()
{
    qDebug() << "TransferQueue: Debounce timeout, processing" << pendingFolderOps_.size()
             << "pending folder ops";

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
    auto result = transfer::checkFolderConfirmation(toState());
    applyState(result.newState);

    if (result.needsConfirmation) {
        qDebug() << "TransferQueue: Asking user about existing folders:"
                 << result.existingFolderNames;
        emit folderExistsConfirmationNeeded(result.existingFolderNames);
        return;
    }

    if (result.shouldStartFolderOp) {
        startFolderOperation(result.folderOpToStart);
    }
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
                              folderName, op.sourcePath);
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
            qDebug() << "TransferQueue: Folder" << op.targetPath
                     << "needs deletion before upload (Replace)";
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
            qDebug() << "TransferQueue: Local folder" << op.targetPath
                     << "needs deletion before download (Replace)";
            QDir localDir(op.targetPath);
            if (localDir.exists() && !localDir.removeRecursively()) {
                qDebug() << "TransferQueue: Failed to delete local folder" << op.targetPath;
                emit statusMessage(tr("Failed to delete local folder '%1'").arg(op.targetPath),
                                   5000);
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
        qWarning() << "TransferQueue: Source directory doesn't exist:"
                   << currentFolderOp_.sourcePath;
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

void TransferQueue::startScan(const QString &remotePath, const QString &localBase,
                              const QString &remoteBase, int batchId)
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

    auto listing = transfer::processDirectoryListingForDownload(currentScan, entries);

    for (const auto &[remotePath, localPath] : listing.newFileDownloads) {
        filesDiscovered_++;
        enqueueDownload(remotePath, localPath, currentScan.batchId);
    }

    for (const auto &subScan : listing.newSubScans) {
        // Compute local directory path for this subdirectory and create it
        QString relativePath = subScan.remotePath.mid(currentScan.remoteBasePath.length());
        if (relativePath.startsWith('/'))
            relativePath = relativePath.mid(1);
        QString localDirPath = currentScan.localBasePath + '/' + relativePath;
        QDir().mkpath(localDirPath);

        pendingScans_.enqueue(subScan);
        requestedListings_.insert(subScan.remotePath);
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
        if (batch.operationType == OperationType::Download && batch.scanned &&
            batch.totalCount() == 0) {
            emptyBatchIds.append(batch.batchId);
        }
    }

    for (int batchId : emptyBatchIds) {
        qDebug() << "TransferQueue: Completing empty batch" << batchId;
        completeBatch(batchId);
    }

    if (!emptyBatchIds.isEmpty()) {
        emit statusMessage(tr("%n empty folder(s) - nothing to download", "", emptyBatchIds.size()),
                           3000);
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
        int batchId = createBatch(OperationType::Delete, tr("Deleting %1").arg(fileName), fileName,
                                  sourcePath);
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

void TransferQueue::handleDirectoryListingForDelete(const QString &path,
                                                    const QList<FtpEntry> &entries)
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

    if (!found)
        return;

    directoriesScanned_++;

    auto listing = transfer::processDirectoryListingForDelete(path, entries);

    for (const auto &fileItem : listing.fileItems) {
        deleteQueue_.append(fileItem);
        filesDiscovered_++;
    }

    for (const auto &subScan : listing.newSubScans) {
        pendingDeleteScans_.enqueue(subScan);
        requestedDeleteListings_.insert(subScan.remotePath);
    }

    deleteQueue_.append(listing.directoryItem);

    emit scanningProgress(directoriesScanned_, pendingDeleteScans_.size(), filesDiscovered_);

    if (!pendingDeleteScans_.isEmpty()) {
        PendingScan next = pendingDeleteScans_.head();
        ftpClient_->list(next.remotePath);
    } else {
        deleteQueue_ = transfer::sortDeleteQueue(deleteQueue_);

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
    if (!transfer::canProcessNext(state_)) {
        qDebug() << "TransferQueue: processNext blocked by state:" << queueStateToString(state_);
        return;
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

            QString fileName =
                QFileInfo(items_[i].operationType == OperationType::Upload ? items_[i].localPath
                                                                           : items_[i].remotePath)
                    .fileName();

            // Check for file existence confirmation (downloads)
            if (items_[i].operationType == OperationType::Download && !overwriteAll_ &&
                !items_[i].confirmed) {
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
            if (items_[i].operationType == OperationType::Upload && !overwriteAll_ &&
                !items_[i].confirmed) {
                QString parentDir = QFileInfo(items_[i].remotePath).path();
                if (parentDir.isEmpty())
                    parentDir = "/";

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
        // Use the found index, not currentIndex_, in case another operation started
        currentIndex_ = idx;
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
        // Use the found index, not currentIndex_, in case another operation started
        currentIndex_ = idx;
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

        QString fileName = QFileInfo(items_[currentIndex_].operationType == OperationType::Upload
                                         ? items_[currentIndex_].localPath
                                         : items_[currentIndex_].remotePath)
                               .fileName();
        emit operationFailed(fileName, message);

        int batchId = items_[currentIndex_].batchId;
        if (TransferBatch *batch = findBatch(batchId)) {
            batch->failedCount++;
            emit batchProgressUpdate(batchId, batch->completedCount + batch->failedCount,
                                     batch->totalCount());

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

void TransferQueue::handleDirectoryListingForFolderCheck(const QString &path,
                                                         const QList<FtpEntry> &entries)
{
    requestedFolderCheckListings_.remove(path);

    transfer::State s = transfer::updateFolderExistence(toState(), path, entries);
    pendingFolderOps_ = s.pendingFolderOps;

    checkFolderConfirmation();
}

void TransferQueue::handleDirectoryListingForUploadCheck(const QString &path,
                                                         const QList<FtpEntry> &entries)
{
    requestedUploadFileCheckListings_.remove(path);

    auto result = transfer::checkUploadFileExists(toState(), entries);
    applyState(result.newState);

    if (result.fileExists) {
        emit overwriteConfirmationNeeded(result.fileName, OperationType::Upload);
    } else {
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
    // Capture the affected batch ID before applyState clears pendingConfirmation
    int affectedBatchId = -1;
    if (response == OverwriteResponse::Skip) {
        int itemIdx = pendingConfirmation_.itemIndex;
        if (itemIdx >= 0 && itemIdx < items_.size()) {
            affectedBatchId = items_[itemIdx].batchId;
        }
    }

    auto result = transfer::respondToOverwrite(toState(), response);
    applyState(result.newState);

    if (result.shouldCancelAll) {
        cancelAll();
        return;
    }

    if (!items_.isEmpty()) {
        emit dataChanged(index(0), index(items_.size() - 1));
    }

    // Check batch completion for Skip case
    if (response == OverwriteResponse::Skip && affectedBatchId >= 0) {
        if (TransferBatch *batch = findBatch(affectedBatchId)) {
            emit batchProgressUpdate(affectedBatchId, batch->completedCount, batch->totalCount());
            if (batch->isComplete()) {
                completeBatch(affectedBatchId);
                return;
            }
        }
    }

    if (result.shouldScheduleProcessNext) {
        scheduleProcessNext();
    }
}

void TransferQueue::respondToFolderExists(FolderExistsResponse response)
{
    auto result = transfer::respondToFolderExists(toState(), response);
    applyState(result.newState);

    if (result.shouldCancelFolderOps) {
        emit operationsCancelled();
        return;
    }

    if (result.shouldStartFolderOp) {
        startFolderOperation(result.folderOpToStart);
    }
}

// ============================================================================
// Batch management
// ============================================================================

int TransferQueue::createBatch(OperationType type, const QString &description,
                               const QString &folderName, const QString &sourcePath)
{
    auto result = transfer::createBatch(toState(), type, description, folderName, sourcePath);

    // createBatch may have purged completed batches — use reset model as a safe catch-all
    beginResetModel();
    items_ = result.newState.items;
    endResetModel();
    batches_ = result.newState.batches;
    nextBatchId_ = result.newState.nextBatchId;
    activeBatchIndex_ = result.newState.activeBatchIndex;

    qDebug() << "TransferQueue: Created batch" << result.batchId << ":" << description;
    emit queueChanged();

    return result.batchId;
}

void TransferQueue::activateNextBatch()
{
    transfer::State s = transfer::activateNextBatch(toState());
    activeBatchIndex_ = s.activeBatchIndex;

    if (activeBatchIndex_ >= 0) {
        qDebug() << "TransferQueue: Activated batch" << batches_[activeBatchIndex_].batchId;
        emit batchStarted(batches_[activeBatchIndex_].batchId);
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

    int completedIndex = currentIndex_;
    auto result = transfer::markItemComplete(toState(), completedIndex, status);
    applyState(result.newState);

    emit dataChanged(index(completedIndex), index(completedIndex));

    if (result.batchId >= 0) {
        if (TransferBatch *batch = findBatch(result.batchId)) {
            emit batchProgressUpdate(result.batchId, batch->completedCount, batch->totalCount());
        }
        if (result.batchIsComplete) {
            completeBatch(result.batchId);
        }
    }
}

TransferBatch *TransferQueue::findBatch(int batchId)
{
    for (auto &batch : batches_) {
        if (batch.batchId == batchId) {
            return &batch;
        }
    }
    return nullptr;
}

const TransferBatch *TransferQueue::findBatch(int batchId) const
{
    for (const auto &batch : batches_) {
        if (batch.batchId == batchId) {
            return &batch;
        }
    }
    return nullptr;
}

TransferBatch *TransferQueue::activeBatch()
{
    if (activeBatchIndex_ >= 0 && activeBatchIndex_ < batches_.size()) {
        return &batches_[activeBatchIndex_];
    }
    return nullptr;
}

int TransferQueue::findItemIndex(const QString &localPath, const QString &remotePath) const
{
    return transfer::findItemIndex(toState(), localPath, remotePath);
}

// ============================================================================
// Queue operations
// ============================================================================

void TransferQueue::clear()
{
    beginResetModel();
    applyState(transfer::clearAll(toState()));
    endResetModel();

    debounceTimer_->stop();
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

    applyState(transfer::cancelAllItems(toState()));

    debounceTimer_->stop();

    emit dataChanged(index(0), index(items_.size() - 1));
    emit queueChanged();
    emit operationsCancelled();
}

void TransferQueue::cancelBatch(int batchId)
{
    int batchIdx = transfer::findBatchIndex(toState(), batchId);
    if (batchIdx < 0) {
        return;
    }

    bool isActive = (batchIdx == activeBatchIndex_);
    if (isActive && ftpClient_ &&
        (state_ == QueueState::Transferring || state_ == QueueState::Deleting)) {
        ftpClient_->abort();
    }

    auto result = transfer::cancelBatch(toState(), batchId);
    applyState(result.newState);

    emit dataChanged(index(0), index(items_.size() - 1));
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
    return transfer::pendingCount(toState());
}

int TransferQueue::activeCount() const
{
    return transfer::activeCount(toState());
}

int TransferQueue::activeAndPendingCount() const
{
    return transfer::activeAndPendingCount(toState());
}

bool TransferQueue::isScanningForDelete() const
{
    return state_ == QueueState::Scanning && !pendingDeleteScans_.isEmpty();
}

bool TransferQueue::hasActiveBatch() const
{
    return transfer::hasActiveBatch(toState());
}

int TransferQueue::queuedBatchCount() const
{
    return transfer::queuedBatchCount(toState());
}

bool TransferQueue::isPathBeingTransferred(const QString &path, OperationType type) const
{
    return transfer::isPathBeingTransferred(toState(), path, type);
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
    return transfer::itemData(items_[index.row()], role);
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
    return transfer::computeActiveBatchProgress(toState());
}

BatchProgress TransferQueue::batchProgress(int batchId) const
{
    return transfer::computeBatchProgress(toState(), batchId);
}

QList<int> TransferQueue::allBatchIds() const
{
    return transfer::allBatchIds(toState());
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
            QString fileName = QFileInfo(items_[i].localPath.isEmpty() ? items_[i].remotePath
                                                                       : items_[i].localPath)
                                   .fileName();

            items_[i].status = TransferItem::Status::Failed;
            items_[i].errorMessage =
                tr("Operation timed out after %1 minutes").arg(OperationTimeoutMs / 60000);
            emit dataChanged(index(i), index(i));
            emit operationFailed(fileName, items_[i].errorMessage);
            break;
        }
    }

    currentIndex_ = -1;
    transitionTo(QueueState::Idle);
    scheduleProcessNext();
}
