#ifndef TRANSFERCORE_H
#define TRANSFERCORE_H

/**
 * @file transfercore.h
 * @brief Pure functions for transfer queue state transitions.
 *
 * Data types (enumerations, structs, State) are declared in transfertypes.h,
 * which this header includes.  Callers that previously included only
 * transfercore.h continue to see all types without modification.
 */

#include "transfertypes.h"

#include <QVariant>

namespace transfer {

// ---------------------------------------------------------------------------
// Path utilities
// ---------------------------------------------------------------------------

/// @brief Strips trailing slashes from a path, preserving the root "/".
/// @param path Input path string.
/// @return Normalized path without trailing slashes.
[[nodiscard]] QString normalizePath(const QString &path);

// ---------------------------------------------------------------------------
// Item counting (read-only queries)
// ---------------------------------------------------------------------------

/// @brief Count items with Pending status.
[[nodiscard]] int pendingCount(const State &state);

/// @brief Count items with InProgress status.
[[nodiscard]] int activeCount(const State &state);

/// @brief Count items that are Pending or InProgress.
[[nodiscard]] int activeAndPendingCount(const State &state);

// ---------------------------------------------------------------------------
// Batch queries
// ---------------------------------------------------------------------------

/// @brief Count incomplete batches (not yet isComplete()).
[[nodiscard]] int queuedBatchCount(const State &state);

/// @brief Returns all batch IDs for incomplete batches.
[[nodiscard]] QList<int> allBatchIds(const State &state);

/// @brief Returns true if activeBatchIndex refers to a valid batch.
[[nodiscard]] bool hasActiveBatch(const State &state);

// ---------------------------------------------------------------------------
// Lookup functions
// ---------------------------------------------------------------------------

/// @brief Find a batch index by batchId. Returns -1 if not found.
[[nodiscard]] int findBatchIndex(const State &state, int batchId);

/// @brief Find an item index by local and remote path. Returns -1 if not found.
[[nodiscard]] int findItemIndex(const State &state, const QString &localPath,
                                const QString &remotePath);

/// @brief Find the index of the next pending item. Returns -1 if none.
[[nodiscard]] int findNextPendingItem(const State &state);

// ---------------------------------------------------------------------------
// Duplicate detection
// ---------------------------------------------------------------------------

/// @brief Returns true if a path is already being transferred.
/// Checks active batches (by sourcePath), pending scans, and pending folder ops.
/// @param path Path to check (will be normalized internally).
/// @param type Operation type to match.
[[nodiscard]] bool isPathBeingTransferred(const State &state, const QString &path,
                                          OperationType type);

// ---------------------------------------------------------------------------
// State machine gate
// ---------------------------------------------------------------------------

/// @brief Returns true if the state machine allows processNext() to run.
/// Only QueueState::Idle allows processing.
[[nodiscard]] bool canProcessNext(QueueState queueState);

// ---------------------------------------------------------------------------
// Progress computation
// ---------------------------------------------------------------------------

/// @brief Compute BatchProgress for the active batch (activeBatchIndex).
/// Returns invalid BatchProgress (batchId == -1) if no active batch.
[[nodiscard]] BatchProgress computeActiveBatchProgress(const State &state);

/// @brief Compute BatchProgress for a specific batch by ID.
/// Returns invalid BatchProgress (batchId == -1) if not found.
[[nodiscard]] BatchProgress computeBatchProgress(const State &state, int batchId);

// ---------------------------------------------------------------------------
// Model data
// ---------------------------------------------------------------------------

/// @brief Extract display data for a transfer item at the given Qt model role.
/// Used by QAbstractListModel::data().
[[nodiscard]] QVariant itemData(const TransferItem &item, int role);

// ---------------------------------------------------------------------------
// State mutation result structs
// ---------------------------------------------------------------------------

/// @brief Result of creating a new batch.
struct CreateBatchResult
{
    State newState;
    int batchId = -1;  ///< ID of the newly created batch
};

/// @brief Result of completing a batch.
struct CompleteBatchResult
{
    State newState;
    int batchId = -1;
    bool isFolderOperation = false;          ///< True if batch belongs to currentFolderOp
    bool hasRemainingActiveBatches = false;  ///< True if there are more incomplete batches
};

/// @brief Result of cancelling a specific batch.
struct CancelBatchResult
{
    State newState;
    bool wasActiveBatch = false;  ///< True if the cancelled batch was the active one
};

/// @brief Result of marking an item complete.
struct MarkCompleteResult
{
    State newState;
    int batchId = -1;
    bool batchIsComplete = false;  ///< True if this was the last item in its batch
};

// ---------------------------------------------------------------------------
// State mutation functions
// ---------------------------------------------------------------------------

/// @brief Purge a completed batch and all its items from state.
/// Adjusts currentIndex and activeBatchIndex as needed.
/// @note Does NOT emit model signals — caller must call beginRemoveRows/endRemoveRows.
[[nodiscard]] State purgeBatch(const State &state, int batchId);

/// @brief Plan for purging a batch: which item indices to remove (descending) and the batch index.
struct PurgeBatchPlan
{
    QList<int> itemIndicesToRemove;  ///< Indices in descending order — safe for sequential removal
    int batchIndex = -1;             ///< Index of the batch in batches list, -1 if not found
};

/// @brief Compute which items to remove for a batch purge, without modifying state.
/// Returns indices in descending order so the caller can remove them one-by-one with Qt model
/// signals interleaved, without invalidating earlier indices.
[[nodiscard]] PurgeBatchPlan planBatchPurge(const State &state, int batchId);

/// @brief Create a new batch (purging completed batches first).
/// @param type Operation type for the batch.
/// @param description Human-readable description.
/// @param folderName Top-level folder name for display.
/// @param sourcePath Root path (for duplicate detection). May be empty.
[[nodiscard]] CreateBatchResult createBatch(const State &state, OperationType type,
                                            const QString &description, const QString &folderName,
                                            const QString &sourcePath = QString());

/// @brief Find the next incomplete batch with pending items and mark it active.
/// Sets activeBatchIndex = -1 if none found.
[[nodiscard]] State activateNextBatch(const State &state);

/// @brief Complete a batch: reset active batch index, transition to Idle, reset currentIndex.
/// Does NOT call onFolderOperationComplete — returns metadata for caller.
[[nodiscard]] CompleteBatchResult completeBatch(const State &state, int batchId);

/// @brief Mark an item as complete/failed/skipped and update its batch counters.
/// Also resets currentIndex to -1.
[[nodiscard]] MarkCompleteResult markItemComplete(const State &state, int itemIndex,
                                                  TransferItem::Status status,
                                                  const QString &errorMessage = QString());

/// @brief Clear all state — items, batches, scanning state, preferences.
/// Equivalent to a full reset.
[[nodiscard]] State clearAll(const State &state);

/// @brief Remove Completed/Failed/Skipped items from the item list.
/// Adjusts currentIndex to stay valid.
[[nodiscard]] State removeCompleted(const State &state);

/// @brief Mark all Pending and InProgress items as Failed("Cancelled").
/// Clears batches, scanning state, folder ops, and preferences.
[[nodiscard]] State cancelAllItems(const State &state);

/// @brief Mark a specific batch's Pending/InProgress items as Failed("Cancelled").
/// If the batch was the active batch, transitions to Idle and resets scanning state.
[[nodiscard]] CancelBatchResult cancelBatch(const State &state, int batchId);

}  // namespace transfer

// ---------------------------------------------------------------------------
// Backward-compatible includes for extracted modules.
// Callers that previously included only transfercore.h continue to see all
// transfer:: types and functions without modification.
// ---------------------------------------------------------------------------
#include "transferconfirmcore.h"
#include "transferdecisioncore.h"
#include "transferftpcore.h"
#include "transferlistingcore.h"

#endif  // TRANSFERCORE_H
