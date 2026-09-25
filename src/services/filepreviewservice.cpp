#include "filepreviewservice.h"

#include "iftpclient.h"

#include "core/ftpclientmixin.h"

FilePreviewService::FilePreviewService(IFtpClient *ftpClient, QObject *parent)
    : IErrorEmitter(parent), ftpClient_(ftpClient)
{
    connect(ftpClient_, &IFtpClient::downloadToMemoryFinished, this,
            &FilePreviewService::onDownloadToMemoryFinished);
    connect(ftpClient_, &IFtpClient::operationFailed, this,
            &FilePreviewService::onFtpOperationFailed);
    connect(ftpClient_, &IFtpClient::error, this, &FilePreviewService::onFtpError);
    connect(ftpClient_, &IFtpClient::disconnected, this, &FilePreviewService::onFtpDisconnected);

    // Forward previewFailed to the uniform IErrorEmitter signal
    connect(this, &FilePreviewService::previewFailed, this,
            [this](const QString &path, const QString &error) {
                emit errorReported(ErrorCategory::FileOperation, ErrorSeverity::Warning,
                                   tr("Preview of %1").arg(path), error);
            });
}

FilePreviewService::~FilePreviewService()
{
    // Disconnect from FTP client BEFORE this object is destroyed to prevent
    // signals from being delivered to slots after member variables are destroyed.
    disconnectFtpClient(ftpClient_, this);
}

void FilePreviewService::requestPreview(const QString &remotePath)
{
    if (!ftpClient_) {
        emit previewFailed(remotePath, tr("No FTP client available"));
        return;
    }

    if (!ftpClient_->isConnected()) {
        emit previewFailed(remotePath, tr("Not connected"));
        return;
    }

    pendingPath_ = remotePath;
    emit previewStarted(remotePath);
    ftpClient_->downloadToMemory(remotePath);
}

void FilePreviewService::cancelRequest()
{
    if (pendingPath_.isEmpty()) {
        return;
    }
    const QString path = pendingPath_;
    pendingPath_.clear();
    if (ftpClient_) {
        // The client is shared: never abort another component's transfer
        ftpClient_->abortIfInFlight(IFtpClient::Operation::DownloadToMemory, path);
    }
}

void FilePreviewService::onDownloadToMemoryFinished(const QString &remotePath,
                                                    const QByteArray &data)
{
    if (remotePath == pendingPath_) {
        pendingPath_.clear();
        emit previewReady(remotePath, data);
    }
}

void FilePreviewService::onFtpOperationFailed(IFtpClient::Operation operation,
                                              const QString &remotePath,
                                              const QString & /*localPath*/, const QString &message)
{
    if (operation != IFtpClient::Operation::DownloadToMemory || remotePath != pendingPath_) {
        return;  // Another component's request on the shared client
    }
    failPendingPreview(message);
}

void FilePreviewService::onFtpError(const QString &message)
{
    // Failures of single requests are matched through operationFailed(); an error
    // that leaves the client disconnected (socket error, timeout) ends the request
    if (ftpClient_ && ftpClient_->isConnected()) {
        return;
    }
    failPendingPreview(message);
}

void FilePreviewService::onFtpDisconnected()
{
    failPendingPreview(tr("Connection to the device was lost"));
}

void FilePreviewService::failPendingPreview(const QString &message)
{
    if (pendingPath_.isEmpty()) {
        return;
    }
    const QString path = pendingPath_;
    pendingPath_.clear();
    emit previewFailed(path, message);
}
