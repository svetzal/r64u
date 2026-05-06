#include "localfileoperationsservice.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

LocalFileOperationsService::LocalFileOperationsService(QObject *parent) : QObject(parent) {}

void LocalFileOperationsService::createFolder(const QString &parentDir, const QString &name)
{
    QString newPath = parentDir;
    if (!newPath.endsWith('/')) {
        newPath += '/';
    }
    newPath += name;

    QDir dir;
    if (dir.mkdir(newPath)) {
        emit folderCreated(newPath);
        emit statusMessage(tr("Local folder created: %1").arg(name));
    } else {
        emit operationFailed(ErrorCategory::FileOperation,
                             tr("Failed to create folder: %1").arg(newPath));
    }
}

void LocalFileOperationsService::renameItem(const QString &oldPath, const QString &newName)
{
    QFileInfo fileInfo(oldPath);
    const QString oldName = fileInfo.fileName();
    const QString itemType = fileInfo.isDir() ? tr("folder") : tr("file");
    const QString newPath = fileInfo.absolutePath() + "/" + newName;

    QDir dir;
    bool success = dir.rename(oldPath, newPath);

    if (success) {
        emit itemRenamed(oldPath, newPath);
        emit statusMessage(tr("Renamed: %1 -> %2").arg(oldName, newName));
    } else {
        QString errorMessage;
        if (!fileInfo.exists()) {
            errorMessage = tr("The %1 no longer exists.").arg(itemType);
        } else if (!fileInfo.isWritable()) {
            errorMessage =
                tr("Permission denied. You don't have permission to rename this %1.").arg(itemType);
        } else {
            errorMessage =
                tr("Failed to rename the %1. Please check that you have the necessary permissions.")
                    .arg(itemType);
        }
        emit operationFailed(ErrorCategory::FileOperation, errorMessage);
        emit statusMessage(tr("Failed to rename: %1").arg(oldName));
    }
}

void LocalFileOperationsService::deleteItems(const QStringList &paths)
{
    int successCount = 0;
    int failCount = 0;

    for (const QString &localPath : paths) {
        QString pathInTrash;
        bool success = QFile::moveToTrash(localPath, &pathInTrash);

        if (success) {
            successCount++;
        } else {
            failCount++;
            QFileInfo fileInfo(localPath);
            const QString itemType = fileInfo.isDir() ? tr("folder") : tr("file");
            QString errorMessage;
            if (!fileInfo.exists()) {
                errorMessage = tr("The %1 no longer exists.").arg(itemType);
            } else if (!fileInfo.isWritable()) {
                errorMessage = tr("Permission denied. You don't have permission to delete this %1.")
                                   .arg(itemType);
            } else {
                errorMessage = tr("Failed to move the %1 to trash. The system may not support "
                                  "trash functionality.")
                                   .arg(itemType);
            }
            emit operationFailed(ErrorCategory::FileOperation, errorMessage);
        }
    }

    if (successCount > 0) {
        emit itemsDeleted(successCount);
    }

    if (paths.size() == 1) {
        if (successCount > 0) {
            emit statusMessage(tr("Moved to trash: %1").arg(QFileInfo(paths.first()).fileName()));
        } else {
            emit statusMessage(tr("Failed to delete: %1").arg(QFileInfo(paths.first()).fileName()));
        }
    } else {
        if (failCount == 0) {
            emit statusMessage(tr("Moved %1 items to trash").arg(successCount));
        } else {
            emit statusMessage(
                tr("Moved %1 items to trash, %2 failed").arg(successCount).arg(failCount));
        }
    }
}
