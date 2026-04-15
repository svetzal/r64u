#include "remotefileoperations.h"

#include "iftpclient.h"

RemoteFileOperations::RemoteFileOperations(IFtpClient *ftpClient, QObject *parent)
    : QObject(parent), ftpClient_(ftpClient)
{
    if (ftpClient_) {
        connect(ftpClient_, &IFtpClient::directoryCreated, this,
                &RemoteFileOperations::folderCreated);
        connect(ftpClient_, &IFtpClient::fileRenamed, this, &RemoteFileOperations::itemRenamed);
        connect(ftpClient_, &IFtpClient::fileRemoved, this, &RemoteFileOperations::itemRemoved);
    }
}

void RemoteFileOperations::createFolder(const QString &path)
{
    if (!ftpClient_) {
        return;
    }
    ftpClient_->makeDirectory(path);
}

void RemoteFileOperations::renameItem(const QString &oldPath, const QString &newPath)
{
    if (!ftpClient_) {
        return;
    }
    ftpClient_->rename(oldPath, newPath);
}
