#include "c64uftpclient.h"

#include "ftpcore.h"
#include "ftpresponsehandler.h"

#include <QDebug>
#include <QFileInfo>
#include <QRegularExpression>

C64UFtpClient::C64UFtpClient(QObject *parent)
    : IFtpClient(parent), controlSocket_(new QTcpSocket(this)), dataSocket_(new QTcpSocket(this)),
      connectionTimer_(new QTimer(this)),
      responseHandler_(new FtpResponseHandler(transferState_, this))
{
    // Connection timeout timer
    connectionTimer_->setSingleShot(true);
    connect(connectionTimer_, &QTimer::timeout, this, &C64UFtpClient::onConnectionTimeout);

    connect(controlSocket_, &QTcpSocket::connected, this, &C64UFtpClient::onControlConnected);
    connect(controlSocket_, &QTcpSocket::disconnected, this, &C64UFtpClient::onControlDisconnected);
    connect(controlSocket_, &QTcpSocket::readyRead, this, &C64UFtpClient::onControlReadyRead);
    connect(controlSocket_, &QTcpSocket::errorOccurred, this, &C64UFtpClient::onControlError);

    connect(dataSocket_, &QTcpSocket::connected, this, &C64UFtpClient::onDataConnected);
    connect(dataSocket_, &QTcpSocket::readyRead, this, &C64UFtpClient::onDataReadyRead);
    connect(dataSocket_, &QTcpSocket::disconnected, this, &C64UFtpClient::onDataDisconnected);
    connect(dataSocket_, &QTcpSocket::errorOccurred, this, &C64UFtpClient::onDataError);
}

C64UFtpClient::~C64UFtpClient()
{
    drainCommandQueue();
    transferState_.reset();
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
        qDebug() << "FTP: connectToHost called but state is" << static_cast<int>(state_);
        emit error(tr("Cannot connect: connection already in progress or established"));
        return;
    }

    qDebug() << "FTP: Connecting to" << host_ << ":" << port_;
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
        return;
    }

    commandQueue_.drain();
    loggedIn_ = false;

    if (dataSocket_->state() != QAbstractSocket::UnconnectedState) {
        dataSocket_->abort();
    }

    if (controlSocket_->state() != QAbstractSocket::UnconnectedState) {
        sendCommand("QUIT");
        controlSocket_->disconnectFromHost();
    }

    setState(State::Disconnected);
}

void C64UFtpClient::sendCommand(const QString &command)
{
    if (controlSocket_->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "FTP: Cannot send command, socket not connected";
        return;
    }
    if (command.startsWith("PASS ")) {
        qDebug() << "FTP: >>" << "PASS ****";
    } else {
        qDebug() << "FTP: >>" << command;
    }
    controlSocket_->write((command + "\r\n").toUtf8());
}

void C64UFtpClient::queueCommand(Command cmd, const QString &arg, const QString &localPath)
{
    commandQueue_.enqueue(cmd, arg, localPath);

    if (state_ == State::Ready) {
        processNextCommand();
    }
}

void C64UFtpClient::queueRetrCommand(const QString &remotePath, const QString &localPath,
                                     std::shared_ptr<QFile> file, bool isMemory)
{
    commandQueue_.enqueueRetr(remotePath, localPath, std::move(file), isMemory);

    if (state_ == State::Ready) {
        processNextCommand();
    }
}

void C64UFtpClient::queueStorCommand(const QString &remotePath, const QString &localPath,
                                     std::shared_ptr<QFile> file)
{
    commandQueue_.enqueueStor(remotePath, localPath, std::move(file));

    if (state_ == State::Ready) {
        processNextCommand();
    }
}

void C64UFtpClient::processNextCommand()
{
    if (commandQueue_.isEmpty()) {
        setState(State::Ready);
        return;
    }

    setState(State::Busy);
    PendingCommand pending = commandQueue_.dequeueNext();
    currentCommand_ = pending.cmd;
    currentArg_ = pending.arg;
    currentLocalPath_ = pending.localPath;

    switch (currentCommand_) {
    case Command::User:
        sendCommand("USER " + user_);
        break;
    case Command::Pass:
        sendCommand("PASS " + password_);
        break;
    case Command::Pwd:
        sendCommand("PWD");
        break;
    case Command::Cwd:
        sendCommand("CWD " + currentArg_);
        break;
    case Command::Type:
        sendCommand("TYPE " + currentArg_);
        break;
    case Command::Pasv:
        sendCommand("PASV");
        break;
    case Command::List:
        sendCommand("LIST" + (currentArg_.isEmpty() ? "" : " " + currentArg_));
        break;
    case Command::Retr:
        transferState_.setCurrentRetrFile(std::move(pending.transferFile),
                                          pending.isMemoryDownload);
        qDebug() << "FTP: Processing RETR, file:" << transferState_.currentRetrFile().get()
                 << "isMemory:" << transferState_.isCurrentRetrMemory();
        sendCommand("RETR " + currentArg_);
        break;
    case Command::Stor:
        transferState_.setCurrentStorFile(std::move(pending.transferFile));
        qDebug() << "FTP: Processing STOR, file:" << transferState_.currentStorFile().get();
        sendCommand("STOR " + currentArg_);
        break;
    case Command::Mkd:
        sendCommand("MKD " + currentArg_);
        break;
    case Command::Rmd:
        sendCommand("RMD " + currentArg_);
        break;
    case Command::Dele:
        sendCommand("DELE " + currentArg_);
        break;
    case Command::RnFr:
        sendCommand("RNFR " + currentArg_);
        break;
    case Command::RnTo:
        sendCommand("RNTO " + currentArg_);
        break;
    case Command::Quit:
        sendCommand("QUIT");
        break;
    default:
        processNextCommand();
        break;
    }
}

FtpResponseContext C64UFtpClient::buildContext() const
{
    return {currentCommand_, currentArg_, currentLocalPath_,   state_,
            loggedIn_,       currentDir_, dataSocket_->state()};
}

void C64UFtpClient::applyAction(const FtpResponseAction &action)
{
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
        emit downloadFinished(action.downloadFinishedRemotePath, action.downloadFinishedLocalPath);
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

void C64UFtpClient::executeResponseAction(const FtpResponseAction &action)
{
    // STOR 150/125: send file data via data socket
    if (action.kind == FtpResponseAction::Kind::None && currentCommand_ == Command::Stor &&
        action.uploadFinishedLocalPath.isEmpty() && action.errorMessage.isEmpty() &&
        !action.clearCurrentStorFile) {
        auto storFile = transferState_.currentStorFile();
        if (storFile && storFile->isOpen()) {
            QByteArray data = storFile->readAll();
            dataSocket_->write(data);
            dataSocket_->disconnectFromHost();
        } else {
            qDebug() << "FTP: ERROR - STOR 150 but no file handle! currentStorFile:"
                     << storFile.get();
        }
    }

    switch (action.kind) {
    case FtpResponseAction::Kind::ProcessNext:
        processNextCommand();
        break;

    case FtpResponseAction::Kind::ProcessNextAndConnect: {
        QString actualHost = controlSocket_->peerAddress().toString();
        qDebug() << "FTP: PASV response host:" << action.dataHost << "port:" << action.dataPort;
        qDebug() << "FTP: Using actual host:" << actualHost << "port:" << action.dataPort;
        dataSocket_->connectToHost(actualHost, action.dataPort);
        processNextCommand();
        break;
    }

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
    qDebug() << "FTP: Control socket connected to" << controlSocket_->peerAddress().toString();
    connectionTimer_->stop();
    setState(State::Connected);
}

void C64UFtpClient::onControlDisconnected()
{
    qDebug() << "FTP: Control socket disconnected";
    performDisconnectCleanup();
    emit disconnected();
}

void C64UFtpClient::onControlError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError)
    qDebug() << "FTP: Control socket error:" << socketError << controlSocket_->errorString();
    QString errorMessage = controlSocket_->errorString();
    performDisconnectCleanup();
    emit error(errorMessage);
}

void C64UFtpClient::onConnectionTimeout()
{
    qDebug() << "FTP: Connection timeout";
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
        FtpResponseContext ctx = buildContext();
        FtpResponseAction action;

        qDebug() << "FTP: <<" << line.code << line.text << "(state:" << static_cast<int>(state_)
                 << ")";

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
    qDebug() << "FTP: Data socket connected to" << dataSocket_->peerAddress().toString() << ":"
             << dataSocket_->peerPort();
}

void C64UFtpClient::onDataReadyRead()
{
    QByteArray data = dataSocket_->readAll();
    qDebug() << "FTP: Data received:" << data.size() << "bytes";

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
    qDebug() << "[DATA] Socket disconnected"
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
        qDebug() << "FTP: Data socket closed by server, reading remaining data...";
        if (dataSocket_->bytesAvailable() > 0) {
            onDataReadyRead();
        }
        return;
    }
    qDebug() << "FTP: Data socket error:" << socketError << dataSocket_->errorString();
    emit error(tr("File transfer interrupted: %1").arg(dataSocket_->errorString()));
}

void C64UFtpClient::drainCommandQueue()
{
    commandQueue_.drain();
}

void C64UFtpClient::resetTransferState()
{
    transferState_.reset();
}

void C64UFtpClient::performDisconnectCleanup()
{
    connectionTimer_->stop();
    drainCommandQueue();
    resetTransferState();
    loggedIn_ = false;
    setState(State::Disconnected);
}

// ============================================================================
// Public interface methods
// ============================================================================

void C64UFtpClient::list(const QString &path)
{
    if (!loggedIn_) {
        emit error(tr("Cannot list directory: not connected to server"));
        return;
    }
    queueCommand(Command::Type, "A");
    queueCommand(Command::Pasv);
    queueCommand(Command::List, path);
}

void C64UFtpClient::changeDirectory(const QString &path)
{
    if (!loggedIn_) {
        emit error(tr("Cannot change directory: not connected to server"));
        return;
    }
    queueCommand(Command::Cwd, path);
}

void C64UFtpClient::makeDirectory(const QString &path)
{
    if (!loggedIn_) {
        emit error(tr("Cannot create directory: not connected to server"));
        return;
    }
    queueCommand(Command::Mkd, path);
}

void C64UFtpClient::removeDirectory(const QString &path)
{
    if (!loggedIn_) {
        emit error(tr("Cannot remove directory: not connected to server"));
        return;
    }
    queueCommand(Command::Rmd, path);
}

void C64UFtpClient::download(const QString &remotePath, const QString &localPath)
{
    if (!loggedIn_) {
        emit error(tr("Cannot download file: not connected to server"));
        return;
    }

    auto file = std::make_shared<QFile>(localPath);
    if (!file->open(QIODevice::WriteOnly)) {
        emit error(tr("Cannot save file '%1': unable to create local file").arg(localPath));
        return;
    }

    transferState_.setTransferSize(0);
    queueCommand(Command::Type, "I");
    queueCommand(Command::Pasv);
    queueRetrCommand(remotePath, localPath, std::move(file), false);
}

void C64UFtpClient::downloadToMemory(const QString &remotePath)
{
    if (!loggedIn_) {
        emit error(tr("Cannot download file: not connected to server"));
        return;
    }

    transferState_.clearRetrBuffer();
    transferState_.setTransferSize(0);
    queueCommand(Command::Type, "I");
    queueCommand(Command::Pasv);
    queueRetrCommand(remotePath, QString(), nullptr, true);
}

void C64UFtpClient::upload(const QString &localPath, const QString &remotePath)
{
    if (!loggedIn_) {
        emit error(tr("Cannot upload file: not connected to server"));
        return;
    }

    auto file = std::make_shared<QFile>(localPath);
    if (!file->open(QIODevice::ReadOnly)) {
        emit error(tr("Cannot read file '%1': file not found or access denied").arg(localPath));
        return;
    }

    transferState_.setTransferSize(file->size());
    queueCommand(Command::Type, "I");
    queueCommand(Command::Pasv);
    queueStorCommand(remotePath, localPath, std::move(file));
}

void C64UFtpClient::remove(const QString &path)
{
    if (!loggedIn_) {
        emit error(tr("Cannot delete file: not connected to server"));
        return;
    }
    queueCommand(Command::Dele, path);
}

void C64UFtpClient::rename(const QString &oldPath, const QString &newPath)
{
    if (!loggedIn_) {
        emit error(tr("Cannot rename file: not connected to server"));
        return;
    }
    queueCommand(Command::RnFr, oldPath, oldPath);  // Store oldPath for signal
    queueCommand(Command::RnTo, newPath);
}

void C64UFtpClient::abort()
{
    drainCommandQueue();

    if (dataSocket_->state() != QAbstractSocket::UnconnectedState) {
        dataSocket_->abort();
    }

    resetTransferState();

    sendCommand("ABOR");
    setState(State::Ready);
}
