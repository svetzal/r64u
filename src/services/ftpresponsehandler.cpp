#include "ftpresponsehandler.h"

#include "core/ftpcore.h"

#include <QDebug>

// FTP response codes (must match C64UFtpClient constants)
static constexpr int FtpReplyServiceReady = 220;
static constexpr int FtpReplyUserLoggedIn = 230;
static constexpr int FtpReplyPasswordRequired = 331;
static constexpr int FtpReplyPathCreated = 257;
static constexpr int FtpReplyActionOk = 250;
static constexpr int FtpReplyEnteringPassive = 227;
static constexpr int FtpReplyFileStatusOk = 150;
static constexpr int FtpReplyDataConnectionOpen = 125;
static constexpr int FtpReplyTransferComplete = 226;
static constexpr int FtpReplyPendingFurtherInfo = 350;
static constexpr int FtpReplyErrorThreshold = 400;
static constexpr int FtpReplyFileExists = 553;

using Command = FtpCommandQueue::Command;
using State = IFtpClient::State;

FtpResponseHandler::FtpResponseHandler(FtpTransferState &transferState, QObject *parent)
    : QObject(parent), transferState_(transferState)
{
}

FtpResponseAction FtpResponseHandler::handleResponse(int code, const QString & /*text*/,
                                                     const FtpResponseContext &ctx)
{
    FtpResponseAction action;

    switch (ctx.clientState) {
    case State::Connected:
        if (code == FtpReplyServiceReady) {
            action.enqueueCommand = Command::User;
            action.kind = FtpResponseAction::Kind::ProcessNext;
        }
        break;

    case State::LoggingIn:
        action.kind = FtpResponseAction::Kind::ProcessNext;
        break;

    default:
        break;
    }

    return action;
}

FtpResponseAction FtpResponseHandler::handleBusyResponse(int code, const QString &text,
                                                         const FtpResponseContext &ctx)
{
    switch (ctx.currentCommand) {
    case Command::User:
        return handleUserResponse(code, text);
    case Command::Pass:
        return handlePassResponse(code, text);
    case Command::Pwd:
        return handlePwdResponse(code, text);
    case Command::Cwd:
        return handleCwdResponse(code, text, ctx);
    case Command::Type:
        return FtpResponseAction{FtpResponseAction::Kind::ProcessNext};
    case Command::Pasv:
        return handlePasvResponse(code, text);
    case Command::List:
        return handleListResponse(code, text, ctx);
    case Command::Retr:
        return handleRetrResponse(code, text, ctx);
    case Command::Stor:
        return handleStorResponse(code, text, ctx);
    case Command::Mkd:
        return handleMkdResponse(code, text, ctx);
    case Command::Rmd:
    case Command::Dele:
        return handleDeleteResponse(code, text, ctx);
    case Command::RnFr:
        return handleRenameFromResponse(code, text, ctx);
    case Command::RnTo:
        return handleRenameToResponse(code, text, ctx);
    default: {
        FtpResponseAction action;
        action.kind = FtpResponseAction::Kind::ProcessNext;
        return action;
    }
    }
}

FtpResponseAction FtpResponseHandler::handleUserResponse(int code, const QString &text) const
{
    FtpResponseAction action;
    action.kind = FtpResponseAction::Kind::ProcessNext;
    if (code == FtpReplyPasswordRequired) {
        action.enqueueCommand = Command::Pass;
    } else if (code == FtpReplyUserLoggedIn) {
        action.transitionToReady = true;
        action.transitionToLoggedIn = true;
        action.emitConnected = true;
    } else {
        action.kind = FtpResponseAction::Kind::Disconnect;
        action.errorMessage = tr("Login failed: server rejected username. %1").arg(text);
    }
    return action;
}

FtpResponseAction FtpResponseHandler::handlePassResponse(int code, const QString &text) const
{
    FtpResponseAction action;
    action.kind = FtpResponseAction::Kind::ProcessNext;
    if (code == FtpReplyUserLoggedIn) {
        action.transitionToReady = true;
        action.transitionToLoggedIn = true;
        action.emitConnected = true;
    } else {
        action.kind = FtpResponseAction::Kind::Disconnect;
        action.errorMessage = tr("Login failed: invalid password. %1").arg(text);
    }
    return action;
}

FtpResponseAction FtpResponseHandler::handlePwdResponse(int code, const QString &text) const
{
    FtpResponseAction action;
    action.kind = FtpResponseAction::Kind::ProcessNext;
    if (code == FtpReplyPathCreated) {
        if (auto path = ftp::parsePwdResponse(text)) {
            action.updatedCurrentDir = *path;
        }
    }
    return action;
}

FtpResponseAction FtpResponseHandler::handleCwdResponse(int code, const QString &text,
                                                        const FtpResponseContext &ctx) const
{
    FtpResponseAction action;
    action.kind = FtpResponseAction::Kind::ProcessNext;
    if (code == FtpReplyActionOk) {
        action.directoryChangedPath = ctx.currentArg;
    } else {
        action.errorMessage = tr("Cannot access directory '%1': %2").arg(ctx.currentArg, text);
    }
    return action;
}

FtpResponseAction FtpResponseHandler::handlePasvResponse(int code, const QString &text) const
{
    FtpResponseAction action;
    action.kind = FtpResponseAction::Kind::ProcessNext;
    if (code == FtpReplyEnteringPassive) {
        auto passiveResult = ftp::parsePassiveResponse(text);
        if (passiveResult) {
            action.kind = FtpResponseAction::Kind::ProcessNextAndConnect;
            action.dataHost = passiveResult->host;
            action.dataPort = passiveResult->port;
        } else {
            action.errorMessage = tr("Data transfer failed: unable to establish data connection");
        }
    } else {
        action.errorMessage =
            tr("Data transfer failed: server does not support passive mode. %1").arg(text);
    }
    return action;
}

FtpResponseAction FtpResponseHandler::handleListResponse(int code, const QString &text,
                                                         const FtpResponseContext &ctx)
{
    FtpResponseAction action;
    action.kind = FtpResponseAction::Kind::ProcessNext;
    if (code == FtpReplyFileStatusOk || code == FtpReplyDataConnectionOpen) {
        action.setDownloading = true;
        action.kind = FtpResponseAction::Kind::None;
    } else if (code == FtpReplyTransferComplete) {
        QString path = ctx.currentArg.isEmpty() ? ctx.currentDir : ctx.currentArg;
        if (ctx.dataSocketState == QAbstractSocket::UnconnectedState) {
            action.directoryListedPath = path;
            action.directoryListedEntries = ftp::parseDirectoryListing(transferState_.listBuffer());
            action.clearListBuffer = true;
        } else {
            action.pendingListPath = path;
            action.kind = FtpResponseAction::Kind::None;
        }
    } else if (code >= FtpReplyErrorThreshold) {
        action.errorMessage = tr("Cannot list directory contents: %1").arg(text);
    }
    return action;
}

FtpResponseAction FtpResponseHandler::handleRetrResponse(int code, const QString &text,
                                                         const FtpResponseContext &ctx)
{
    FtpResponseAction action;
    action.kind = FtpResponseAction::Kind::ProcessNext;
    if (code == FtpReplyFileStatusOk || code == FtpReplyDataConnectionOpen) {
        action.setDownloading = true;
        if (auto bytes = ftp::parseRetrByteCount(text)) {
            transferState_.setTransferSize(*bytes);
        }
        action.kind = FtpResponseAction::Kind::None;
    } else if (code == FtpReplyTransferComplete) {
        if (ctx.dataSocketState == QAbstractSocket::UnconnectedState) {
            if (transferState_.isCurrentRetrMemory()) {
                action.downloadToMemoryPath = ctx.currentArg;
                action.downloadToMemoryData = transferState_.retrBuffer();
                action.clearRetrBuffer = true;
            } else if (transferState_.currentRetrFile()) {
                action.downloadFinishedRemotePath = ctx.currentArg;
                action.downloadFinishedLocalPath = ctx.currentLocalPath;
            }
            action.clearCurrentRetrFile = true;
        } else {
            action.savePendingRetrRemotePath = ctx.currentArg;
            action.savePendingRetrLocalPath = ctx.currentLocalPath;
            action.kind = FtpResponseAction::Kind::None;
        }
    } else if (code >= FtpReplyErrorThreshold) {
        action.errorMessage = tr("Download failed for '%1': %2").arg(ctx.currentArg, text);
        if (transferState_.isCurrentRetrMemory()) {
            action.clearRetrBuffer = true;
        }
        action.clearCurrentRetrFile = true;
    }
    return action;
}

FtpResponseAction FtpResponseHandler::handleStorResponse(int code, const QString &text,
                                                         const FtpResponseContext &ctx) const
{
    FtpResponseAction action;
    action.kind = FtpResponseAction::Kind::ProcessNext;
    if (code == FtpReplyFileStatusOk || code == FtpReplyDataConnectionOpen) {
        action.kind = FtpResponseAction::Kind::None;
    } else if (code == FtpReplyTransferComplete) {
        action.uploadFinishedLocalPath = ctx.currentLocalPath;
        action.uploadFinishedRemotePath = ctx.currentArg;
        action.clearCurrentStorFile = true;
    } else if (code >= FtpReplyErrorThreshold) {
        action.errorMessage = tr("Upload failed for '%1': %2").arg(ctx.currentArg, text);
        action.clearCurrentStorFile = true;
    }
    return action;
}

FtpResponseAction FtpResponseHandler::handleMkdResponse(int code, const QString &text,
                                                        const FtpResponseContext &ctx) const
{
    FtpResponseAction action;
    action.kind = FtpResponseAction::Kind::ProcessNext;
    if (code == FtpReplyPathCreated || code == FtpReplyFileExists) {
        action.directoryCreatedPath = ctx.currentArg;
    } else {
        action.errorMessage = tr("Cannot create directory '%1': %2").arg(ctx.currentArg, text);
    }
    return action;
}

FtpResponseAction FtpResponseHandler::handleDeleteResponse(int code, const QString &text,
                                                           const FtpResponseContext &ctx) const
{
    FtpResponseAction action;
    action.kind = FtpResponseAction::Kind::ProcessNext;
    if (code == FtpReplyActionOk) {
        action.fileRemovedPath = ctx.currentArg;
    } else {
        action.errorMessage = tr("Cannot delete '%1': %2").arg(ctx.currentArg, text);
    }
    return action;
}

FtpResponseAction FtpResponseHandler::handleRenameFromResponse(int code, const QString &text,
                                                               const FtpResponseContext &ctx) const
{
    FtpResponseAction action;
    action.kind = FtpResponseAction::Kind::ProcessNext;
    if (code != FtpReplyPendingFurtherInfo) {
        action.errorMessage =
            tr("Cannot rename '%1': file not found or access denied. %2").arg(ctx.currentArg, text);
    }
    return action;
}

FtpResponseAction FtpResponseHandler::handleRenameToResponse(int code, const QString &text,
                                                             const FtpResponseContext &ctx) const
{
    FtpResponseAction action;
    action.kind = FtpResponseAction::Kind::ProcessNext;
    if (code == FtpReplyActionOk) {
        action.fileRenamedOldPath = ctx.currentLocalPath;
        action.fileRenamedNewPath = ctx.currentArg;
    } else {
        action.errorMessage = tr("Cannot rename to '%1': %2").arg(ctx.currentArg, text);
    }
    return action;
}

void FtpResponseHandler::handleDataReceived(const QByteArray &data, const FtpResponseContext &ctx)
{
    if (ctx.currentCommand == Command::List) {
        if (transferState_.hasPendingList()) {
            transferState_.appendToPendingList(data);
        } else {
            transferState_.appendListData(data);
        }
    } else if (ctx.currentCommand == Command::Retr) {
        bool isMemory = transferState_.hasPendingRetr() ? transferState_.pendingRetrIsMemory()
                                                        : transferState_.isCurrentRetrMemory();
        QFile *file = transferState_.hasPendingRetr() ? transferState_.pendingRetrFile()
                                                      : transferState_.currentRetrFile().get();

        if (isMemory) {
            transferState_.appendRetrData(data);
        } else if (file) {
            file->write(data);
        }
    }
}

FtpResponseAction FtpResponseHandler::handleDataDisconnected(const FtpResponseContext &ctx)
{
    FtpResponseAction action;
    transferState_.setDownloading(false);

    if (transferState_.hasPendingList()) {
        auto pending = transferState_.takePendingList();
        if (pending) {
            action.directoryListedPath = pending->path;
            action.directoryListedEntries = ftp::parseDirectoryListing(pending->buffer);
            action.kind = FtpResponseAction::Kind::ProcessNext;
        }
    } else if (transferState_.hasPendingRetr()) {
        auto pending = transferState_.takePendingRetr();
        if (pending) {
            if (pending->isMemory) {
                action.downloadToMemoryPath = pending->remotePath;
                action.downloadToMemoryData = transferState_.retrBuffer();
                action.clearRetrBuffer = true;
            } else if (pending->file) {
                pending->file->close();
                action.downloadFinishedRemotePath = pending->remotePath;
                action.downloadFinishedLocalPath = pending->localPath;
            }
            action.kind = FtpResponseAction::Kind::ProcessNext;
        }
    }

    return action;
}
