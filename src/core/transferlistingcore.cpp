/**
 * @file transferlistingcore.cpp
 * @brief Implementation of directory listing processing pure functions.
 */

#include "transferlistingcore.h"

#include <QFileInfo>

#include <algorithm>

namespace transfer {

namespace {

/// The device's storage is FAT, where names differing only in case are the same entry.
bool isSameDeviceName(const QString &a, const QString &b)
{
    return QString::compare(a, b, Qt::CaseInsensitive) == 0;
}

}  // namespace

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

QString localDirectoryForScan(const PendingScan &scan)
{
    if (scan.remotePath == scan.remoteBasePath) {
        return scan.localBasePath;
    }
    QString relativePath = scan.remotePath.mid(scan.remoteBasePath.length());
    if (relativePath.startsWith('/'))
        relativePath = relativePath.mid(1);
    return scan.localBasePath + '/' + relativePath;
}

DirectoryListingResult processDirectoryListingForDownload(const PendingScan &currentScan,
                                                          const QList<FtpEntry> &entries)
{
    DirectoryListingResult result;
    result.directoriesScanned = 1;

    const QString localTargetDir = localDirectoryForScan(currentScan);

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
            result.newFileDownloads.append({entryRemotePath, localFilePath, entry.size});
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

    for (PendingFolderOp &op : result.pendingFolderOps) {
        if (!op.confirmed && op.destPath == parentPath) {
            const QString targetFolderName = QFileInfo(op.targetPath).fileName();
            op.destExists = std::any_of(entries.begin(), entries.end(), [&](const FtpEntry &entry) {
                return entry.isDirectory && isSameDeviceName(entry.name, targetFolderName);
            });
        }
    }

    return result;
}

UploadFileCheckResult checkUploadFileExists(const State &state, const QList<FtpEntry> &entries)
{
    UploadFileCheckResult result;
    result.newState = state;

    if (state.queueState != QueueState::CheckingUploadTarget || state.currentIndex < 0 ||
        state.currentIndex >= state.items.size()) {
        return result;
    }

    const QString targetFileName = QFileInfo(state.items[state.currentIndex].remotePath).fileName();
    result.fileName = targetFileName;

    result.fileExists = std::any_of(entries.begin(), entries.end(), [&](const FtpEntry &entry) {
        return !entry.isDirectory && isSameDeviceName(entry.name, targetFileName);
    });

    if (result.fileExists) {
        result.newState.queueState = QueueState::AwaitingFileConfirm;
        result.newState.pendingConfirmation.itemIndex = state.currentIndex;
        result.newState.pendingConfirmation.opType = OperationType::Upload;
    } else {
        result.newState.items[state.currentIndex].confirmed = true;
        result.newState.queueState = QueueState::Idle;
    }

    return result;
}

State abandonFolderCheck(const State &state)
{
    State result = state;
    if (result.queueState != QueueState::CollectingItems) {
        return result;
    }
    // The folder operations stay queued and are checked again when the queue resumes
    result.requestedFolderCheckListings.clear();
    result.queueState = QueueState::Idle;
    return result;
}

State abandonUploadCheck(const State &state)
{
    State result = state;
    if (result.queueState != QueueState::CheckingUploadTarget) {
        return result;
    }
    // The item stays Pending and is checked again when the queue resumes
    result.requestedUploadFileCheckListings.clear();
    result.currentIndex = -1;
    result.queueState = QueueState::Idle;
    return result;
}

}  // namespace transfer
