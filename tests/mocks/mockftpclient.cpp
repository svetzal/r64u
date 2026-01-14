#include "mockftpclient.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>

MockFtpClient::MockFtpClient(QObject *parent)
    : IFtpClient(parent)
{
}

void MockFtpClient::setHost(const QString &host, quint16 port)
{
    Q_UNUSED(port)
    host_ = host;
}

void MockFtpClient::setCredentials(const QString &user, const QString &password)
{
    Q_UNUSED(user)
    Q_UNUSED(password)
}

void MockFtpClient::connectToHost()
{
    // In mock, use mockSetConnected() to control this
}

void MockFtpClient::disconnect()
{
    connected_ = false;
    state_ = State::Disconnected;
    emit disconnected();
}

void MockFtpClient::list(const QString &path)
{
    listRequests_.append(path);

    PendingOp op;
    op.type = PendingOp::List;
    op.path = path;
    pendingOps_.enqueue(op);
}

void MockFtpClient::changeDirectory(const QString &path)
{
    currentDir_ = path;
    emit directoryChanged(path);
}

void MockFtpClient::makeDirectory(const QString &path)
{
    mkdirRequests_.append(path);

    PendingOp op;
    op.type = PendingOp::Mkdir;
    op.path = path;
    pendingOps_.enqueue(op);
}

void MockFtpClient::removeDirectory(const QString &path)
{
    // Track for test assertions (reuses deleteRequests_ since it's the same semantic operation)
    deleteRequests_.append(path);

    PendingOp op;
    op.type = PendingOp::RemoveDir;
    op.path = path;
    pendingOps_.enqueue(op);
}

void MockFtpClient::download(const QString &remotePath, const QString &localPath)
{
    downloadRequests_.append(remotePath);

    PendingOp op;
    op.type = PendingOp::Download;
    op.path = remotePath;
    op.localPath = localPath;
    pendingOps_.enqueue(op);
}

void MockFtpClient::downloadToMemory(const QString &remotePath)
{
    PendingOp op;
    op.type = PendingOp::DownloadToMemory;
    op.path = remotePath;
    pendingOps_.enqueue(op);
}

void MockFtpClient::upload(const QString &localPath, const QString &remotePath)
{
    uploadRequests_.append(localPath);

    PendingOp op;
    op.type = PendingOp::Upload;
    op.path = remotePath;
    op.localPath = localPath;
    pendingOps_.enqueue(op);
}

void MockFtpClient::remove(const QString &path)
{
    deleteRequests_.append(path);

    PendingOp op;
    op.type = PendingOp::Delete;
    op.path = path;
    pendingOps_.enqueue(op);
}

void MockFtpClient::rename(const QString &oldPath, const QString &newPath)
{
    PendingOp op;
    op.type = PendingOp::Rename;
    op.path = oldPath;
    op.newPath = newPath;
    pendingOps_.enqueue(op);
}

void MockFtpClient::abort()
{
    pendingOps_.clear();
}

// === Mock control methods ===

void MockFtpClient::mockSetConnected(bool connected)
{
    connected_ = connected;
    state_ = connected ? State::Ready : State::Disconnected;
    if (connected) {
        emit this->connected();
    } else {
        emit disconnected();
    }
}

void MockFtpClient::mockSetDirectoryListing(const QString &path, const QList<FtpEntry> &entries)
{
    mockListings_[path] = entries;
}

void MockFtpClient::mockSetDownloadData(const QString &remotePath, const QByteArray &data)
{
    mockDownloadData_[remotePath] = data;
}

void MockFtpClient::mockSetNextOperationFails(const QString &errorMessage)
{
    nextOpFails_ = true;
    nextOpError_ = errorMessage;
}

void MockFtpClient::mockProcessNextOperation()
{
    if (pendingOps_.isEmpty()) {
        return;
    }

    PendingOp op = pendingOps_.dequeue();

    // Check if this operation should fail
    if (nextOpFails_) {
        nextOpFails_ = false;
        emit error(nextOpError_);
        return;
    }

    switch (op.type) {
    case PendingOp::List: {
        QList<FtpEntry> entries = mockListings_.value(op.path);
        emit directoryListed(op.path, entries);
        break;
    }
    case PendingOp::Download: {
        QByteArray data = mockDownloadData_.value(op.path);
        // Write to local file
        QDir().mkpath(QFileInfo(op.localPath).absolutePath());
        QFile file(op.localPath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(data);
            file.close();
        }
        emit downloadProgress(op.path, data.size(), data.size());
        emit downloadFinished(op.path, op.localPath);
        break;
    }
    case PendingOp::DownloadToMemory: {
        QByteArray data = mockDownloadData_.value(op.path);
        emit downloadToMemoryFinished(op.path, data);
        break;
    }
    case PendingOp::Upload: {
        QFile file(op.localPath);
        qint64 size = file.exists() ? file.size() : 0;
        emit uploadProgress(op.localPath, size, size);
        emit uploadFinished(op.localPath, op.path);
        break;
    }
    case PendingOp::Mkdir: {
        emit directoryCreated(op.path);
        break;
    }
    case PendingOp::Delete: {
        emit fileRemoved(op.path);
        break;
    }
    case PendingOp::RemoveDir: {
        emit fileRemoved(op.path);  // Same signal as remove - directory was removed
        break;
    }
    case PendingOp::Rename: {
        emit fileRenamed(op.path, op.newPath);
        break;
    }
    }
}

void MockFtpClient::mockProcessAllOperations()
{
    while (!pendingOps_.isEmpty()) {
        mockProcessNextOperation();
    }
}

void MockFtpClient::mockSimulateConnect()
{
    connected_ = true;
    state_ = State::Ready;
    emit connected();
}

void MockFtpClient::mockSimulateDisconnect()
{
    connected_ = false;
    state_ = State::Disconnected;
    emit disconnected();
}

void MockFtpClient::mockReset()
{
    connected_ = false;
    state_ = State::Disconnected;
    pendingOps_.clear();
    mockListings_.clear();
    mockDownloadData_.clear();
    listRequests_.clear();
    downloadRequests_.clear();
    mkdirRequests_.clear();
    uploadRequests_.clear();
    deleteRequests_.clear();
    nextOpFails_ = false;
    nextOpError_.clear();
}
