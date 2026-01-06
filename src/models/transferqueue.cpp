#include "transferqueue.h"
#include "../services/c64uftpclient.h"

#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QDebug>

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
    }
}

void TransferQueue::enqueueUpload(const QString &localPath, const QString &remotePath)
{
    TransferItem item;
    item.localPath = localPath;
    item.remotePath = remotePath;
    item.direction = TransferItem::Direction::Upload;
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
    item.direction = TransferItem::Direction::Download;
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
    if (ftpClient_ && processing_) {
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

    emit dataChanged(index(0), index(items_.size() - 1));
    emit queueChanged();
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
        QString path = (item.direction == TransferItem::Direction::Upload)
            ? item.localPath : item.remotePath;
        return QFileInfo(path).fileName();
    }
    case LocalPathRole:
        return item.localPath;
    case RemotePathRole:
        return item.remotePath;
    case DirectionRole:
        return static_cast<int>(item.direction);
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
    roles[DirectionRole] = "direction";
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
    if (!ftpClient_ || !ftpClient_->isConnected()) {
        processing_ = false;
        return;
    }

    // Don't start transfers while we're still scanning directories
    if (scanningDirectories_) {
        qDebug() << "TransferQueue: processNext - waiting for directory scanning to complete";
        return;
    }

    // Find next pending item
    for (int i = 0; i < items_.size(); ++i) {
        if (items_[i].status == TransferItem::Status::Pending) {
            currentIndex_ = i;
            items_[i].status = TransferItem::Status::InProgress;
            processing_ = true;

            emit dataChanged(index(i), index(i));

            QString fileName = QFileInfo(
                items_[i].direction == TransferItem::Direction::Upload
                    ? items_[i].localPath : items_[i].remotePath
            ).fileName();
            emit transferStarted(fileName);

            if (items_[i].direction == TransferItem::Direction::Upload) {
                ftpClient_->upload(items_[i].localPath, items_[i].remotePath);
            } else {
                ftpClient_->download(items_[i].remotePath, items_[i].localPath);
            }
            return;
        }
    }

    // No more pending items
    processing_ = false;
    currentIndex_ = -1;
    emit allTransfersCompleted();
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
        emit transferCompleted(fileName);
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
    int idx = findItemIndex(localPath, remotePath);
    if (idx >= 0) {
        items_[idx].status = TransferItem::Status::Completed;
        items_[idx].bytesTransferred = items_[idx].totalBytes;
        emit dataChanged(index(idx), index(idx));

        QString fileName = QFileInfo(remotePath).fileName();
        emit transferCompleted(fileName);
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
            items_[currentIndex_].direction == TransferItem::Direction::Upload
                ? items_[currentIndex_].localPath
                : items_[currentIndex_].remotePath
        ).fileName();
        emit transferFailed(fileName, message);
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
    qDebug() << "TransferQueue: requestedListings_:" << requestedListings_;

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
