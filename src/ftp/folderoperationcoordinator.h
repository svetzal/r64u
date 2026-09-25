#ifndef FOLDEROPERATIONCOORDINATOR_H
#define FOLDEROPERATIONCOORDINATOR_H

#include "core/transfercore.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

class IFtpClient;
class ILocalFileSystemService;
class QTimer;

/**
 * @brief Coordinator for recursive folder upload/download/delete operations.
 *
 * Manages debounce, folder existence confirmation, and operation lifecycle.
 * Uses a callback for batch creation (to keep TransferQueue as the owner of
 * batch state) and emits signals to request the start of each phase.
 *
 * Local file system operations (directory existence, creation, deletion) are
 * routed through ILocalFileSystemService, keeping this coordinator independently
 * testable without real disk access.
 */
class FolderOperationCoordinator : public QObject
{
    Q_OBJECT

public:
    explicit FolderOperationCoordinator(transfer::State &state, IFtpClient *ftpClient,
                                        ILocalFileSystemService *localFs,
                                        QObject *parent = nullptr);

    void setFtpClient(IFtpClient *client);
    void setLocalFileSystem(ILocalFileSystemService *fs);

    /// Provide a callback for creating a new batch (returns batchId)
    void setCreateBatchCallback(std::function<int(transfer::OperationType, const QString &,
                                                  const QString &, const QString &)>
                                    callback);

    /// Enqueue a new recursive upload, download or delete operation.
    /// For deletes @p sourcePath is the remote folder and @p destPath is unused.
    /// The operation starts right away when nothing else runs, otherwise it waits its turn.
    void enqueueRecursive(transfer::OperationType type, const QString &sourcePath,
                          const QString &destPath);

    /// Called when the user responds to a folder-exists dialog
    void respondToFolderExists(transfer::FolderExistsResponse response);

    /// Called when a folder operation (scan+transfer) has fully completed
    void onFolderOperationComplete();

    /// Start the next pending folder op from state_.pendingFolderOps (for processNext).
    /// An operation whose folder-exists question is not settled yet is checked first.
    void startNextPendingFolderOp();

    /// Stop the debounce timer (called by TransferQueue::clear() and cancelAll())
    void stopDebounce();

    /// Resume folder confirmation flow after a remote folder existence check completes
    void resumeAfterFolderCheck();

signals:
    /// Request TransferQueue to start a download scan
    void startDownloadScanRequested(const QString &remotePath, const QString &localBase,
                                    const QString &remoteBase, int batchId);

    /// Request TransferQueue to queue directories and start directory creation
    void startDirectoryCreationRequested(const QString &localDir, const QString &remoteDir);

    /// Request the start of a recursive delete of @p remotePath (a Delete folder operation)
    void startDeleteRequested(const QString &remotePath);

    /// Ask the user whether to Merge/Replace/Cancel the listed existing folders
    void folderConfirmationNeeded(const QStringList &folderNames);

    /// A folder operation is starting
    void operationStarted(const QString &folderName, transfer::OperationType type);

    /// All queued folder operations finished
    void allOperationsCompleted();

    /// All queued folder operations were cancelled
    void operationsCancelled();

    void statusMessage(const QString &message, int timeout = 0);
    void scheduleProcessNextRequested();
    void operationFailed(const QString &fileName, const QString &error);

    /// The pendingUploadAfterDelete flag has been set; caller should start delete
    void pendingUploadAfterDeleteSet(const QString &targetPath);

private slots:
    void onDebounceTimeout();

private:
    [[nodiscard]] static QString batchDescription(transfer::OperationType type,
                                                  const QString &folderName);
    void startFolderOperation(const transfer::PendingFolderOp &op);
    /// Check whether the pending operations' target folders exist, then ask or start.
    void checkPendingFolderOps();
    void checkFolderConfirmation();

    transfer::State &state_;
    IFtpClient *ftpClient_ = nullptr;
    ILocalFileSystemService *localFs_ = nullptr;
    QTimer *debounceTimer_ = nullptr;
    static constexpr int DebounceMs = 50;

    std::function<int(transfer::OperationType, const QString &, const QString &, const QString &)>
        createBatchCallback_;
};

#endif  // FOLDEROPERATIONCOORDINATOR_H
