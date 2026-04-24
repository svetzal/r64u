/**
 * @file recursivescancoordinator.cpp
 * @brief Implementation of RecursiveScanCoordinator.
 */

#include "recursivescancoordinator.h"

#include "services/iftpclient.h"
#include "services/ilocalfilesystem.h"
#include "services/transfercore.h"
#include "utils/logging.h"

#include <QFileInfo>

RecursiveScanCoordinator::RecursiveScanCoordinator(transfer::State &state, IFtpClient *ftpClient,
                                                   ILocalFileSystem *localFs, QObject *parent)
    : QObject(parent), state_(state), ftpClient_(ftpClient), localFs_(localFs)
{
}

void RecursiveScanCoordinator::setFtpClient(IFtpClient *client)
{
    ftpClient_ = client;
}

void RecursiveScanCoordinator::setLocalFileSystem(ILocalFileSystem *fs)
{
    localFs_ = fs;
}

bool RecursiveScanCoordinator::handlesListing(const QString &path) const
{
    return state_.requestedFolderCheckListings.contains(path) ||
           state_.requestedUploadFileCheckListings.contains(path) ||
           state_.requestedDeleteListings.contains(path) || state_.requestedListings.contains(path);
}

void RecursiveScanCoordinator::onDirectoryListed(const QString &path,
                                                 const QList<FtpEntry> &entries)
{
    if (state_.requestedFolderCheckListings.contains(path)) {
        handleFolderCheck(path, entries);
    } else if (state_.requestedUploadFileCheckListings.contains(path)) {
        handleUploadCheck(path, entries);
    } else if (state_.requestedDeleteListings.contains(path)) {
        handleDirectoryListingForDelete(path, entries);
    } else if (state_.requestedListings.contains(path)) {
        handleDirectoryListingForDownload(path, entries);
    }
}

void RecursiveScanCoordinator::startDownloadScan(const QString &remotePath,
                                                 const QString &localBase,
                                                 const QString &remoteBase, int batchId)
{
    state_.scanningFolderName = QFileInfo(remotePath).fileName();
    state_.directoriesScanned = 0;
    state_.filesDiscovered = 0;

    transfer::PendingScan scan;
    scan.remotePath = remotePath;
    scan.localBasePath = localBase;
    scan.remoteBasePath = remoteBase;
    scan.batchId = batchId;
    state_.pendingScans.enqueue(scan);

    state_.requestedListings.insert(remotePath);

    emit scanningStarted(state_.scanningFolderName, transfer::OperationType::Download);
    emit scanningProgress(0, 1, 0);

    if (ftpClient_) {
        ftpClient_->list(remotePath);
    }
}

void RecursiveScanCoordinator::startDeleteScan(const QString &remotePath)
{
    QString folderName = QFileInfo(remotePath).fileName();
    state_.scanningFolderName = folderName;
    state_.directoriesScanned = 0;
    state_.filesDiscovered = 0;

    transfer::PendingScan scan;
    scan.remotePath = remotePath;
    state_.pendingDeleteScans.enqueue(scan);
    state_.requestedDeleteListings.insert(remotePath);

    emit scanningStarted(folderName, transfer::OperationType::Delete);
    emit scanningProgress(0, 1, 0);

    if (ftpClient_) {
        ftpClient_->list(remotePath);
    }
}

void RecursiveScanCoordinator::handleDirectoryListingForDownload(const QString &path,
                                                                 const QList<FtpEntry> &entries)
{
    state_.requestedListings.remove(path);

    // Find matching scan
    transfer::PendingScan currentScan;
    bool found = false;
    for (int i = 0; i < state_.pendingScans.size(); ++i) {
        if (state_.pendingScans[i].remotePath == path) {
            currentScan = state_.pendingScans.takeAt(i);
            found = true;
            break;
        }
    }

    if (!found) {
        qCDebug(LogTransfer) << "RecursiveScanCoordinator: No matching pending scan for" << path;
        return;
    }

    state_.directoriesScanned++;

    auto listing = transfer::processDirectoryListingForDownload(currentScan, entries);

    for (const auto &[remotePath, localPath] : listing.newFileDownloads) {
        state_.filesDiscovered++;
        emit downloadFileDiscovered(remotePath, localPath, currentScan.batchId);
    }

    for (const auto &subScan : listing.newSubScans) {
        // Compute local directory path for this subdirectory and create it
        QString relativePath = subScan.remotePath.mid(currentScan.remoteBasePath.length());
        if (relativePath.startsWith('/'))
            relativePath = relativePath.mid(1);
        QString localDirPath = currentScan.localBasePath + '/' + relativePath;

        // Create local directory via gateway
        if (localFs_ && !localFs_->createDirectoryPath(localDirPath)) {
            qCDebug(LogTransfer) << "RecursiveScanCoordinator: Failed to create local directory"
                                 << localDirPath;
        }

        state_.pendingScans.enqueue(subScan);
        state_.requestedListings.insert(subScan.remotePath);
    }

    emit scanningProgress(state_.directoriesScanned, state_.pendingScans.size(),
                          state_.filesDiscovered);

    if (!state_.pendingScans.isEmpty()) {
        transfer::PendingScan next = state_.pendingScans.head();
        if (ftpClient_) {
            ftpClient_->list(next.remotePath);
        }
    } else {
        finishScanning();
    }
}

void RecursiveScanCoordinator::handleDirectoryListingForDelete(const QString &path,
                                                               const QList<FtpEntry> &entries)
{
    state_.requestedDeleteListings.remove(path);

    bool found = false;
    for (int i = 0; i < state_.pendingDeleteScans.size(); ++i) {
        if (state_.pendingDeleteScans[i].remotePath == path) {
            state_.pendingDeleteScans.removeAt(i);
            found = true;
            break;
        }
    }

    if (!found) {
        qCDebug(LogTransfer) << "RecursiveScanCoordinator: no matching pending delete scan for"
                             << path;
        return;
    }

    state_.directoriesScanned++;

    auto listing = transfer::processDirectoryListingForDelete(path, entries);

    for (const auto &fileItem : listing.fileItems) {
        state_.deleteQueue.append(fileItem);
        state_.filesDiscovered++;
    }

    for (const auto &subScan : listing.newSubScans) {
        state_.pendingDeleteScans.enqueue(subScan);
        state_.requestedDeleteListings.insert(subScan.remotePath);
    }

    state_.deleteQueue.append(listing.directoryItem);

    emit scanningProgress(state_.directoriesScanned, state_.pendingDeleteScans.size(),
                          state_.filesDiscovered);

    if (!state_.pendingDeleteScans.isEmpty()) {
        transfer::PendingScan next = state_.pendingDeleteScans.head();
        if (ftpClient_) {
            ftpClient_->list(next.remotePath);
        }
    } else {
        state_.deleteQueue = transfer::sortDeleteQueue(state_.deleteQueue);
        emit deleteScanComplete();
    }
}

void RecursiveScanCoordinator::handleFolderCheck(const QString &path,
                                                 const QList<FtpEntry> &entries)
{
    state_.requestedFolderCheckListings.remove(path);
    state_ = transfer::updateFolderExistence(state_, path, entries);
    emit folderCheckComplete(path);
}

void RecursiveScanCoordinator::handleUploadCheck(const QString &path,
                                                 const QList<FtpEntry> &entries)
{
    state_.requestedUploadFileCheckListings.remove(path);
    auto result = transfer::checkUploadFileExists(state_, entries);
    state_ = result.newState;

    if (result.fileExists) {
        emit uploadCheckFileExists(result.fileName);
    } else {
        emit uploadCheckNoConflict();
    }
}

void RecursiveScanCoordinator::finishScanning()
{
    qCDebug(LogTransfer) << "RecursiveScanCoordinator: Scanning complete, filesDiscovered:"
                         << state_.filesDiscovered;

    // Check for empty batches
    QList<int> emptyBatchIds;
    for (const transfer::TransferBatch &batch : state_.batches) {
        if (batch.operationType == transfer::OperationType::Download && batch.scanned &&
            batch.totalCount() == 0) {
            emptyBatchIds.append(batch.batchId);
        }
    }

    for (int batchId : emptyBatchIds) {
        qCDebug(LogTransfer) << "RecursiveScanCoordinator: Completing empty batch" << batchId;
        emit completeBatchRequested(batchId);
    }

    if (!emptyBatchIds.isEmpty()) {
        emit statusMessage(tr("%n empty folder(s) - nothing to download", "", emptyBatchIds.size()),
                           3000);
    }

    emit scheduleProcessNextRequested();
}
