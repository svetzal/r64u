#include "remotefileoperationsservice.h"

#include "iftpclient.h"

#include "utils/logging.h"

RemoteFileOperationsService::RemoteFileOperationsService(IFtpClient *ftpClient, QObject *parent)
    : IErrorEmitter(parent), ftpClient_(ftpClient)
{
    if (ftpClient_) {
        connect(ftpClient_, &IFtpClient::directoryCreated, this, [this](const QString &path) {
            pendingFolders_.remove(path);
            emit folderCreated(path);
        });
        connect(ftpClient_, &IFtpClient::fileRenamed, this,
                [this](const QString &oldPath, const QString &newPath) {
                    pendingRenames_.remove(oldPath);
                    emit itemRenamed(oldPath, newPath);
                });
        connect(ftpClient_, &IFtpClient::fileRemoved, this,
                &RemoteFileOperationsService::itemRemoved);
        connect(ftpClient_, &IFtpClient::operationFailed, this,
                &RemoteFileOperationsService::onFtpOperationFailed);
        connect(ftpClient_, &IFtpClient::disconnected, this, [this]() {
            // The client drops its requests with the connection, and reports that itself
            pendingFolders_.clear();
            pendingRenames_.clear();
        });
    }
}

bool RemoteFileOperationsService::ensureFtpClient(const QString &operationLabel)
{
    if (!ftpClient_) {
        qCWarning(LogFileOps) << operationLabel << "skipped: FTP client not configured";
        reportFailure(operationLabel, tr("FTP client not configured"));
        return false;
    }
    return true;
}

void RemoteFileOperationsService::reportFailure(const QString &operationLabel,
                                                const QString &message)
{
    emit operationFailed(operationLabel, message);
    emit errorReported(ErrorCategory::FileOperation, ErrorSeverity::Warning,
                       tr("%1 failed").arg(operationLabel), message);
}

void RemoteFileOperationsService::createFolder(const QString &path)
{
    if (!ensureFtpClient(tr("Create folder")))
        return;
    pendingFolders_.insert(path);
    ftpClient_->makeDirectory(path);
}

void RemoteFileOperationsService::renameItem(const QString &oldPath, const QString &newPath)
{
    if (!ensureFtpClient(tr("Rename")))
        return;
    pendingRenames_.insert(oldPath);
    ftpClient_->rename(oldPath, newPath);
}

void RemoteFileOperationsService::onFtpOperationFailed(IFtpClient::Operation operation,
                                                       const QString &remotePath,
                                                       const QString & /*localPath*/,
                                                       const QString &message)
{
    // Only this service's own requests: the client is shared (e.g. with transfers)
    if (operation == IFtpClient::Operation::MakeDirectory && pendingFolders_.remove(remotePath)) {
        reportFailure(tr("Create folder"), message);
    } else if (operation == IFtpClient::Operation::Rename && pendingRenames_.remove(remotePath)) {
        reportFailure(tr("Rename"), message);
    }
}
