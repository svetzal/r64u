#include "c64uftpclient.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>

C64UFtpClient::C64UFtpClient(QObject *parent)
    : QObject(parent)
{
}

void C64UFtpClient::setHost(const QString &host, quint16 port)
{
    Q_UNUSED(port)
    host_ = host;
}

void C64UFtpClient::setCredentials(const QString &user, const QString &password)
{
    Q_UNUSED(user)
    Q_UNUSED(password)
}

void C64UFtpClient::connectToHost()
{
    // In mock, use mockSetConnected() to control this
}

void C64UFtpClient::disconnect()
{
    connected_ = false;
    state_ = State::Disconnected;
    emit disconnected();
}

void C64UFtpClient::list(const QString &path)
{
    listRequests_.append(path);

    PendingOp op;
    op.type = PendingOp::List;
    op.path = path;
    pendingOps_.enqueue(op);
}

void C64UFtpClient::changeDirectory(const QString &path)
{
    currentDir_ = path;
    emit directoryChanged(path);
}

void C64UFtpClient::makeDirectory(const QString &path)
{
    mkdirRequests_.append(path);

    PendingOp op;
    op.type = PendingOp::Mkdir;
    op.path = path;
    pendingOps_.enqueue(op);
}

void C64UFtpClient::removeDirectory(const QString &path)
{
    Q_UNUSED(path)
}

void C64UFtpClient::download(const QString &remotePath, const QString &localPath)
{
    downloadRequests_.append(remotePath);

    PendingOp op;
    op.type = PendingOp::Download;
    op.path = remotePath;
    op.localPath = localPath;
    pendingOps_.enqueue(op);
}

void C64UFtpClient::downloadToMemory(const QString &remotePath)
{
    PendingOp op;
    op.type = PendingOp::DownloadToMemory;
    op.path = remotePath;
    pendingOps_.enqueue(op);
}

void C64UFtpClient::upload(const QString &localPath, const QString &remotePath)
{
    uploadRequests_.append(localPath);

    PendingOp op;
    op.type = PendingOp::Upload;
    op.path = remotePath;
    op.localPath = localPath;
    pendingOps_.enqueue(op);
}

void C64UFtpClient::remove(const QString &path)
{
    emit fileRemoved(path);
}

void C64UFtpClient::rename(const QString &oldPath, const QString &newPath)
{
    emit fileRenamed(oldPath, newPath);
}

void C64UFtpClient::abort()
{
    pendingOps_.clear();
}

// === Mock control methods ===

void C64UFtpClient::mockSetConnected(bool connected)
{
    connected_ = connected;
    state_ = connected ? State::Ready : State::Disconnected;
    if (connected) {
        emit this->connected();
    } else {
        emit disconnected();
    }
}

void C64UFtpClient::mockSetDirectoryListing(const QString &path, const QList<FtpEntry> &entries)
{
    mockListings_[path] = entries;
}

void C64UFtpClient::mockSetDownloadData(const QString &remotePath, const QByteArray &data)
{
    mockDownloadData_[remotePath] = data;
}

void C64UFtpClient::mockSetNextOperationFails(const QString &errorMessage)
{
    nextOpFails_ = true;
    nextOpError_ = errorMessage;
}

void C64UFtpClient::mockProcessNextOperation()
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
    }
}

void C64UFtpClient::mockProcessAllOperations()
{
    while (!pendingOps_.isEmpty()) {
        mockProcessNextOperation();
    }
}

void C64UFtpClient::mockReset()
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
    nextOpFails_ = false;
    nextOpError_.clear();
}
