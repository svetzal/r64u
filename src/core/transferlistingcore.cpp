/**
 * @file transferlistingcore.cpp
 * @brief Implementation of directory listing processing pure functions.
 */

#include "transferlistingcore.h"

#include <QFileInfo>

#include <algorithm>

namespace transfer {

QList<DeleteItem> sortDeleteQueue(const QList<DeleteItem> &queue)
{
    QList<DeleteItem> result = queue;
    std::sort(result.begin(), result.end(), [](const DeleteItem &a, const DeleteItem &b) {
        if (!a.isDirectory && b.isDirectory)
            return true;
        if (a.isDirectory && !b.isDirectory)
            return false;
        if (a.isDirectory && b.isDirectory) {
            return a.path.count('/') > b.path.count('/');
        }
        return false;
    });
    return result;
}

DirectoryListingResult processDirectoryListingForDownload(const PendingScan &currentScan,
                                                          const QList<FtpEntry> &entries)
{
    DirectoryListingResult result;
    result.directoriesScanned = 1;

    // Calculate local directory for this path
    QString localTargetDir;
    if (currentScan.remotePath == currentScan.remoteBasePath) {
        localTargetDir = currentScan.localBasePath;
    } else {
        QString relativePath = currentScan.remotePath.mid(currentScan.remoteBasePath.length());
        if (relativePath.startsWith('/'))
            relativePath = relativePath.mid(1);
        localTargetDir = currentScan.localBasePath + '/' + relativePath;
    }

    for (const FtpEntry &entry : entries) {
        QString entryRemotePath = currentScan.remotePath;
        if (!entryRemotePath.endsWith('/'))
            entryRemotePath += '/';
        entryRemotePath += entry.name;

        if (entry.isDirectory) {
            PendingScan subScan;
            subScan.remotePath = entryRemotePath;
            subScan.localBasePath = currentScan.localBasePath;
            subScan.remoteBasePath = currentScan.remoteBasePath;
            subScan.batchId = currentScan.batchId;
            result.newSubScans.append(subScan);
        } else {
            QString localFilePath = localTargetDir + '/' + entry.name;
            result.newFileDownloads.append({entryRemotePath, localFilePath});
        }
    }

    return result;
}

DeleteListingResult processDirectoryListingForDelete(const QString &path,
                                                     const QList<FtpEntry> &entries)
{
    DeleteListingResult result;

    for (const FtpEntry &entry : entries) {
        QString entryPath = path;
        if (!entryPath.endsWith('/'))
            entryPath += '/';
        entryPath += entry.name;

        if (entry.isDirectory) {
            PendingScan subScan;
            subScan.remotePath = entryPath;
            result.newSubScans.append(subScan);
        } else {
            DeleteItem item;
            item.path = entryPath;
            item.isDirectory = false;
            result.fileItems.append(item);
        }
    }

    // The directory itself is deleted after its contents
    result.directoryItem.path = path;
    result.directoryItem.isDirectory = true;

    return result;
}

State updateFolderExistence(const State &state, const QString &parentPath,
                            const QList<FtpEntry> &entries)
{
    State result = state;

    QSet<QString> existingDirs;
    for (const FtpEntry &entry : entries) {
        if (entry.isDirectory) {
            existingDirs.insert(entry.name);
        }
    }

    QQueue<PendingFolderOp> updatedOps;
    while (!result.pendingFolderOps.isEmpty()) {
        PendingFolderOp op = result.pendingFolderOps.dequeue();
        if (op.destPath == parentPath) {
            QString targetFolderName = QFileInfo(op.targetPath).fileName();
            op.destExists = existingDirs.contains(targetFolderName);
        }
        updatedOps.enqueue(op);
    }
    result.pendingFolderOps = updatedOps;

    return result;
}

UploadFileCheckResult checkUploadFileExists(const State &state, const QList<FtpEntry> &entries)
{
    UploadFileCheckResult result;
    result.newState = state;

    if (state.currentIndex < 0 || state.currentIndex >= state.items.size()) {
        return result;
    }

    const QString targetFileName = QFileInfo(state.items[state.currentIndex].remotePath).fileName();
    result.fileName = targetFileName;

    result.fileExists = std::any_of(entries.begin(), entries.end(), [&](const FtpEntry &entry) {
        return !entry.isDirectory && entry.name == targetFileName;
    });

    if (result.fileExists) {
        result.newState.queueState = QueueState::AwaitingFileConfirm;
        result.newState.pendingConfirmation.itemIndex = state.currentIndex;
        result.newState.pendingConfirmation.opType = OperationType::Upload;
    } else {
        result.newState.items[state.currentIndex].confirmed = true;
    }

    return result;
}

}  // namespace transfer
