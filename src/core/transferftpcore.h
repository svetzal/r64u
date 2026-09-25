/**
 * @file transferftpcore.h
 * @brief Pure functions for FTP operation completion and enqueue helpers.
 */

#ifndef TRANSFERFTPCORE_H
#define TRANSFERFTPCORE_H

#include "transfercore.h"

namespace transfer {

// ---------------------------------------------------------------------------
// FTP handler helpers
// ---------------------------------------------------------------------------

/// @brief Result of transitioning queue state to Idle after a transfer completes.
struct CompleteTransferResult
{
    State newState;
    bool transitionedToIdle = false;  ///< True if queueState changed from Transferring to Idle
};

/// @brief Transition queue state to Idle after a file transfer completes.
/// Only transitions if currently in Transferring state; leaves all other states unchanged.
[[nodiscard]] CompleteTransferResult completeTransferOperation(const State &state);

/// @brief Result of advancing the recursive delete progress counter.
struct AdvanceDeleteResult
{
    State newState;
    bool advanced = false;  ///< True if the path matched and deletedCount was incremented
    QString fileName;       ///< Extracted file name for the progress signal
    int currentCount = 0;   ///< New deletedCount value after advance
    int totalCount = 0;     ///< Total deleteQueue size
};

/// @brief Advance the recursive delete progress counter when a file is removed.
/// Only advances if the queue is in Deleting state and the path matches deleteQueue[deletedCount].
[[nodiscard]] AdvanceDeleteResult advanceDeleteProgress(const State &state, const QString &path);

/// @brief Result of finding an InProgress delete item matching a remote path.
struct FindDeleteItemResult
{
    bool found = false;  ///< True if a matching InProgress delete item was found
    int itemIndex = -1;  ///< Index of the matched item in state.items
    QString fileName;    ///< Extracted file name for signal emission
};

/// @brief Find an InProgress delete item matching the given remote path.
/// Does NOT modify state — returns the item index for the caller to act on.
[[nodiscard]] FindDeleteItemResult findInProgressDeleteItem(const State &state,
                                                            const QString &path);

// ---------------------------------------------------------------------------
// Ownership of FTP client events
//
// The FTP client is shared with the file browser, previews, playlists and the
// config loader, so its signals only concern the queue when they belong to a
// request the queue itself issued.
// ---------------------------------------------------------------------------

/// @brief Returns true if the item the queue dispatched last is in flight and matches
///        the given operation.
///
/// Only the dispatched item qualifies: earlier rows with the same paths (cancelled,
/// failed or already completed, or the same file queued twice) never match.
/// @param type Operation type of the FTP request (Download, Upload or Delete).
/// @param remotePath Remote path of the request.
/// @param localPath Local path of the request; ignored for Delete.
[[nodiscard]] bool isInFlightItem(const State &state, OperationType type, const QString &remotePath,
                                  const QString &localPath);

/// @brief Returns true if the item the queue dispatched last is still in progress.
[[nodiscard]] bool hasInFlightItem(const State &state);

/// @brief Index of the in-flight item matching the given operation (see isInFlightItem()),
///        or -1 if the request is not the one the queue is waiting for.
[[nodiscard]] int inFlightItemIndex(const State &state, OperationType type,
                                    const QString &remotePath, const QString &localPath);

/// @brief Index of the in-flight download of @p remotePath, or -1 if there is none.
[[nodiscard]] int inFlightDownloadIndex(const State &state, const QString &remotePath);

/// @brief Index of the in-flight upload of @p localPath, or -1 if there is none.
[[nodiscard]] int inFlightUploadIndex(const State &state, const QString &localPath);

/// @brief Returns true if the queue is waiting for a listing of @p path
///        (recursive scan, delete scan, folder-exists or upload-exists check).
[[nodiscard]] bool isAwaitedListing(const State &state, const QString &path);

/// @brief Returns true if @p path is the directory the queue is creating right now.
[[nodiscard]] bool isAwaitedMkdir(const State &state, const QString &path);

/// @brief Returns true if @p path is the entry the recursive delete is removing right now.
[[nodiscard]] bool isAwaitedRecursiveDelete(const State &state, const QString &path);

// ---------------------------------------------------------------------------
// Enqueue item helpers
// ---------------------------------------------------------------------------

/// @brief Result of enqueueing an item into the transfer state.
struct EnqueueItemResult
{
    State newState;
    int batchIdx = -1;     ///< The batch index the item was added to
    int insertedRow = -1;  ///< Row index in state.items where item was inserted
};

/// @brief Append a transfer item to state.items and to the batch at batchIdx.
/// Does NOT activate, schedule, or emit signals — caller handles those side effects.
[[nodiscard]] EnqueueItemResult enqueueItem(const State &state, const TransferItem &item,
                                            int batchIdx);

}  // namespace transfer

#endif  // TRANSFERFTPCORE_H
