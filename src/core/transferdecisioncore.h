/**
 * @file transferdecisioncore.h
 * @brief Pure functions for deciding processNext/processNextDelete actions and handling FTP errors.
 */

#ifndef TRANSFERDECISIONCORE_H
#define TRANSFERDECISIONCORE_H

#include "transfercore.h"

#include <functional>

namespace transfer {

// ---------------------------------------------------------------------------
// Phase 7: processNext decision logic
// ---------------------------------------------------------------------------

/// @brief Describes what action TransferQueue::processNext() should take.
enum class ProcessNextAction {
    Blocked,                      ///< State machine disallows processing
    NoFtpClient,                  ///< FTP client not connected
    StartFolderOp,                ///< Dequeue and start a pending folder op
    NeedOverwriteCheck_Download,  ///< Local file exists, ask user
    NeedOverwriteCheck_Upload,    ///< Check remote file existence first
    StartTransfer,                ///< Start the next pending transfer
    NoPending                     ///< No pending items remain
};

/// @brief Decision produced by decideNextAction().
struct ProcessNextDecision
{
    ProcessNextAction action = ProcessNextAction::Blocked;
    int itemIndex = -1;               ///< For StartTransfer/NeedOverwriteCheck
    PendingFolderOp folderOpToStart;  ///< For StartFolderOp
    QString uploadCheckDir;           ///< Remote dir to LIST for NeedOverwriteCheck_Upload
    QString fileNameForSignal;        ///< File name for operationStarted signal
};

/// @brief Decide what processNext() should do next, given current state.
/// @param state Current transfer queue state.
/// @param ftpConnected Whether the FTP client is connected and ready.
/// @param localFileExists Predicate to check if a local file exists (injected to keep pure).
[[nodiscard]] ProcessNextDecision
decideNextAction(const State &state, bool ftpConnected,
                 const std::function<bool(const QString &)> &localFileExists);

// ---------------------------------------------------------------------------
// Phase 7: FTP error result
// ---------------------------------------------------------------------------

/// @brief Result of handling an FTP error.
struct FtpErrorResult
{
    State newState;
    bool isDeleteError = false;            ///< Error during recursive delete
    bool shouldProcessNextDelete = false;  ///< True when delete error should continue to next item
    QString deleteFileName;                ///< For operationFailed signal (delete)
    bool hasCurrentItem = false;           ///< Error during file transfer
    QString transferFileName;              ///< For operationFailed signal (transfer)
    int failedBatchId = -1;
    bool batchIsComplete = false;
    bool shouldScheduleProcessNext = false;
    bool isFolderCreationError = false;  ///< Error during directory creation
    QString folderName;                  ///< For operationFailed signal (folder)
    int folderBatchId = -1;
};

/// @brief Handle an FTP error, returning the new state and metadata for signal emission.
/// @param state Current transfer queue state.
/// @param message Error message from the FTP client.
[[nodiscard]] FtpErrorResult handleFtpError(const State &state, const QString &message);

// ---------------------------------------------------------------------------
// Phase 7: Operation timeout
// ---------------------------------------------------------------------------

/// @brief Handle an operation timeout — mark any InProgress item as Failed.
/// @param state Current transfer queue state.
/// @return Updated state with the timed-out item marked Failed and queue transitioned to Idle.
[[nodiscard]] State handleOperationTimeout(const State &state);

// ---------------------------------------------------------------------------
// Delete decision helpers
// ---------------------------------------------------------------------------

/// @brief Describes what processNextDelete() should do next.
enum class NextDeleteAction {
    AllDone,             ///< All deletes complete, no pending upload
    PendingUploadReady,  ///< All deletes complete, pending upload should start
    DispatchNext         ///< Dispatch the next delete item to the FTP client
};

/// @brief Decision produced by decideNextDeleteAction().
struct NextDeleteDecision
{
    NextDeleteAction action = NextDeleteAction::DispatchNext;
    int completedCount = 0;  ///< For "Deleted N items" message (AllDone/PendingUploadReady)
    DeleteItem nextItem;     ///< Valid when action == DispatchNext
};

/// @brief Decide what processNextDelete() should do given the current delete queue state.
/// Does NOT check FTP connectivity — caller must verify before dispatching.
[[nodiscard]] NextDeleteDecision decideNextDeleteAction(const State &state);

}  // namespace transfer

#endif  // TRANSFERDECISIONCORE_H
