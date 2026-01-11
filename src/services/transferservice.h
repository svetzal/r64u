/**
 * @file transferservice.h
 * @brief Service for coordinating file transfer operations.
 *
 * This service encapsulates the transfer workflow, providing high-level
 * signals for UI widgets instead of direct TransferQueue coupling.
 */

#ifndef TRANSFERSERVICE_H
#define TRANSFERSERVICE_H

#include <QObject>
#include <QString>

#include "models/transferqueue.h"

class IFtpClient;
class DeviceConnection;

/**
 * @brief Service for coordinating file transfer operations.
 *
 * TransferService provides a high-level interface for file transfer operations,
 * decoupling UI widgets from the TransferQueue model. Benefits include:
 * - UI widgets can be tested in isolation
 * - Transfer policy decisions are centralized
 * - Cleaner separation of concerns
 *
 * @par Example usage:
 * @code
 * TransferService *service = new TransferService(connection, queue, this);
 *
 * connect(service, &TransferService::operationStarted,
 *         this, &MyWidget::onOperationStarted);
 *
 * service->uploadFile("/local/file.txt", "/remote/dir/");
 * service->uploadDirectory("/local/folder", "/remote/dir/");
 * @endcode
 */
class TransferService : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a transfer service.
     * @param connection The device connection for connection state checks (not owned).
     * @param queue The transfer queue to delegate operations to (not owned).
     * @param parent Optional parent QObject for memory management.
     */
    explicit TransferService(DeviceConnection *connection,
                             TransferQueue *queue,
                             QObject *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~TransferService() override;

    /// @name Upload Operations
    /// @{

    /**
     * @brief Uploads a single file to the remote device.
     * @param localPath Path to the local file.
     * @param remoteDir Remote directory to upload to.
     * @return True if the operation was queued, false if not connected.
     */
    bool uploadFile(const QString &localPath, const QString &remoteDir);

    /**
     * @brief Recursively uploads a directory to the remote device.
     * @param localDir Path to the local directory.
     * @param remoteDir Remote directory to upload to.
     * @return True if the operation was queued, false if not connected.
     */
    bool uploadDirectory(const QString &localDir, const QString &remoteDir);
    /// @}

    /// @name Download Operations
    /// @{

    /**
     * @brief Downloads a single file from the remote device.
     * @param remotePath Path to the remote file.
     * @param localDir Local directory to download to.
     * @return True if the operation was queued, false if not connected.
     */
    bool downloadFile(const QString &remotePath, const QString &localDir);

    /**
     * @brief Recursively downloads a directory from the remote device.
     * @param remoteDir Path to the remote directory.
     * @param localDir Local directory to download to.
     * @return True if the operation was queued, false if not connected.
     */
    bool downloadDirectory(const QString &remoteDir, const QString &localDir);
    /// @}

    /// @name Delete Operations
    /// @{

    /**
     * @brief Deletes a file or directory from the remote device.
     * @param remotePath Path to the remote file or directory.
     * @param isDirectory True if the path is a directory.
     * @return True if the operation was queued, false if not connected.
     */
    bool deleteRemote(const QString &remotePath, bool isDirectory);

    /**
     * @brief Recursively deletes a directory from the remote device.
     * @param remotePath Path to the remote directory.
     * @return True if the operation was queued, false if not connected.
     */
    bool deleteRecursive(const QString &remotePath);
    /// @}

    /// @name Queue Management
    /// @{

    /**
     * @brief Cancels all pending operations.
     */
    void cancelAll();

    /**
     * @brief Removes completed items from the queue.
     */
    void removeCompleted();

    /**
     * @brief Clears all items from the queue.
     */
    void clear();
    /// @}

    /// @name Queue State
    /// @{

    /**
     * @brief Checks if any operations are in progress.
     * @return True if processing transfers.
     */
    [[nodiscard]] bool isProcessing() const;

    /**
     * @brief Checks if directory scanning is in progress.
     * @return True if scanning directories.
     */
    [[nodiscard]] bool isScanning() const;

    /**
     * @brief Checks if delete operations are in progress.
     * @return True if processing deletions.
     */
    [[nodiscard]] bool isProcessingDelete() const;

    /**
     * @brief Checks if directories are being created.
     * @return True if creating directories.
     */
    [[nodiscard]] bool isCreatingDirectories() const;

    /**
     * @brief Returns the number of pending items.
     * @return Count of pending items.
     */
    [[nodiscard]] int pendingCount() const;

    /**
     * @brief Returns the number of active items.
     * @return Count of active items.
     */
    [[nodiscard]] int activeCount() const;

    /**
     * @brief Returns the total item count for progress tracking.
     * @return Total number of items.
     */
    [[nodiscard]] int totalCount() const;

    /**
     * @brief Returns the delete progress count.
     * @return Number of deleted items.
     */
    [[nodiscard]] int deleteProgress() const;

    /**
     * @brief Returns the total delete count.
     * @return Total number of items to delete.
     */
    [[nodiscard]] int deleteTotalCount() const;

    /**
     * @brief Checks if scanning for delete operation.
     * @return True if scanning directories for delete.
     */
    [[nodiscard]] bool isScanningForDelete() const;
    /// @}

    /// @name Overwrite Handling
    /// @{

    /**
     * @brief Responds to an overwrite confirmation request.
     * @param response The user's response to the overwrite prompt.
     */
    void respondToOverwrite(OverwriteResponse response);

    /**
     * @brief Sets whether to automatically overwrite files.
     * @param autoOverwrite True to overwrite without prompting.
     */
    void setAutoOverwrite(bool autoOverwrite);
    /// @}

    /// @name Folder Exists Handling
    /// @{

    /**
     * @brief Responds to a folder exists confirmation request.
     * @param response The user's response to the folder exists prompt.
     */
    void respondToFolderExists(FolderExistsResponse response);

    /**
     * @brief Sets whether to automatically merge folders.
     * @param autoMerge True to merge without prompting.
     */
    void setAutoMerge(bool autoMerge);
    /// @}

    /**
     * @brief Returns the underlying TransferQueue (for model access).
     * @return Pointer to the TransferQueue.
     */
    [[nodiscard]] TransferQueue* queue() { return queue_; }

signals:
    /// @name Operation Signals
    /// @{

    /**
     * @brief Emitted when an operation starts.
     * @param fileName The name of the file being processed.
     * @param type The type of operation.
     */
    void operationStarted(const QString &fileName, OperationType type);

    /**
     * @brief Emitted when an operation completes successfully.
     * @param fileName The name of the file that was processed.
     */
    void operationCompleted(const QString &fileName);

    /**
     * @brief Emitted when an operation fails.
     * @param fileName The name of the file.
     * @param error The error message.
     */
    void operationFailed(const QString &fileName, const QString &error);

    /**
     * @brief Emitted when all operations are completed.
     */
    void allOperationsCompleted();

    /**
     * @brief Emitted when operations are cancelled.
     */
    void operationsCancelled();

    /**
     * @brief Emitted when the queue changes.
     */
    void queueChanged();
    /// @}

    /// @name Progress Signals
    /// @{

    /**
     * @brief Emitted to report delete progress.
     * @param fileName The file being deleted.
     * @param current Current item number.
     * @param total Total items to delete.
     */
    void deleteProgressUpdate(const QString &fileName, int current, int total);
    /// @}

    /// @name Confirmation Signals
    /// @{

    /**
     * @brief Emitted when overwrite confirmation is needed.
     * @param fileName The file that exists.
     * @param type The operation type.
     */
    void overwriteConfirmationNeeded(const QString &fileName, OperationType type);

    /**
     * @brief Emitted when folder exists confirmation is needed.
     * @param folderName The folder that exists.
     */
    void folderExistsConfirmationNeeded(const QString &folderName);
    /// @}

    /// @name Status Signals
    /// @{

    /**
     * @brief Emitted when a status message should be displayed.
     * @param message The message text.
     * @param timeout Display timeout in milliseconds (0 for no timeout).
     */
    void statusMessage(const QString &message, int timeout = 0);
    /// @}

private:
    DeviceConnection *connection_ = nullptr;
    TransferQueue *queue_ = nullptr;
};

#endif // TRANSFERSERVICE_H
