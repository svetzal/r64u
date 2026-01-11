#include "transferqueue.h"
#include "../services/c64uftpclient.h"

#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QDebug>
#include <algorithm>

TransferQueue::TransferQueue(QObject *parent)
    : QAbstractListModel(parent)
{
}

void TransferQueue::setFtpClient(C64UFtpClient *client)
{
    if (ftpClient_) {
        disconnect(ftpClient_, nullptr, this, nullptr);
    }

    ftpClient_ = client;

    if (ftpClient_) {
        connect(ftpClient_, &C64UFtpClient::uploadProgress,
                this, &TransferQueue::onUploadProgress);
        connect(ftpClient_, &C64UFtpClient::uploadFinished,
                this, &TransferQueue::onUploadFinished);
        connect(ftpClient_, &C64UFtpClient::downloadProgress,
                this, &TransferQueue::onDownloadProgress);
        connect(ftpClient_, &C64UFtpClient::downloadFinished,
                this, &TransferQueue::onDownloadFinished);
        connect(ftpClient_, &C64UFtpClient::error,
                this, &TransferQueue::onFtpError);
        connect(ftpClient_, &C64UFtpClient::directoryCreated,
                this, &TransferQueue::onDirectoryCreated);
        connect(ftpClient_, &C64UFtpClient::directoryListed,
                this, &TransferQueue::onDirectoryListed);
        connect(ftpClient_, &C64UFtpClient::fileRemoved,
                this, &TransferQueue::onFileRemoved);
    }
}

void TransferQueue::enqueueUpload(const QString &localPath, const QString &remotePath)
{
    TransferItem item;
    item.localPath = localPath;
    item.remotePath = remotePath;
    item.operationType = OperationType::Upload;
    item.status = TransferItem::Status::Pending;
    item.totalBytes = QFileInfo(localPath).size();

    beginInsertRows(QModelIndex(), items_.size(), items_.size());
    items_.append(item);
    endInsertRows();

    emit queueChanged();

    if (!processing_) {
        processNext();
    }
}

void TransferQueue::enqueueDownload(const QString &remotePath, const QString &localPath)
{
    TransferItem item;
    item.localPath = localPath;
    item.remotePath = remotePath;
    item.operationType = OperationType::Download;
    item.status = TransferItem::Status::Pending;

    beginInsertRows(QModelIndex(), items_.size(), items_.size());
    items_.append(item);
    endInsertRows();

    emit queueChanged();

    if (!processing_) {
        processNext();
    }
}

void TransferQueue::enqueueRecursiveUpload(const QString &localDir, const QString &remoteDir)
{
    if (!ftpClient_ || !ftpClient_->isConnected()) return;

    // Collect all directories that need to be created first
    QDir baseDir(localDir);
    if (!baseDir.exists()) return;

    QString baseName = QFileInfo(localDir).fileName();
    QString targetDir = remoteDir;
    if (!targetDir.endsWith('/')) targetDir += '/';
    targetDir += baseName;

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

    // Start creating directories
    processPendingDirectoryCreation();
}

void TransferQueue::processRecursiveUpload(const QString &localDir, const QString &remoteDir)
{
    // Queue all files for upload
    QDir dir(localDir);
    QDirIterator it(localDir, QDir::Files, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        QString filePath = it.next();
        QString relativePath = dir.relativeFilePath(filePath);
        QString remotePath = remoteDir + '/' + relativePath;

        enqueueUpload(filePath, remotePath);
    }
}

void TransferQueue::processPendingDirectoryCreation()
{
    if (creatingDirectory_ || pendingMkdirs_.isEmpty()) {
        return;
    }

    creatingDirectory_ = true;
    PendingMkdir mkdir = pendingMkdirs_.head();
    ftpClient_->makeDirectory(mkdir.remotePath);
}

void TransferQueue::enqueueRecursiveDownload(const QString &remoteDir, const QString &localDir)
{
    if (!ftpClient_ || !ftpClient_->isConnected()) return;

    qDebug() << "TransferQueue: enqueueRecursiveDownload" << remoteDir << "->" << localDir;

    // Normalize remote path - remove trailing slashes
    QString normalizedRemote = remoteDir;
    while (normalizedRemote.endsWith('/') && normalizedRemote.length() > 1) {
        normalizedRemote.chop(1);
    }

    // Set scanning mode - this prevents processNext() from starting downloads
    // until all directories have been scanned
    scanningDirectories_ = true;

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

    // Queue the initial directory scan
    PendingScan scan;
    scan.remotePath = normalizedRemote;
    scan.localBasePath = targetDir;
    pendingScans_.enqueue(scan);

    // Track that we're requesting this listing (to avoid conflict with RemoteFileModel)
    requestedListings_.insert(normalizedRemote);
    qDebug() << "TransferQueue: Requesting listing for:" << normalizedRemote;

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

    // Clear recursive operation state
    pendingScans_.clear();
    requestedListings_.clear();
    scanningDirectories_ = false;
    pendingMkdirs_.clear();
    creatingDirectory_ = false;

    // Clear recursive delete state
    pendingDeleteScans_.clear();
    requestedDeleteListings_.clear();
    deleteQueue_.clear();
    currentDeleteIndex_ = 0;
    totalDeleteItems_ = 0;
    deletedCount_ = 0;
    processingDelete_ = false;

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
    if (ftpClient_ && (processing_ || processingDelete_)) {
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

    // Clear recursive operation state
    pendingScans_.clear();
    requestedListings_.clear();
    scanningDirectories_ = false;
    pendingMkdirs_.clear();
    creatingDirectory_ = false;

    // Clear recursive delete state
    pendingDeleteScans_.clear();
    requestedDeleteListings_.clear();
    deleteQueue_.clear();
    currentDeleteIndex_ = 0;
    totalDeleteItems_ = 0;
    deletedCount_ = 0;
    processingDelete_ = false;

    emit dataChanged(index(0), index(items_.size() - 1));
    emit queueChanged();
    emit operationsCancelled();
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
             << "scanningDirectories_:" << scanningDirectories_;

    if (!ftpClient_ || !ftpClient_->isConnected()) {
        qDebug() << "TransferQueue: processNext - FTP client not ready, stopping";
        processing_ = false;
        return;
    }

    // Don't start transfers while we're still scanning directories
    if (scanningDirectories_) {
        qDebug() << "TransferQueue: processNext - waiting for directory scanning to complete";
        return;
    }

    // Don't proceed while waiting for overwrite confirmation
    if (waitingForOverwriteResponse_) {
        qDebug() << "TransferQueue: processNext - waiting for overwrite response";
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

            // Check for file existence and ask for overwrite confirmation
            if (items_[i].operationType == OperationType::Download && !overwriteAll_) {
                QFileInfo localFile(items_[i].localPath);
                if (localFile.exists()) {
                    qDebug() << "TransferQueue: File exists, asking for confirmation:" << items_[i].localPath;
                    waitingForOverwriteResponse_ = true;
                    emit overwriteConfirmationNeeded(fileName, OperationType::Download);
                    return;
                }
            }

            items_[i].status = TransferItem::Status::InProgress;
            processing_ = true;

            emit dataChanged(index(i), index(i));
            emit operationStarted(fileName, items_[i].operationType);

            qDebug() << "TransferQueue: Starting operation" << i << "remote:" << items_[i].remotePath
                     << "local:" << items_[i].localPath;

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
    qDebug() << "TransferQueue: processNext - no more pending items";
    processing_ = false;
    currentIndex_ = -1;
    overwriteAll_ = false;  // Reset for next batch
    emit allOperationsCompleted();
}

void TransferQueue::respondToOverwrite(OverwriteResponse response)
{
    if (!waitingForOverwriteResponse_) {
        return;
    }

    waitingForOverwriteResponse_ = false;

    switch (response) {
    case OverwriteResponse::Overwrite:
        // Proceed with just this file
        qDebug() << "TransferQueue: User chose to overwrite this file";
        processNext();
        break;

    case OverwriteResponse::OverwriteAll:
        // Set flag and proceed
        qDebug() << "TransferQueue: User chose to overwrite all files";
        overwriteAll_ = true;
        processNext();
        break;

    case OverwriteResponse::Skip:
        // Skip this file and continue
        qDebug() << "TransferQueue: User chose to skip this file";
        if (currentIndex_ >= 0 && currentIndex_ < items_.size()) {
            items_[currentIndex_].status = TransferItem::Status::Completed;
            items_[currentIndex_].errorMessage = tr("Skipped");
            emit dataChanged(index(currentIndex_), index(currentIndex_));
        }
        currentIndex_ = -1;
        processNext();
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
    for (int i = 0; i < items_.size(); ++i) {
        if (items_[i].localPath == localPath && items_[i].remotePath == remotePath) {
            return i;
        }
    }
    return -1;
}

void TransferQueue::onUploadProgress(const QString &file, qint64 sent, qint64 total)
{
    Q_UNUSED(file)
    if (currentIndex_ >= 0 && currentIndex_ < items_.size()) {
        items_[currentIndex_].bytesTransferred = sent;
        items_[currentIndex_].totalBytes = total;
        emit dataChanged(index(currentIndex_), index(currentIndex_));
    }
}

void TransferQueue::onUploadFinished(const QString &localPath, const QString &remotePath)
{
    int idx = findItemIndex(localPath, remotePath);
    if (idx >= 0) {
        items_[idx].status = TransferItem::Status::Completed;
        items_[idx].bytesTransferred = items_[idx].totalBytes;
        emit dataChanged(index(idx), index(idx));

        QString fileName = QFileInfo(localPath).fileName();
        emit operationCompleted(fileName);
    }

    emit queueChanged();
    processNext();
}

void TransferQueue::onDownloadProgress(const QString &file, qint64 received, qint64 total)
{
    Q_UNUSED(file)
    if (currentIndex_ >= 0 && currentIndex_ < items_.size()) {
        items_[currentIndex_].bytesTransferred = received;
        items_[currentIndex_].totalBytes = total;
        emit dataChanged(index(currentIndex_), index(currentIndex_));
    }
}

void TransferQueue::onDownloadFinished(const QString &remotePath, const QString &localPath)
{
    qDebug() << "TransferQueue: onDownloadFinished remotePath:" << remotePath << "localPath:" << localPath;

    int idx = findItemIndex(localPath, remotePath);
    qDebug() << "TransferQueue: findItemIndex returned:" << idx;

    if (idx >= 0) {
        items_[idx].status = TransferItem::Status::Completed;
        items_[idx].bytesTransferred = items_[idx].totalBytes;
        emit dataChanged(index(idx), index(idx));

        QString fileName = QFileInfo(remotePath).fileName();
        emit operationCompleted(fileName);
    } else {
        // Debug: show what we were looking for vs what's in the queue
        qDebug() << "TransferQueue: ERROR - item not found! Queue contents:";
        for (int i = 0; i < items_.size(); ++i) {
            qDebug() << "  Item" << i << "remote:" << items_[i].remotePath << "local:" << items_[i].localPath;
        }
    }

    emit queueChanged();
    processNext();
}

void TransferQueue::onFtpError(const QString &message)
{
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
    }

    emit queueChanged();
    processNext();
}

void TransferQueue::onDirectoryCreated(const QString &path)
{
    if (!creatingDirectory_) return;

    creatingDirectory_ = false;

    // Check if this was for our recursive upload
    if (!pendingMkdirs_.isEmpty()) {
        PendingMkdir completed = pendingMkdirs_.dequeue();

        // If this was the last directory, queue all files for upload
        if (pendingMkdirs_.isEmpty()) {
            // Find the original local directory (first one created was the root)
            processRecursiveUpload(completed.localDir, completed.remoteBase);
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

            // Queue subdirectory for scanning
            PendingScan subScan;
            subScan.remotePath = entryRemotePath;
            subScan.localBasePath = currentScan.localBasePath;  // Keep the same base
            pendingScans_.enqueue(subScan);

            // Track that we'll request this listing
            requestedListings_.insert(entryRemotePath);
            qDebug() << "TransferQueue: Queued subdir scan:" << entryRemotePath;
        } else {
            // Queue file for download
            QString localFilePath = localTargetDir + '/' + entry.name;
            qDebug() << "TransferQueue: Queuing download:" << entryRemotePath << "->" << localFilePath;
            enqueueDownload(entryRemotePath, localFilePath);
        }
    }

    // Continue scanning if there are more directories
    if (!pendingScans_.isEmpty()) {
        PendingScan next = pendingScans_.head();
        qDebug() << "TransferQueue: Next scan:" << next.remotePath;
        ftpClient_->list(next.remotePath);
    } else {
        qDebug() << "TransferQueue: All scans complete, starting transfers";
        // All directories scanned - now start processing the download queue
        scanningDirectories_ = false;
        processNext();
    }
}

void TransferQueue::enqueueDelete(const QString &remotePath, bool isDirectory)
{
    TransferItem item;
    item.remotePath = remotePath;
    item.operationType = OperationType::Delete;
    item.status = TransferItem::Status::Pending;
    item.isDirectory = isDirectory;

    beginInsertRows(QModelIndex(), items_.size(), items_.size());
    items_.append(item);
    endInsertRows();

    emit queueChanged();

    if (!processing_ && !processingDelete_) {
        processNext();
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

    // Clear any previous delete state
    deleteQueue_.clear();
    currentDeleteIndex_ = 0;
    totalDeleteItems_ = 0;
    deletedCount_ = 0;

    // Queue the initial directory scan
    PendingDeleteScan scan;
    scan.remotePath = normalizedPath;
    pendingDeleteScans_.enqueue(scan);

    // Track that we're requesting this listing
    requestedDeleteListings_.insert(normalizedPath);
    qDebug() << "TransferQueue: Requesting delete listing for:" << normalizedPath;

    // Signal that scanning has started
    emit operationStarted(QFileInfo(normalizedPath).fileName(), OperationType::Delete);
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
            qDebug() << "TransferQueue: Queued file for delete:" << entryPath;
        }
    }

    // Add the directory itself to the queue AFTER its contents (depth-first)
    DeleteItem dirItem;
    dirItem.path = path;
    dirItem.isDirectory = true;
    deleteQueue_.append(dirItem);
    qDebug() << "TransferQueue: Queued directory for delete:" << path;

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
        return;
    }

    if (currentDeleteIndex_ >= deleteQueue_.size()) {
        qDebug() << "TransferQueue: All deletes complete";
        processingDelete_ = false;
        deleteQueue_.clear();
        emit operationCompleted(tr("Deleted %1 items").arg(deletedCount_));
        emit allOperationsCompleted();
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
    if (processingDelete_ && currentDeleteIndex_ < deleteQueue_.size()) {
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
            items_[i].status = TransferItem::Status::Completed;
            emit dataChanged(index(i), index(i));

            QString fileName = QFileInfo(path).fileName();
            emit operationCompleted(fileName);

            emit queueChanged();
            processNext();
            return;
        }
    }
}
