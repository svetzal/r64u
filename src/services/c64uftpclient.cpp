#include "c64uftpclient.h"

#include "ftpcore.h"

#include <QDebug>
#include <QFileInfo>
#include <QRegularExpression>

C64UFtpClient::C64UFtpClient(QObject *parent)
    : IFtpClient(parent), controlSocket_(new QTcpSocket(this)), dataSocket_(new QTcpSocket(this)),
      connectionTimer_(new QTimer(this))
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
    // Clean up any file handles in queued commands
    drainCommandQueue();

    // Close all transfer file handles
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

    // Start connection timeout timer
    connectionTimer_->start(ConnectionTimeoutMs);

    // Defer the actual socket connect to the event loop so callers always observe
    // the Connecting state before any error fires.  Qt 6.10+ (macOS) can emit
    // errorOccurred synchronously inside connectToHost() for immediately-unreachable
    // hosts, which would collapse Disconnected→Connecting→Disconnected within a
    // single call frame and break the observable state machine contract.
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
    // Don't log password
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
        // Set the current RETR state from the queued command
        // This ensures we have the correct file even if other operations were queued
        transferState_.setCurrentRetrFile(std::move(pending.transferFile),
                                          pending.isMemoryDownload);
        qDebug() << "FTP: Processing RETR, file:" << transferState_.currentRetrFile().get()
                 << "isMemory:" << transferState_.isCurrentRetrMemory();
        sendCommand("RETR " + currentArg_);
        break;
    case Command::Stor:
        // Extract the file handle from the queued command, similar to RETR
        // This prevents corruption when another upload is queued during signal handling
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

void C64UFtpClient::onControlConnected()
{
    qDebug() << "FTP: Control socket connected to" << controlSocket_->peerAddress().toString();

    // Stop connection timeout timer
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

    // FTP responses end with \r\n; multi-line continuations are filtered out
    auto parsed = ftp::splitResponseLines(responseBuffer_);
    responseBuffer_ = parsed.remainingBuffer;

    for (const auto &line : parsed.lines) {
        handleResponse(line.code, line.text);
    }
}

void C64UFtpClient::handleResponse(int code, const QString &text)
{
    qDebug() << "FTP: <<" << code << text << "(state:" << static_cast<int>(state_) << ")";

    // 1xx: Positive Preliminary
    // 2xx: Positive Completion
    // 3xx: Positive Intermediate
    // 4xx: Transient Negative
    // 5xx: Permanent Negative

    switch (state_) {
    case State::Connected:
        // Welcome message
        if (code == FtpReplyServiceReady) {
            setState(State::LoggingIn);
            queueCommand(Command::User);
            processNextCommand();
        }
        break;

    case State::LoggingIn:
        // Login responses are handled in handleBusyResponse since
        // processNextCommand() transitions to Busy before responses arrive
        processNextCommand();
        break;

    case State::Busy:
        handleBusyResponse(code, text);
        break;

    default:
        break;
    }
}

void C64UFtpClient::handleBusyResponse(int code, const QString &text)
{
    switch (currentCommand_) {
    case Command::User:
        if (code == FtpReplyPasswordRequired) {
            // Password required
            queueCommand(Command::Pass);
        } else if (code == FtpReplyUserLoggedIn) {
            // Logged in without password
            loggedIn_ = true;
            setState(State::Ready);
            emit connected();
        } else {
            emit error(tr("Login failed: server rejected username. %1").arg(text));
            disconnect();
            return;
        }
        processNextCommand();
        break;

    case Command::Pass:
        if (code == FtpReplyUserLoggedIn) {
            loggedIn_ = true;
            setState(State::Ready);
            emit connected();
        } else {
            emit error(tr("Login failed: invalid password. %1").arg(text));
            disconnect();
            return;
        }
        processNextCommand();
        break;

    case Command::Pwd:
        if (code == FtpReplyPathCreated) {
            // Extract path from response like: 257 "/path" is current directory
            if (auto path = ftp::parsePwdResponse(text)) {
                currentDir_ = *path;
            }
        }
        processNextCommand();
        break;

    case Command::Cwd:
        if (code == FtpReplyActionOk) {
            currentDir_ = currentArg_;
            emit directoryChanged(currentDir_);
        } else {
            emit error(tr("Cannot access directory '%1': %2").arg(currentArg_, text));
        }
        processNextCommand();
        break;

    case Command::Type:
        processNextCommand();
        break;

    case Command::Pasv:
        if (code == FtpReplyEnteringPassive) {
            auto passiveResult = ftp::parsePassiveResponse(text);
            if (passiveResult) {
                // Use the control socket's peer address instead of the IP from PASV
                // Many FTP servers return internal IPs that aren't reachable
                QString actualHost = controlSocket_->peerAddress().toString();
                qDebug() << "FTP: PASV response host:" << passiveResult->host
                         << "port:" << passiveResult->port;
                qDebug() << "FTP: Using actual host:" << actualHost
                         << "port:" << passiveResult->port;
                dataSocket_->connectToHost(actualHost, passiveResult->port);
                // Send the next command (LIST/RETR/STOR) immediately
                // The server expects the command before sending data
                processNextCommand();
            } else {
                emit error(tr("Data transfer failed: unable to establish data connection"));
                processNextCommand();
            }
        } else {
            emit error(
                tr("Data transfer failed: server does not support passive mode. %1").arg(text));
            processNextCommand();
        }
        break;

    case Command::List:
        if (code == FtpReplyFileStatusOk || code == FtpReplyDataConnectionOpen) {
            // Transfer starting - don't clear buffer here as data may have already arrived
            // (C64U server sometimes sends data before 150 response)
            transferState_.setDownloading(true);
            qDebug() << "FTP: 150 received, listBuffer_ size:"
                     << transferState_.listBuffer().size();
        } else if (code == FtpReplyTransferComplete) {
            // Transfer complete on server side, but data may still be in flight
            QString path = currentArg_.isEmpty() ? currentDir_ : currentArg_;
            if (dataSocket_->state() == QAbstractSocket::UnconnectedState) {
                // Data socket already closed, process immediately
                qDebug() << "FTP: 226 received, data socket already closed, processing";
                qDebug() << "FTP: LIST complete, total data:" << transferState_.listBuffer().size()
                         << "bytes";
                QList<FtpEntry> entries = ftp::parseDirectoryListing(transferState_.listBuffer());
                qDebug() << "FTP: Parsed" << entries.size() << "entries";
                transferState_.clearListBuffer();
                emit directoryListed(path, entries);
                processNextCommand();
            } else {
                // Wait for data socket to close before processing
                // Save the buffer with the pending state to prevent race conditions
                // if another list() call clears listBuffer_ while we're waiting
                qDebug() << "FTP: 226 received, waiting for data socket to finish, saving"
                         << transferState_.listBuffer().size() << "bytes";
                transferState_.savePendingList(path, transferState_.listBuffer());
                transferState_.clearListBuffer();
                // Don't process next command yet - wait for data socket
            }
        } else if (code >= FtpReplyErrorThreshold) {
            emit error(tr("Cannot list directory contents: %1").arg(text));
            processNextCommand();
        }
        break;

    case Command::Retr:
        if (code == FtpReplyFileStatusOk || code == FtpReplyDataConnectionOpen) {
            // Transfer starting
            transferState_.setDownloading(true);
            // Parse size from response if available
            QRegularExpression rx("\\((\\d+)\\s+bytes\\)");
            auto match = rx.match(text);
            if (match.hasMatch()) {
                transferState_.setTransferSize(match.captured(1).toLongLong());
            }
        } else if (code == FtpReplyTransferComplete) {
            // Transfer complete on server side, but data may still be in flight
            // Use transferState_ current file/memory flag set when RETR was dequeued
            qDebug() << "[RETR] 226 received for" << currentArg_
                     << "dataSocket state:" << dataSocket_->state()
                     << "bytesAvailable:" << dataSocket_->bytesAvailable()
                     << "pendingRetr_ exists:" << transferState_.hasPendingRetr()
                     << "isMemory:" << transferState_.isCurrentRetrMemory()
                     << "file:" << transferState_.currentRetrFile().get();
            if (dataSocket_->state() == QAbstractSocket::UnconnectedState) {
                // Data socket already closed, process immediately
                qDebug() << "[RETR] Data socket already closed, processing immediately";
                if (transferState_.isCurrentRetrMemory()) {
                    emit downloadToMemoryFinished(currentArg_, transferState_.retrBuffer());
                    transferState_.clearRetrBuffer();
                } else if (transferState_.currentRetrFile()) {
                    transferState_.currentRetrFile()->close();
                    emit downloadFinished(currentArg_, currentLocalPath_);
                } else {
                    qDebug() << "FTP: ERROR - RETR 226 but no file handle!";
                }
                transferState_.clearCurrentRetrFile();
                processNextCommand();
            } else {
                // Wait for data socket to close before processing
                qDebug() << "[RETR] Creating pendingRetr_, waiting for data socket to finish"
                         << "socketState:" << dataSocket_->state();
                transferState_.savePendingRetr(currentArg_, currentLocalPath_,
                                               transferState_.currentRetrFile(),
                                               transferState_.isCurrentRetrMemory());
                // Clear current state (saved in pending struct)
                // Use reset variant that does NOT close the file (file is now owned by pending)
                transferState_.setCurrentRetrFile(nullptr, false);

                // CRITICAL: Check if socket is ALREADY disconnected (race condition)
                if (dataSocket_->state() == QAbstractSocket::UnconnectedState) {
                    qCritical() << "[RETR] BUG: Socket already disconnected but we just created "
                                   "pendingRetr_!";
                }
                // Don't process next command yet - wait for data socket
            }
        } else if (code >= FtpReplyErrorThreshold) {
            emit error(tr("Download failed for '%1': %2").arg(currentArg_, text));
            if (transferState_.isCurrentRetrMemory()) {
                transferState_.clearRetrBuffer();
            }
            transferState_.clearCurrentRetrFile();
            processNextCommand();
        }
        break;

    case Command::Stor:
        if (code == FtpReplyFileStatusOk || code == FtpReplyDataConnectionOpen) {
            // Ready to receive data, start sending
            // Use currentStorFile which was extracted from the queued command
            // to prevent corruption when another upload is queued during signal handling
            auto storFile = transferState_.currentStorFile();
            if (storFile && storFile->isOpen()) {
                QByteArray data = storFile->readAll();
                dataSocket_->write(data);
                dataSocket_->disconnectFromHost();
            } else {
                qDebug() << "FTP: ERROR - STOR 150 but no file handle! currentStorFile:"
                         << storFile.get();
            }
        } else if (code == FtpReplyTransferComplete) {
            // Transfer complete - close and reset BEFORE emitting signal
            // to prevent the signal handler's upload() call from being affected
            QString localPath = currentLocalPath_;
            QString remotePath = currentArg_;
            transferState_.clearCurrentStorFile();
            emit uploadFinished(localPath, remotePath);
            processNextCommand();
        } else if (code >= FtpReplyErrorThreshold) {
            emit error(tr("Upload failed for '%1': %2").arg(currentArg_, text));
            transferState_.clearCurrentStorFile();
            processNextCommand();
        }
        break;

    case Command::Mkd:
        qDebug() << "FTP: MKD response code:" << code << "for" << currentArg_
                 << "(257=created, 553=exists)";
        if (code == FtpReplyPathCreated || code == FtpReplyFileExists) {
            // 257 = created, 553 = already exists (both are success for our purposes)
            qDebug() << "FTP: MKD success - emitting directoryCreated for" << currentArg_;
            emit directoryCreated(currentArg_);
        } else {
            qDebug() << "FTP: MKD failure - emitting error for" << currentArg_;
            emit error(tr("Cannot create directory '%1': %2").arg(currentArg_, text));
        }
        processNextCommand();
        break;

    case Command::Rmd:
    case Command::Dele:
        if (code == FtpReplyActionOk) {
            emit fileRemoved(currentArg_);
        } else {
            emit error(tr("Cannot delete '%1': %2").arg(currentArg_, text));
        }
        processNextCommand();
        break;

    case Command::RnFr:
        if (code == FtpReplyPendingFurtherInfo) {
            // Ready for RNTO
        } else {
            emit error(tr("Cannot rename '%1': file not found or access denied. %2")
                           .arg(currentArg_, text));
        }
        processNextCommand();
        break;

    case Command::RnTo:
        if (code == FtpReplyActionOk) {
            emit fileRenamed(currentLocalPath_, currentArg_);  // oldPath stored in localPath
        } else {
            emit error(tr("Cannot rename to '%1': %2").arg(currentArg_, text));
        }
        processNextCommand();
        break;

    default:
        processNextCommand();
        break;
    }
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

void C64UFtpClient::onDataConnected()
{
    qDebug() << "FTP: Data socket connected to" << dataSocket_->peerAddress().toString() << ":"
             << dataSocket_->peerPort();
    // Data connection established, now send the actual command
    // The command was already sent, waiting for data
}

void C64UFtpClient::onDataReadyRead()
{
    QByteArray data = dataSocket_->readAll();
    qDebug() << "FTP: Data received:" << data.size() << "bytes";

    if (currentCommand_ == Command::List) {
        // If we have a pending LIST (226 arrived but socket still open), append to its buffer
        if (transferState_.hasPendingList()) {
            transferState_.appendToPendingList(data);
        } else {
            transferState_.appendListData(data);
        }
    } else if (currentCommand_ == Command::Retr) {
        // Check pending state first (226 may have arrived but data still coming)
        // then fall back to current RETR state (set when RETR was dequeued)
        bool isMemory = transferState_.hasPendingRetr() ? transferState_.pendingRetrIsMemory()
                                                        : transferState_.isCurrentRetrMemory();
        QFile *file = transferState_.hasPendingRetr() ? transferState_.pendingRetrFile()
                                                      : transferState_.currentRetrFile().get();

        if (isMemory) {
            transferState_.appendRetrData(data);
            emit downloadProgress(currentArg_, transferState_.retrBuffer().size(),
                                  transferState_.transferSize());
        } else if (file) {
            file->write(data);
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
    // Read any remaining data before disconnect completes
    if (dataSocket_->bytesAvailable() > 0) {
        onDataReadyRead();
    }
    transferState_.setDownloading(false);

    // If we received 226 and were waiting for data socket to close, process now
    if (transferState_.hasPendingList()) {
        // Use the saved buffer from pendingList_, not listBuffer_
        // (which may have been cleared by a new list() call)
        // Extract both values before any further calls that could invalidate the optional
        auto pending = transferState_.takePendingList();
        qDebug() << "FTP: Processing pending LIST, total data:" << pending->buffer.size()
                 << "bytes";
        QList<FtpEntry> entries = ftp::parseDirectoryListing(pending->buffer);
        qDebug() << "FTP: Parsed" << entries.size() << "entries";
        emit directoryListed(pending->path, entries);
        processNextCommand();
    } else if (transferState_.hasPendingRetr()) {
        auto pending = transferState_.takePendingRetr();
        qDebug() << "[RETR] Processing pending RETR for" << pending->remotePath
                 << "localPath:" << pending->localPath << "isMemory:" << pending->isMemory
                 << "file:" << pending->file.get();
        // Use the SAVED state, not the current global state (which may have been
        // corrupted by other operations like downloadToMemory for file preview)
        if (pending->isMemory) {
            emit downloadToMemoryFinished(pending->remotePath, transferState_.retrBuffer());
            transferState_.clearRetrBuffer();
        } else if (pending->file) {
            pending->file->close();
            emit downloadFinished(pending->remotePath, pending->localPath);
        } else {
            qDebug() << "FTP: ERROR - pending RETR has no file handle!";
        }
        processNextCommand();
    }
}

void C64UFtpClient::onDataError(QAbstractSocket::SocketError socketError)
{
    // RemoteHostClosedError is normal - server closes after sending data
    // But we need to read any remaining data in the buffer first
    if (socketError == QAbstractSocket::RemoteHostClosedError) {
        qDebug() << "FTP: Data socket closed by server, reading remaining data...";
        if (dataSocket_->bytesAvailable() > 0) {
            onDataReadyRead();  // Read any remaining buffered data
        }
        return;
    }
    qDebug() << "FTP: Data socket error:" << socketError << dataSocket_->errorString();
    emit error(tr("File transfer interrupted: %1").arg(dataSocket_->errorString()));
}

// Public interface methods

void C64UFtpClient::list(const QString &path)
{
    if (!loggedIn_) {
        emit error(tr("Cannot list directory: not connected to server"));
        return;
    }
    // Don't clear listBuffer_ here - it would wipe data from an in-flight LIST.
    // The buffer is properly cleared after processing each LIST response:
    // - In handleBusyResponse after parsing (226 with socket closed)
    // - When saving to pendingList_ (226 with socket still open)
    queueCommand(Command::Type, "A");  // ASCII mode for listing
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

    // Create the file now, but pass ownership to the queued command
    // This way the file handle stays with the RETR command even if
    // other operations are queued before it executes
    auto file = std::make_shared<QFile>(localPath);
    if (!file->open(QIODevice::WriteOnly)) {
        emit error(tr("Cannot save file '%1': unable to create local file").arg(localPath));
        return;
    }

    transferState_.setTransferSize(0);
    queueCommand(Command::Type, "I");  // Binary mode
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
    queueCommand(Command::Type, "I");  // Binary mode
    queueCommand(Command::Pasv);
    queueRetrCommand(remotePath, QString(), nullptr, true);  // nullptr becomes empty unique_ptr
}

void C64UFtpClient::upload(const QString &localPath, const QString &remotePath)
{
    if (!loggedIn_) {
        emit error(tr("Cannot upload file: not connected to server"));
        return;
    }

    // Create file handle and store it in the queued command, not as a global member
    // This prevents corruption when another upload is queued during signal handling
    auto file = std::make_shared<QFile>(localPath);
    if (!file->open(QIODevice::ReadOnly)) {
        emit error(tr("Cannot read file '%1': file not found or access denied").arg(localPath));
        return;
    }

    transferState_.setTransferSize(file->size());
    queueCommand(Command::Type, "I");  // Binary mode
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
    // Clean up any file handles in queued commands
    drainCommandQueue();

    if (dataSocket_->state() != QAbstractSocket::UnconnectedState) {
        dataSocket_->abort();
    }

    // Clear all transfer state (buffers, file handles, pending state)
    resetTransferState();

    sendCommand("ABOR");
    setState(State::Ready);
}
