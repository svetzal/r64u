#include "remotefileoperationsservice.h"

#include "iftpclient.h"

#include "utils/logging.h"

RemoteFileOperationsService::RemoteFileOperationsService(IFtpClient *ftpClient, QObject *parent)
    : IErrorEmitter(parent), ftpClient_(ftpClient)
{
    if (ftpClient_) {
        connect(ftpClient_, &IFtpClient::directoryCreated, this,
                &RemoteFileOperationsService::folderCreated);
        connect(ftpClient_, &IFtpClient::fileRenamed, this,
                &RemoteFileOperationsService::itemRenamed);
        connect(ftpClient_, &IFtpClient::fileRemoved, this,
                &RemoteFileOperationsService::itemRemoved);
    }
}

bool RemoteFileOperationsService::ensureFtpClient(const QString &operationLabel)
{
    if (!ftpClient_) {
        qCWarning(LogFileOps) << operationLabel << "skipped: FTP client not configured";
        emit operationFailed(operationLabel, tr("FTP client not configured"));
        emit errorReported(ErrorCategory::FileOperation, ErrorSeverity::Warning,
                           tr("%1 failed").arg(operationLabel), tr("FTP client not configured"));
        return false;
    }
    return true;
}

void RemoteFileOperationsService::createFolder(const QString &path)
{
    if (!ensureFtpClient(tr("Create folder")))
        return;
    ftpClient_->makeDirectory(path);
}

void RemoteFileOperationsService::renameItem(const QString &oldPath, const QString &newPath)
{
    if (!ensureFtpClient(tr("Rename")))
        return;
    ftpClient_->rename(oldPath, newPath);
}
