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
