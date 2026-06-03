/**
 * @file transferconfirmcore.h
 * @brief Pure functions for handling overwrite and folder-exists confirmation logic.
 */

#ifndef TRANSFERCONFIRMCORE_H
#define TRANSFERCONFIRMCORE_H

#include "transfercore.h"

namespace transfer {

// ---------------------------------------------------------------------------
// Phase 4: Confirmation result structs
// ---------------------------------------------------------------------------

/// @brief Result of processing an overwrite response.
struct OverwriteResult
{
    State newState;
    bool shouldScheduleProcessNext = false;
    bool shouldCancelAll = false;
};

/// @brief Result of processing a folder-exists response.
struct FolderExistsResult
{
    State newState;
    bool shouldStartFolderOp = false;
    PendingFolderOp folderOpToStart;  ///< Valid when shouldStartFolderOp is true
    bool shouldCancelFolderOps = false;
};

/// @brief Result of checking folder confirmation requirements.
struct FolderConfirmResult
{
    State newState;
    bool needsConfirmation = false;
    QStringList existingFolderNames;  ///< Non-empty when needsConfirmation is true
    bool shouldStartFolderOp = false;
    PendingFolderOp folderOpToStart;  ///< Valid when shouldStartFolderOp is true
};

/// @brief Process an overwrite confirmation response.
/// Precondition: state.queueState == QueueState::AwaitingFileConfirm.
/// Returns unchanged state if precondition not met.
[[nodiscard]] OverwriteResult respondToOverwrite(const State &state, OverwriteResponse response);

/// @brief Process a folder-exists confirmation response.
/// Precondition: state.queueState == QueueState::AwaitingFolderConfirm.
/// Returns unchanged state if precondition not met.
[[nodiscard]] FolderExistsResult respondToFolderExists(const State &state,
                                                       FolderExistsResponse response);

/// @brief Check pending folder ops and decide if confirmation is needed.
/// Returns FolderConfirmResult describing what the shell should do next.
[[nodiscard]] FolderConfirmResult checkFolderConfirmation(const State &state);

}  // namespace transfer

#endif  // TRANSFERCONFIRMCORE_H
