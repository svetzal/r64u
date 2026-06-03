/**
 * @file transferdispatchhandler.cpp
 * @brief Implementation of TransferDispatchHandler.
 */

#include "transferdispatchhandler.h"

#include "ftp/folderoperationcoordinator.h"
#include "services/iftpclient.h"
#include "services/ilocalfilesystemservice.h"
#include "core/transferdecisioncore.h"
#include "utils/logging.h"

#include <QFileInfo>

TransferDispatchHandler::TransferDispatchHandler(transfer::State &state, QObject *parent)
    : QObject(parent), state_(state)
{
}

void TransferDispatchHandler::setFtpClient(IFtpClient *client)
{
    ftpClient_ = client;
}

void TransferDispatchHandler::setLocalFileSystem(ILocalFileSystemService *fs)
{
    localFs_ = fs;
}

void TransferDispatchHandler::setTimeoutManager(TransferTimeoutManager * /*manager*/)
{
    // Timeout is managed by TransferManager; this handler uses startTimeoutRequested /
    // stopTimeoutRequested signals instead of holding its own pointer.
}

void TransferDispatchHandler::setFolderCoordinator(FolderOperationCoordinator *coordinator)
{
    folderCoordinator_ = coordinator;
}

// ============================================================================
// State machine
// ============================================================================

void TransferDispatchHandler::transitionTo(QueueState newState)
{
    if (state_.queueState == newState) {
        return;
    }

    qCDebug(LogTransfer) << "TransferDispatchHandler: State transition"
                         << transfer::queueStateToString(state_.queueState) << "->"
                         << transfer::queueStateToString(newState);

    state_.queueState = newState;
}

// ============================================================================
// Core dispatch loop
// ============================================================================

void TransferDispatchHandler::processNext()
{
    qCDebug(LogTransfer) << "TransferDispatchHandler: processNext, state:"
                         << transfer::queueStateToString(state_.queueState);

    bool ftpReady = ftpClient_ && ftpClient_->isConnected();

    auto localFileExists = [this](const QString &path) -> bool {
        return localFs_ && localFs_->fileExists(path);
    };

    auto decision = transfer::decideNextAction(state_, ftpReady, localFileExists);

    switch (decision.action) {
    case transfer::ProcessNextAction::Blocked:
        qCDebug(LogTransfer) << "TransferDispatchHandler: processNext blocked by state:"
                             << transfer::queueStateToString(state_.queueState);
        emit statusMessage(tr("Transfer queue is busy"));
        return;

    case transfer::ProcessNextAction::NoFtpClient:
        qCDebug(LogTransfer) << "TransferDispatchHandler: FTP client not ready";
        emit operationFailed(QString(), tr("Not connected to device"));
        return;

    case transfer::ProcessNextAction::StartFolderOp:
        if (folderCoordinator_) {
            folderCoordinator_->startNextPendingFolderOp();
        }
        return;

    case transfer::ProcessNextAction::NeedOverwriteCheck_Download:
        transitionTo(QueueState::AwaitingFileConfirm);
        state_.currentIndex = decision.itemIndex;
        state_.pendingConfirmation.itemIndex = decision.itemIndex;
        state_.pendingConfirmation.opType = OperationType::Download;
        emit overwriteConfirmationNeeded(decision.fileNameForSignal, OperationType::Download);
        return;

    case transfer::ProcessNextAction::NeedOverwriteCheck_Upload:
        state_.currentIndex = decision.itemIndex;
        state_.requestedUploadFileCheckListings.insert(decision.uploadCheckDir);
        if (ftpClient_) {
            ftpClient_->list(decision.uploadCheckDir);
        }
        return;

    case transfer::ProcessNextAction::StartTransfer: {
        int i = decision.itemIndex;
        state_.currentIndex = i;
        state_.items[i].status = transfer::TransferItem::Status::InProgress;
        transitionTo(QueueState::Transferring);

        emit itemDataChanged(i);
        emit operationStarted(decision.fileNameForSignal, state_.items[i].operationType);

        emit startTimeoutRequested();

        if (state_.items[i].operationType == OperationType::Upload) {
            ftpClient_->upload(state_.items[i].localPath, state_.items[i].remotePath);
        } else if (state_.items[i].operationType == OperationType::Download) {
            ftpClient_->download(state_.items[i].remotePath, state_.items[i].localPath);
        } else if (state_.items[i].operationType == OperationType::Delete) {
            if (state_.items[i].isDirectory) {
                ftpClient_->removeDirectory(state_.items[i].remotePath);
            } else {
                ftpClient_->remove(state_.items[i].remotePath);
            }
        }
        return;
    }

    case transfer::ProcessNextAction::NoPending:
        qCDebug(LogTransfer) << "TransferDispatchHandler: No more pending items";
        emit stopTimeoutRequested();
        state_.currentIndex = -1;
        if (state_.batches.isEmpty()) {
            emit allOperationsCompleted();
        }
        return;
    }
}
