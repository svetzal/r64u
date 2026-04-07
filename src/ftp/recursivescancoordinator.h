/**
 * @file recursivescancoordinator.h
 * @brief Coordinator for recursive directory scanning operations.
 */

#ifndef RECURSIVESCANCOORDINATOR_H
#define RECURSIVESCANCOORDINATOR_H

#include "services/ftpentry.h"
#include "services/transfercore.h"

#include <QObject>
#include <QString>

class IFtpClient;

/**
 * @brief Coordinator for recursive directory scanning operations.
 *
 * Handles: download scans, delete scans, folder existence checks,
 * and upload file existence checks. Operates on the shared transfer::State
 * and emits signals when items are discovered or operations complete.
 */
class RecursiveScanCoordinator : public QObject
{
    Q_OBJECT

public:
    explicit RecursiveScanCoordinator(transfer::State &state, IFtpClient *ftpClient,
                                      QObject *parent = nullptr);

    void setFtpClient(IFtpClient *client);

    /// Returns true if this coordinator expects a listing for this path
    [[nodiscard]] bool handlesListing(const QString &path) const;

    /// Called by TransferQueue::onDirectoryListed when handlesListing() is true
    void onDirectoryListed(const QString &path, const QList<FtpEntry> &entries);

    /// Entry point for starting a recursive download scan
    void startDownloadScan(const QString &remotePath, const QString &localBase,
                           const QString &remoteBase, int batchId);

    /// Entry point for starting a recursive delete scan
    void startDeleteScan(const QString &remotePath);

signals:
    void downloadFileDiscovered(const QString &remotePath, const QString &localPath, int batchId);
    void downloadScanComplete();
    void deleteScanComplete();
    void folderCheckComplete(const QString &path);
    void uploadCheckFileExists(const QString &fileName);
    void uploadCheckNoConflict();
    void completeBatchRequested(int batchId);
    void scheduleProcessNextRequested();
    void statusMessage(const QString &message, int timeout);
    void scanningStarted(const QString &folderName, transfer::OperationType type);
    void scanningProgress(int directoriesScanned, int directoriesRemaining, int filesDiscovered);

private:
    void handleDirectoryListingForDownload(const QString &path, const QList<FtpEntry> &entries);
    void handleDirectoryListingForDelete(const QString &path, const QList<FtpEntry> &entries);
    void handleFolderCheck(const QString &path, const QList<FtpEntry> &entries);
    void handleUploadCheck(const QString &path, const QList<FtpEntry> &entries);
    void finishScanning();

    transfer::State &state_;
    IFtpClient *ftpClient_ = nullptr;
};

#endif  // RECURSIVESCANCOORDINATOR_H
