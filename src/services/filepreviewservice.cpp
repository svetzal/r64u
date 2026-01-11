#include "filepreviewservice.h"
#include "iftpclient.h"

FilePreviewService::FilePreviewService(IFtpClient *ftpClient, QObject *parent)
    : QObject(parent)
    , ftpClient_(ftpClient)
{
    connect(ftpClient_, &IFtpClient::downloadToMemoryFinished,
            this, &FilePreviewService::onDownloadToMemoryFinished);
    connect(ftpClient_, &IFtpClient::error,
            this, &FilePreviewService::onFtpError);
}

FilePreviewService::~FilePreviewService()
{
    // Disconnect from FTP client BEFORE this object is destroyed to prevent
    // signals from being delivered to slots after member variables are destroyed.
    if (ftpClient_) {
        disconnect(ftpClient_, nullptr, this, nullptr);
    }
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
    if (!pendingPath_.isEmpty()) {
        pendingPath_.clear();
        if (ftpClient_) {
            ftpClient_->abort();
        }
    }
}

void FilePreviewService::onDownloadToMemoryFinished(const QString &remotePath, const QByteArray &data)
{
    if (remotePath == pendingPath_) {
        pendingPath_.clear();
        emit previewReady(remotePath, data);
    }
}

void FilePreviewService::onFtpError(const QString &message)
{
    if (!pendingPath_.isEmpty()) {
        QString path = pendingPath_;
        pendingPath_.clear();
        emit previewFailed(path, message);
    }
}
