#include "transferqueue.h"
#include "../services/c64uftpclient.h"

#include <QFileInfo>

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

void TransferQueue::clear()
{
    if (items_.isEmpty()) return;

    beginResetModel();
    items_.clear();
    endResetModel();

    processing_ = false;
    currentIndex_ = -1;
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
