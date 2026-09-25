#ifndef RECURSIVESCANCOORDINATOR_H
#define RECURSIVESCANCOORDINATOR_H

#include "core/transfercore.h"
#include "ftp/ftpentry.h"

#include <QObject>
#include <QString>

class IFtpClient;
class ILocalFileSystemService;

/**
 * @brief Coordinator for recursive directory scanning operations.
 *
 * Handles: download scans, delete scans, folder existence checks,
 * and upload file existence checks. Operates on the shared transfer::State
 * and emits signals when items are discovered or operations complete.
 *
 * Local file system mutations (creating directories during download scans)
 * are routed through ILocalFileSystemService, keeping this coordinator independently
 * testable without real disk access.
 */
class RecursiveScanCoordinator : public QObject
{
    Q_OBJECT

public:
    explicit RecursiveScanCoordinator(transfer::State &state, IFtpClient *ftpClient,
                                      ILocalFileSystemService *localFs, QObject *parent = nullptr);

    void setFtpClient(IFtpClient *client);
    void setLocalFileSystem(ILocalFileSystemService *fs);

    /// Returns true if this coordinator expects a listing for this path
    [[nodiscard]] bool handlesListing(const QString &path) const;

    /// Called by TransferQueue::onDirectoryListed when handlesListing() is true
    void onDirectoryListed(const QString &path, const QList<FtpEntry> &entries);

    /// Returns true if this coordinator awaits @p path as part of a recursive download scan
    [[nodiscard]] bool awaitsDownloadListing(const QString &path) const;

    /// A download scan's listing of @p path failed: the directory is reported through
    /// downloadDirectoryFailed() and the scan carries on with the remaining directories.
    void onDownloadListingFailed(const QString &path, const QString &message);

    /// Entry point for starting a recursive download scan
    void startDownloadScan(const QString &remotePath, const QString &localBase,
                           const QString &remoteBase, int batchId);

    /// Entry point for starting a recursive delete scan
    void startDeleteScan(const QString &remotePath);

signals:
    /// A file to download; @p size is its size in bytes from the listing.
    void downloadFileDiscovered(const QString &remotePath, const QString &localPath, int batchId,
                                qint64 size);
    /// Every directory of the download scan for @p batchId has been listed (or failed).
    void downloadScanComplete(int batchId);
    /// The remote directory @p remotePath (mirrored to @p localPath) could not be listed.
    void downloadDirectoryFailed(const QString &remotePath, const QString &localPath, int batchId,
                                 const QString &message);
    void deleteScanComplete();
    void folderCheckComplete(const QString &path);
    void uploadCheckFileExists(const QString &fileName);
    void uploadCheckNoConflict();
    void statusMessage(const QString &message, int timeout = 0);
    void scanningStarted(const QString &folderName, transfer::OperationType type);
    void scanningProgress(int directoriesScanned, int directoriesRemaining, int filesDiscovered);

private:
    void handleDirectoryListingForDownload(const QString &path, const QList<FtpEntry> &entries);
    void handleDirectoryListingForDelete(const QString &path, const QList<FtpEntry> &entries);
    void handleFolderCheck(const QString &path, const QList<FtpEntry> &entries);
    void handleUploadCheck(const QString &path, const QList<FtpEntry> &entries);
    /// Lists the next pending directory, or reports the scan of @p batchId complete.
    void continueDownloadScan(int batchId);

    transfer::State &state_;
    IFtpClient *ftpClient_ = nullptr;
    ILocalFileSystemService *localFs_ = nullptr;
};

#endif  // RECURSIVESCANCOORDINATOR_H
