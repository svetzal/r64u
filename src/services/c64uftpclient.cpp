#include "c64uftpclient.h"

#include "ftpresponsehandler.h"

#include "core/ftpcore.h"
#include "utils/logging.h"

#include <QFileInfo>
#include <QRegularExpression>

#include <array>
#include <filesystem>
#include <system_error>

C64UFtpClient::C64UFtpClient(QObject *parent)
    : IFtpClient(parent), controlSocket_(new QTcpSocket(this)), dataSocket_(new QTcpSocket(this)),
      connectionTimer_(new QTimer(this)), abortReplyTimer_(new QTimer(this)),
      responseHandler_(new FtpResponseHandler(transferState_, this))
{
    // Connection timeout timer
    connectionTimer_->setSingleShot(true);
    connect(connectionTimer_, &QTimer::timeout, this, &C64UFtpClient::onConnectionTimeout);

    abortReplyTimer_->setSingleShot(true);
    connect(abortReplyTimer_, &QTimer::timeout, this, &C64UFtpClient::onAbortReplyTimeout);

    connect(controlSocket_, &QTcpSocket::connected, this, &C64UFtpClient::onControlConnected);
    connect(controlSocket_, &QTcpSocket::disconnected, this, &C64UFtpClient::onControlDisconnected);
    connect(controlSocket_, &QTcpSocket::readyRead, this, &C64UFtpClient::onControlReadyRead);
    connect(controlSocket_, &QTcpSocket::errorOccurred, this, &C64UFtpClient::onControlError);

    connect(dataSocket_, &QTcpSocket::connected, this, &C64UFtpClient::onDataConnected);
    connect(dataSocket_, &QTcpSocket::readyRead, this, &C64UFtpClient::onDataReadyRead);
    connect(dataSocket_, &QTcpSocket::disconnected, this, &C64UFtpClient::onDataDisconnected);
    connect(dataSocket_, &QTcpSocket::errorOccurred, this, &C64UFtpClient::onDataError);
    connect(dataSocket_, &QTcpSocket::bytesWritten, this, &C64UFtpClient::onDataBytesWritten);

    // IFtpClient base constructor wires error() → errorReported()
}

C64UFtpClient::~C64UFtpClient()
{
    drainCommandQueue();
    resetTransferState();
}

void C64UFtpClient::setHost(const QString &host, quint16 port)
{
    host_ = host;
    port_ = port;
}

void C64UFtpClient::setCredentials(const QString &user, const QString &password)
{
    user_ = user.isEmpty() ? "anonymous" : user;
    password_ = password;
}

void C64UFtpClient::setState(State state)
{
    if (state_ != state) {
        state_ = state;
        emit stateChanged(state);
    }
}

void C64UFtpClient::connectToHost()
{
    if (state_ != State::Disconnected) {
        qCDebug(LogFtp) << "FTP: connectToHost called but state is" << static_cast<int>(state_);
        emit error(tr("Cannot connect: connection already in progress or established"));
        return;
    }

    qCDebug(LogFtp) << "FTP: Connecting to" << host_ << ":" << port_;
    setState(State::Connecting);

    connectionTimer_->start(ConnectionTimeoutMs);

    // Defer to event loop to ensure Connecting state is observable before errors fire
    QTimer::singleShot(0, this, [this]() {
        if (state_ == State::Connecting) {
            controlSocket_->connectToHost(host_, port_);
        }
    });
}

void C64UFtpClient::disconnect()
{
    if (state_ == State::Disconnected) {
        qCDebug(LogFtp) << "FTP: disconnect() called when already disconnected — no-op";
        return;
    }

    commandQueue_.drain();
    loggedIn_ = false;
    resetCommandTracking();
    discardDataTransfer();

    if (controlSocket_->state() == QAbstractSocket::ConnectedState) {
        sendCommand("QUIT");
    }
    if (controlSocket_->state() != QAbstractSocket::UnconnectedState) {
        controlSocket_->disconnectFromHost();
    }

    setState(State::Disconnected);
}

void C64UFtpClient::sendCommand(const QString &command)
{
    if (controlSocket_->state() != QAbstractSocket::ConnectedState) {
        qCWarning(LogFtp) << "FTP: Cannot send command, socket not connected";
        emit error(tr("Cannot send command: not connected"));
        return;
    }
    if (command.startsWith("PASS ")) {
        qCDebug(LogFtp) << "FTP: >>" << "PASS ****";
    } else {
        qCDebug(LogFtp) << "FTP: >>" << command;
    }
    controlSocket_->write((command + "\r\n").toUtf8());
}

void C64UFtpClient::queueCommand(Command cmd, const QString &arg, const QString &localPath,
                                 quint64 operationId)
{
    commandQueue_.enqueue(cmd, arg, localPath, operationId);

    if (state_ == State::Ready) {
        processNextCommand();
    }
}

void C64UFtpClient::queueRetrCommand(const QString &remotePath, const QString &localPath,
                                     std::shared_ptr<QFile> file, bool isMemory,
                                     quint64 operationId)
{
    commandQueue_.enqueueRetr(remotePath, localPath, std::move(file), isMemory, operationId);

    if (state_ == State::Ready) {
        processNextCommand();
    }
}

void C64UFtpClient::queueStorCommand(const QString &remotePath, const QString &localPath,
                                     std::shared_ptr<QFile> file, quint64 operationId)
{
    commandQueue_.enqueueStor(remotePath, localPath, std::move(file), operationId);

    if (state_ == State::Ready) {
        processNextCommand();
    }
}

void C64UFtpClient::processNextCommand()
{
    if (commandQueue_.isEmpty()) {
        currentCommand_ = Command::None;
        setState(State::Ready);
        return;
    }

    setState(State::Busy);
    PendingCommand pending = commandQueue_.dequeueNext();
    currentCommand_ = pending.cmd;
    currentArg_ = pending.arg;
    currentLocalPath_ = pending.localPath;
    currentOperationId_ = pending.operationId;
    upload_ = UploadProgress{};

    // Per-transfer state is initialised when the transfer starts, never when it
    // is queued: other transfers may still be using the shared state until then.
    if (currentCommand_ == Command::List) {
        transferState_.clearListBuffer();
    } else if (currentCommand_ == Command::Retr) {
        transferState_.clearRetrBuffer();
        transferState_.setTransferSize(0);
        std::shared_ptr<QFile> partFile;
        if (!pending.isMemoryDownload) {
            partFile = openPartialDownload(currentLocalPath_);
            if (!partFile) {
                failDownloadBeforeStart();
                return;
            }
        }
        transferState_.setCurrentRetrFile(std::move(partFile), pending.isMemoryDownload);
        qCDebug(LogFtp) << "FTP: Processing RETR, file:" << transferState_.currentRetrFile().get()
                        << "isMemory:" << transferState_.isCurrentRetrMemory();
    } else if (currentCommand_ == Command::Stor) {
        transferState_.setTransferSize(pending.transferFile ? pending.transferFile->size() : 0);
        transferState_.setCurrentStorFile(std::move(pending.transferFile));
        qCDebug(LogFtp) << "FTP: Processing STOR, file:" << transferState_.currentStorFile().get();
    }

    QString wireCommand = ftp::formatCommand(currentCommand_, currentArg_, user_, password_);
    if (!wireCommand.isEmpty()) {
        awaitingFinalReply_ = true;
        sendCommand(wireCommand);
    } else {
        // Command::None or any unrecognised command — skip and process the next one
        processNextCommand();
    }
}

FtpResponseContext C64UFtpClient::buildContext() const
{
    return {currentCommand_, currentArg_, currentLocalPath_,   state_,
            loggedIn_,       currentDir_, dataSocket_->state()};
}

void C64UFtpClient::applyAction(const FtpResponseAction &action)
{
    if (!action.errorMessage.isEmpty()) {
        // An error ends the operation: its remaining commands (e.g. the RETR
        // after a failed PASV) would only fail again and report a second error.
        // Servers may leave the passive connection open after refusing the
        // transfer; close it so the next PASV can connect. A partial download
        // is removed before the state mutations below release its handle.
        dropRestOfCurrentOperation();
        abortDataConnection();
        discardPartialDownloads();
    }
    applyTransferStateMutations(action);
    applyConnectionStateChanges(action);
    emitResponseSignals(action);
    executeResponseAction(action);
}

void C64UFtpClient::applyTransferStateMutations(const FtpResponseAction &action)
{
    if (action.setDownloading) {
        transferState_.setDownloading(true);
    }
    if (action.clearListBuffer) {
        transferState_.clearListBuffer();
    }
    if (action.clearCurrentRetrFile) {
        transferState_.clearCurrentRetrFile();
    }
    if (action.clearCurrentStorFile) {
        transferState_.clearCurrentStorFile();
    }
    if (action.clearRetrBuffer) {
        transferState_.clearRetrBuffer();
    }

    if (!action.pendingListPath.isNull()) {
        transferState_.savePendingList(action.pendingListPath, transferState_.listBuffer());
        transferState_.clearListBuffer();
    }
    if (!action.savePendingRetrRemotePath.isNull()) {
        transferState_.savePendingRetr(
            action.savePendingRetrRemotePath, action.savePendingRetrLocalPath,
            transferState_.currentRetrFile(), transferState_.isCurrentRetrMemory());
        transferState_.setCurrentRetrFile(nullptr, false);
    }
}

void C64UFtpClient::applyConnectionStateChanges(const FtpResponseAction &action)
{
    if (action.transitionToReady) {
        setState(State::Ready);
    }
    if (action.transitionToLoggedIn) {
        loggedIn_ = true;
    }
    if (!action.updatedCurrentDir.isEmpty()) {
        currentDir_ = action.updatedCurrentDir;
    }
    if (action.enqueueCommand != Command::None) {
        queueCommand(action.enqueueCommand);
    }
}

void C64UFtpClient::emitResponseSignals(const FtpResponseAction &action)
{
    if (action.emitConnected) {
        emit connected();
    }
    if (action.emitDisconnected) {
        emit disconnected();
    }
    if (!action.directoryChangedPath.isEmpty()) {
        currentDir_ = action.directoryChangedPath;
        emit directoryChanged(action.directoryChangedPath);
    }
    if (!action.directoryCreatedPath.isEmpty()) {
        emit directoryCreated(action.directoryCreatedPath);
    }
    if (!action.fileRemovedPath.isEmpty()) {
        emit fileRemoved(action.fileRemovedPath);
    }
    if (!action.fileRenamedOldPath.isEmpty()) {
        emit fileRenamed(action.fileRenamedOldPath, action.fileRenamedNewPath);
    }
    if (!action.downloadFinishedRemotePath.isEmpty()) {
        const QString commitError = commitPartialDownload(action.downloadFinishedLocalPath);
        if (commitError.isEmpty()) {
            emit downloadFinished(action.downloadFinishedRemotePath,
                                  action.downloadFinishedLocalPath);
        } else {
            emit error(
                tr("Cannot save file '%1': %2").arg(action.downloadFinishedLocalPath, commitError));
        }
    }
    if (!action.downloadToMemoryPath.isEmpty()) {
        emit downloadToMemoryFinished(action.downloadToMemoryPath, action.downloadToMemoryData);
    }
    if (!action.uploadFinishedLocalPath.isEmpty()) {
        emit uploadFinished(action.uploadFinishedLocalPath, action.uploadFinishedRemotePath);
    }
    if (!action.directoryListedPath.isEmpty()) {
        emit directoryListed(action.directoryListedPath, action.directoryListedEntries);
    }
    if (!action.downloadProgressPath.isEmpty()) {
        emit downloadProgress(action.downloadProgressPath, action.downloadProgressBytes,
                              action.downloadProgressTotal);
    }
    if (!action.errorMessage.isEmpty()) {
        emit error(action.errorMessage);
    }
}

void C64UFtpClient::startUpload()
{
    auto storFile = transferState_.currentStorFile();
    if (!storFile || !storFile->isOpen()) {
        qCWarning(LogFtp) << "FTP: ERROR - STOR 150 but no file handle! currentStorFile:"
                          << storFile.get();
        return;
    }
    upload_ = UploadProgress{};
    upload_.active = true;
    sendNextUploadChunks();
}

void C64UFtpClient::sendNextUploadChunks()
{
    auto storFile = transferState_.currentStorFile();
    if (!upload_.active || upload_.allDataQueued || !storFile) {
        return;
    }

    // Keep at most about one chunk buffered in the socket so progress follows
    // what has actually been sent and large files are never held in memory.
    while (dataSocket_->bytesToWrite() < UploadChunkSize && !storFile->atEnd()) {
        const QByteArray chunk = storFile->read(UploadChunkSize);
        if (chunk.isEmpty()) {
            failInFlightTransfer(
                tr("Cannot read file '%1': %2").arg(currentLocalPath_, storFile->errorString()));
            return;
        }
        dataSocket_->write(chunk);
    }

    if (storFile->atEnd()) {
        // Closing the data connection (after the buffer drains) ends the STOR
        upload_.allDataQueued = true;
        dataSocket_->disconnectFromHost();
    }
}

void C64UFtpClient::onDataBytesWritten(qint64 bytes)
{
    if (!upload_.active || currentCommand_ != Command::Stor) {
        return;
    }
    upload_.bytesSent += bytes;
    emit uploadProgress(currentLocalPath_, upload_.bytesSent, transferState_.transferSize());
    sendNextUploadChunks();
}

void C64UFtpClient::connectDataSocketForAction(const FtpResponseAction &action)
{
    QString actualHost = controlSocket_->peerAddress().toString();
    qCDebug(LogFtp) << "FTP: PASV response host:" << action.dataHost << "port:" << action.dataPort;
    qCDebug(LogFtp) << "FTP: Using actual host:" << actualHost << "port:" << action.dataPort;
    // connectToHost() is ignored while a previous connection lingers
    abortDataConnection();
    dataSocket_->connectToHost(actualHost, action.dataPort);
}

void C64UFtpClient::executeResponseAction(const FtpResponseAction &action)
{
    if (action.startUpload) {
        startUpload();
    }

    switch (action.kind) {
    case FtpResponseAction::Kind::ProcessNext:
        processNextCommand();
        break;

    case FtpResponseAction::Kind::ProcessNextAndConnect:
        connectDataSocketForAction(action);
        processNextCommand();
        break;

    case FtpResponseAction::Kind::Disconnect:
        disconnect();
        break;

    case FtpResponseAction::Kind::EmitError:
        // Already emitted in emitResponseSignals
        break;

    case FtpResponseAction::Kind::None:
    default:
        break;
    }
}

void C64UFtpClient::onControlConnected()
{
    qCDebug(LogFtp) << "FTP: Control socket connected to"
                    << controlSocket_->peerAddress().toString();
    connectionTimer_->stop();
    setState(State::Connected);
}

void C64UFtpClient::onControlDisconnected()
{
    qCDebug(LogFtp) << "FTP: Control socket disconnected";
    performDisconnectCleanup();
    emit disconnected();
}

void C64UFtpClient::onControlError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError)
    qCWarning(LogFtp) << "FTP: Control socket error:" << socketError
                      << controlSocket_->errorString();
    QString errorMessage = controlSocket_->errorString();
    performDisconnectCleanup();
    emit error(errorMessage);
}

void C64UFtpClient::onConnectionTimeout()
{
    qCWarning(LogFtp) << "FTP: Connection timeout";
    controlSocket_->abort();
    performDisconnectCleanup();
    emit error(tr("Connection timed out after %1 seconds").arg(ConnectionTimeoutMs / 1000));
}

void C64UFtpClient::onControlReadyRead()
{
    responseBuffer_ += QString::fromUtf8(controlSocket_->readAll());

    auto parsed = ftp::splitResponseLines(responseBuffer_);
    responseBuffer_ = parsed.remainingBuffer;

    for (const auto &line : parsed.lines) {
        qCDebug(LogFtp) << "FTP: <<" << line.code << line.text
                        << "(state:" << static_cast<int>(state_) << ")";

        if (repliesToDiscard_ > 0) {
            discardReply(line.code);
            continue;
        }
        if (state_ == State::Busy && line.code >= FtpReplyFinalThreshold) {
            awaitingFinalReply_ = false;
        }

        FtpResponseContext ctx = buildContext();
        FtpResponseAction action;
        if (state_ == State::Busy) {
            action = responseHandler_->handleBusyResponse(line.code, line.text, ctx);
        } else {
            action = responseHandler_->handleResponse(line.code, line.text, ctx);
        }

        applyAction(action);
    }
}

void C64UFtpClient::onDataConnected()
{
    qCDebug(LogFtp) << "FTP: Data socket connected to" << dataSocket_->peerAddress().toString()
                    << ":" << dataSocket_->peerPort();
}

void C64UFtpClient::onDataReadyRead()
{
    QByteArray data = dataSocket_->readAll();
    qCDebug(LogFtp) << "FTP: Data received:" << data.size() << "bytes";

    FtpResponseContext ctx = buildContext();
    responseHandler_->handleDataReceived(data, ctx);

    // Emit progress signals for RETR
    if (currentCommand_ == Command::Retr) {
        bool isMemory = transferState_.hasPendingRetr() ? transferState_.pendingRetrIsMemory()
                                                        : transferState_.isCurrentRetrMemory();
        QFile *file = transferState_.hasPendingRetr() ? transferState_.pendingRetrFile()
                                                      : transferState_.currentRetrFile().get();

        if (isMemory) {
            emit downloadProgress(currentArg_, transferState_.retrBuffer().size(),
                                  transferState_.transferSize());
        } else if (file) {
            emit downloadProgress(currentArg_, file->size(), transferState_.transferSize());
        }
    }
}

void C64UFtpClient::onDataDisconnected()
{
    qCDebug(LogFtp) << "[DATA] Socket disconnected"
                    << "bytesAvailable:" << dataSocket_->bytesAvailable()
                    << "pendingRetr_ exists:" << transferState_.hasPendingRetr()
                    << "pendingList_ exists:" << transferState_.hasPendingList()
                    << "currentCommand_:" << static_cast<int>(currentCommand_);

    if (dataSocket_->bytesAvailable() > 0) {
        onDataReadyRead();
    }

    FtpResponseContext ctx = buildContext();
    FtpResponseAction action = responseHandler_->handleDataDisconnected(ctx);
    applyAction(action);
}

void C64UFtpClient::onDataError(QAbstractSocket::SocketError socketError)
{
    if (socketError == QAbstractSocket::RemoteHostClosedError) {
        // The server closing the data connection is how every RETR and LIST
        // ends in FTP, so it is not an error here. A transfer that was cut
        // short is reported on the control channel (426/451), which the
        // response handler turns into the operation's error.
        qCDebug(LogFtp) << "FTP: Data socket closed by server, reading remaining data...";
        if (dataSocket_->bytesAvailable() > 0) {
            onDataReadyRead();
        }
        return;
    }
    qCWarning(LogFtp) << "FTP: Data socket error:" << socketError << dataSocket_->errorString();
    if (state_ != State::Busy || repliesToDiscard_ > 0 ||
        !ftp::isDataTransferCommand(currentCommand_)) {
        qCDebug(LogFtp) << "FTP: Ignoring data socket error outside a data transfer";
        return;
    }

    failInFlightTransfer(tr("File transfer interrupted: %1").arg(dataSocket_->errorString()));
}

void C64UFtpClient::failInFlightTransfer(const QString &message)
{
    // Report the failure once, here, and end the operation. The server will
    // still answer the transfer command (typically 425/426); that reply is
    // swallowed so it neither reports a second error nor gets paired with the
    // next command.
    dropRestOfCurrentOperation();
    discardDataTransfer();
    if (awaitingFinalReply_) {
        repliesToDiscard_ = 1;
    }
    emit error(message);
    if (repliesToDiscard_ == 0 && state_ == State::Busy) {
        processNextCommand();
    }
}

void C64UFtpClient::discardReply(int code)
{
    // Preliminary (1xx) replies never complete a command
    if (code < FtpReplyFinalThreshold) {
        return;
    }
    qCDebug(LogFtp) << "FTP: Discarded reply" << code << "belonging to an aborted command";
    if (--repliesToDiscard_ == 0) {
        abortReplyTimer_->stop();
        processNextCommand();
    }
}

void C64UFtpClient::onAbortReplyTimeout()
{
    qCWarning(LogFtp) << "FTP: Gave up waiting for" << repliesToDiscard_
                      << "reply(ies) to ABOR; resuming command queue";
    repliesToDiscard_ = 0;
    processNextCommand();
}

void C64UFtpClient::abortDataConnection()
{
    if (dataSocket_->state() != QAbstractSocket::UnconnectedState) {
        // Block signals so the teardown is not mistaken for a completed transfer
        const QSignalBlocker blocker(dataSocket_);
        dataSocket_->abort();
    }
}

void C64UFtpClient::discardDataTransfer()
{
    abortDataConnection();
    resetTransferState();
    upload_ = UploadProgress{};
}

void C64UFtpClient::resetCommandTracking()
{
    abortReplyTimer_->stop();
    awaitingFinalReply_ = false;
    repliesToDiscard_ = 0;
    currentCommand_ = Command::None;
}

void C64UFtpClient::dropRestOfCurrentOperation()
{
    for (const auto &dropped : commandQueue_.takeOperation(currentOperationId_)) {
        if (dropped.transferFile) {
            dropped.transferFile->close();
        }
    }
}

void C64UFtpClient::drainCommandQueue()
{
    commandQueue_.drain();
}

void C64UFtpClient::resetTransferState()
{
    discardPartialDownloads();
    transferState_.reset();
}

std::shared_ptr<QFile> C64UFtpClient::openPartialDownload(const QString &localPath)
{
    auto file = std::make_shared<QFile>(ftp::partialDownloadPath(localPath));
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(LogFtp) << "FTP: Cannot create" << file->fileName() << file->errorString();
        return nullptr;
    }
    return file;
}

void C64UFtpClient::failDownloadBeforeStart()
{
    const QString message =
        tr("Cannot save file '%1': unable to create local file").arg(currentLocalPath_);
    dropRestOfCurrentOperation();
    abortDataConnection();  // PASV already opened it for this RETR
    emit error(message);
    if (state_ == State::Busy) {
        processNextCommand();
    }
}

QString C64UFtpClient::commitPartialDownload(const QString &localPath)
{
    const QString partPath = ftp::partialDownloadPath(localPath);
    std::error_code errorCode;
    // Atomically replaces any existing file at localPath
    std::filesystem::rename(std::filesystem::path(partPath.toStdU16String()),
                            std::filesystem::path(localPath.toStdU16String()), errorCode);
    if (errorCode) {
        qCWarning(LogFtp) << "FTP: Cannot move" << partPath << "to" << localPath
                          << QString::fromStdString(errorCode.message());
        QFile::remove(partPath);
        return QString::fromStdString(errorCode.message());
    }
    return {};
}

void C64UFtpClient::discardPartialDownloads()
{
    const std::array<QFile *, 2> partFiles = {transferState_.currentRetrFile().get(),
                                              transferState_.pendingRetrFile()};
    for (QFile *file : partFiles) {
        if (file) {
            file->close();
            QFile::remove(file->fileName());
        }
    }
}

void C64UFtpClient::performDisconnectCleanup()
{
    connectionTimer_->stop();
    drainCommandQueue();
    resetTransferState();
    resetCommandTracking();
    loggedIn_ = false;
    setState(State::Disconnected);
}

bool C64UFtpClient::ensureLoggedIn(const QString &operation)
{
    if (!loggedIn_) {
        emit error(tr("Cannot %1: not connected to server").arg(operation));
        return false;
    }
    return true;
}

// ============================================================================
// Public interface methods
// ============================================================================

void C64UFtpClient::list(const QString &path)
{
    if (!ensureLoggedIn(tr("list directory")))
        return;
    const quint64 operationId = beginOperation();
    for (const auto &spec : ftp::buildListSequence(path)) {
        queueCommand(spec.cmd, spec.arg, QString(), operationId);
    }
}

void C64UFtpClient::changeDirectory(const QString &path)
{
    if (!ensureLoggedIn(tr("change directory")))
        return;
    queueCommand(Command::Cwd, path, QString(), beginOperation());
}

void C64UFtpClient::makeDirectory(const QString &path)
{
    if (!ensureLoggedIn(tr("create directory")))
        return;
    queueCommand(Command::Mkd, path, QString(), beginOperation());
}

void C64UFtpClient::removeDirectory(const QString &path)
{
    if (!ensureLoggedIn(tr("remove directory")))
        return;
    queueCommand(Command::Rmd, path, QString(), beginOperation());
}

void C64UFtpClient::download(const QString &remotePath, const QString &localPath)
{
    if (!ensureLoggedIn(tr("download file")))
        return;

    // The destination is written via a ".part" file opened when the transfer
    // starts, so queuing (or failing) a download never touches an existing file.
    const quint64 operationId = beginOperation();
    for (const auto &spec : ftp::buildDownloadPrelude()) {
        queueCommand(spec.cmd, spec.arg, QString(), operationId);
    }
    queueRetrCommand(remotePath, localPath, nullptr, false, operationId);
}

void C64UFtpClient::downloadToMemory(const QString &remotePath)
{
    if (!ensureLoggedIn(tr("download file")))
        return;

    const quint64 operationId = beginOperation();
    for (const auto &spec : ftp::buildDownloadPrelude()) {
        queueCommand(spec.cmd, spec.arg, QString(), operationId);
    }
    queueRetrCommand(remotePath, QString(), nullptr, true, operationId);
}

void C64UFtpClient::upload(const QString &localPath, const QString &remotePath)
{
    if (!ensureLoggedIn(tr("upload file")))
        return;

    auto file = std::make_shared<QFile>(localPath);
    if (!file->open(QIODevice::ReadOnly)) {
        emit error(tr("Cannot read file '%1': file not found or access denied").arg(localPath));
        return;
    }

    const quint64 operationId = beginOperation();
    for (const auto &spec : ftp::buildUploadPrelude()) {
        queueCommand(spec.cmd, spec.arg, QString(), operationId);
    }
    queueStorCommand(remotePath, localPath, std::move(file), operationId);
}

void C64UFtpClient::remove(const QString &path)
{
    if (!ensureLoggedIn(tr("delete file")))
        return;
    queueCommand(Command::Dele, path, QString(), beginOperation());
}

void C64UFtpClient::rename(const QString &oldPath, const QString &newPath)
{
    if (!ensureLoggedIn(tr("rename file")))
        return;
    const quint64 operationId = beginOperation();
    queueCommand(Command::RnFr, oldPath, oldPath, operationId);  // Store oldPath for signal
    queueCommand(Command::RnTo, newPath, QString(), operationId);
}

void C64UFtpClient::abort()
{
    if (!loggedIn_) {
        // Nothing can be in flight before login completes; changing state here
        // would make a disconnected client look connected and block reconnects.
        qCDebug(LogFtp) << "FTP: abort() ignored — not logged in";
        return;
    }

    if (state_ != State::Busy || repliesToDiscard_ > 0) {
        qCDebug(LogFtp) << "FTP: abort() ignored — nothing in flight";
        return;
    }

    if (ftp::isTransferPreludeCommand(currentCommand_)) {
        // TYPE/PASV cannot be aborted on the wire. Swallow the reply that is on
        // its way so the transfer it prepares is never started.
        dropRestOfCurrentOperation();
        if (awaitingFinalReply_) {
            repliesToDiscard_ = 1;
        } else {
            processNextCommand();
        }
        return;
    }

    if (!ftp::isDataTransferCommand(currentCommand_)) {
        // Control-only commands (CWD, MKD, DELE, ...) complete on their own
        qCDebug(LogFtp) << "FTP: abort() ignored — no data transfer in flight";
        return;
    }

    dropRestOfCurrentOperation();
    discardDataTransfer();

    if (!awaitingFinalReply_) {
        // The server already reported completion; only the data close was pending
        processNextCommand();
        return;
    }

    // Expect the aborted command's final reply (e.g. 426) plus the reply to ABOR
    // (225/226). Servers that send only one are covered by the timeout.
    repliesToDiscard_ = 2;
    abortReplyTimer_->start(AbortReplyTimeoutMs);
    sendCommand("ABOR");
}
