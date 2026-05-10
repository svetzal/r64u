#include "remotefileoperationsservice.h"

#include "iftpclient.h"

#include "utils/logging.h"

RemoteFileOperationsService::RemoteFileOperationsService(IFtpClient *ftpClient, QObject *parent)
    : QObject(parent), ftpClient_(ftpClient)
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

void RemoteFileOperationsService::createFolder(const QString &path)
{
    if (!ftpClient_) {
        qCWarning(LogFileOps) << "createFolder skipped: FTP client not configured";
        emit operationFailed(tr("Create folder"), tr("FTP client not configured"));
        return;
    }
    ftpClient_->makeDirectory(path);
}

void RemoteFileOperationsService::renameItem(const QString &oldPath, const QString &newPath)
{
    if (!ftpClient_) {
        qCWarning(LogFileOps) << "renameItem skipped: FTP client not configured";
        emit operationFailed(tr("Rename"), tr("FTP client not configured"));
        return;
    }
    ftpClient_->rename(oldPath, newPath);
}
