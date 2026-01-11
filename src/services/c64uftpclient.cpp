#include "c64uftpclient.h"

#include <QRegularExpression>
#include <QFileInfo>
#include <QDebug>

C64UFtpClient::C64UFtpClient(QObject *parent)
    : QObject(parent)
    , controlSocket_(new QTcpSocket(this))
    , dataSocket_(new QTcpSocket(this))
    , connectionTimer_(new QTimer(this))
{
    // Connection timeout timer
    connectionTimer_->setSingleShot(true);
    connect(connectionTimer_, &QTimer::timeout,
            this, &C64UFtpClient::onConnectionTimeout);

    connect(controlSocket_, &QTcpSocket::connected,
            this, &C64UFtpClient::onControlConnected);
    connect(controlSocket_, &QTcpSocket::disconnected,
            this, &C64UFtpClient::onControlDisconnected);
    connect(controlSocket_, &QTcpSocket::readyRead,
            this, &C64UFtpClient::onControlReadyRead);
    connect(controlSocket_, &QTcpSocket::errorOccurred,
            this, &C64UFtpClient::onControlError);

    connect(dataSocket_, &QTcpSocket::connected,
            this, &C64UFtpClient::onDataConnected);
    connect(dataSocket_, &QTcpSocket::readyRead,
            this, &C64UFtpClient::onDataReadyRead);
    connect(dataSocket_, &QTcpSocket::disconnected,
            this, &C64UFtpClient::onDataDisconnected);
    connect(dataSocket_, &QTcpSocket::errorOccurred,
            this, &C64UFtpClient::onDataError);
}

C64UFtpClient::~C64UFtpClient()
{
    // Clean up any file handles in queued commands
    // unique_ptr handles cleanup automatically when commandQueue_ is cleared
    while (!commandQueue_.isEmpty()) {
        PendingCommand cmd = std::move(commandQueue_.head());
        commandQueue_.dequeue();
        if (cmd.transferFile) {
            cmd.transferFile->close();
        }
        // unique_ptr automatically deletes when cmd goes out of scope
    }

    // Close files - unique_ptr handles deletion automatically
    if (transferFile_) {
        transferFile_->close();
    }

    if (currentRetrFile_) {
        currentRetrFile_->close();
    }

    if (pendingRetr_ && pendingRetr_->file) {
        pendingRetr_->file->close();
    }
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

    controlSocket_->connectToHost(host_, port_);
}

void C64UFtpClient::disconnect()
{
    if (state_ == State::Disconnected) {
        return;
    }

    commandQueue_.clear();
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
    PendingCommand pending;
    pending.cmd = cmd;
    pending.arg = arg;
    pending.localPath = localPath;
    commandQueue_.enqueue(pending);

    if (state_ == State::Ready) {
        processNextCommand();
    }
}

void C64UFtpClient::queueRetrCommand(const QString &remotePath, const QString &localPath,
                                      std::shared_ptr<QFile> file, bool isMemory)
{
    PendingCommand pending;
    pending.cmd = Command::Retr;
    pending.arg = remotePath;
    pending.localPath = localPath;
    pending.transferFile = std::move(file);
    pending.isMemoryDownload = isMemory;
    commandQueue_.enqueue(std::move(pending));

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
    PendingCommand pending = std::move(commandQueue_.head());
    commandQueue_.dequeue();
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
        currentRetrFile_ = std::move(pending.transferFile);
        currentRetrIsMemory_ = pending.isMemoryDownload;
        qDebug() << "FTP: Processing RETR, file:" << currentRetrFile_.get() << "isMemory:" << currentRetrIsMemory_;
        sendCommand("RETR " + currentArg_);
        break;
    case Command::Stor:
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

    // Stop connection timeout timer
    connectionTimer_->stop();

    // Clear command queue to prevent stale commands executing on reconnection
    while (!commandQueue_.isEmpty()) {
        PendingCommand cmd = std::move(commandQueue_.head());
        commandQueue_.dequeue();
        if (cmd.transferFile) {
            cmd.transferFile->close();
        }
    }

    // Clear any pending transfer state
    if (currentRetrFile_) {
        currentRetrFile_->close();
        currentRetrFile_.reset();
    }
    currentRetrIsMemory_ = false;
    pendingList_.reset();
    if (pendingRetr_ && pendingRetr_->file) {
        pendingRetr_->file->close();
    }
    pendingRetr_.reset();

    loggedIn_ = false;
    setState(State::Disconnected);
    emit disconnected();
}

void C64UFtpClient::onControlError(QAbstractSocket::SocketError socketError)
{
    qDebug() << "FTP: Control socket error:" << socketError << controlSocket_->errorString();

    // Stop connection timeout timer
    connectionTimer_->stop();

    // Clear command queue to prevent stale commands executing on reconnection
    while (!commandQueue_.isEmpty()) {
        PendingCommand cmd = std::move(commandQueue_.head());
        commandQueue_.dequeue();
        if (cmd.transferFile) {
            cmd.transferFile->close();
        }
    }

    // Clear any pending transfer state
    if (currentRetrFile_) {
        currentRetrFile_->close();
        currentRetrFile_.reset();
    }
    currentRetrIsMemory_ = false;
    pendingList_.reset();
    if (pendingRetr_ && pendingRetr_->file) {
        pendingRetr_->file->close();
    }
    pendingRetr_.reset();

    loggedIn_ = false;
    emit error(controlSocket_->errorString());
    setState(State::Disconnected);
}

void C64UFtpClient::onConnectionTimeout()
{
    qDebug() << "FTP: Connection timeout";

    // Abort the connection attempt
    controlSocket_->abort();

    // Clear any pending state
    while (!commandQueue_.isEmpty()) {
        PendingCommand cmd = std::move(commandQueue_.head());
        commandQueue_.dequeue();
        if (cmd.transferFile) {
            cmd.transferFile->close();
        }
    }

    loggedIn_ = false;
    setState(State::Disconnected);
    emit error(tr("Connection timed out after %1 seconds").arg(ConnectionTimeoutMs / 1000));
}

void C64UFtpClient::onControlReadyRead()
{
    responseBuffer_ += QString::fromUtf8(controlSocket_->readAll());

    // FTP responses end with \r\n
    while (responseBuffer_.contains("\r\n")) {
        int idx = responseBuffer_.indexOf("\r\n");
        QString line = responseBuffer_.left(idx);
        responseBuffer_ = responseBuffer_.mid(idx + CrLfLength);

        // Parse response code (first 3 digits)
        if (line.length() >= FtpReplyCodeLength) {
            bool ok = false;
            int code = line.left(FtpReplyCodeLength).toInt(&ok);
            if (ok) {
                // Check if this is a multi-line response (4th char is '-')
                if (line.length() > FtpReplyCodeLength && line[FtpReplyCodeLength] == '-') {
                    // Multi-line response, wait for final line
                    continue;
                }
                handleResponse(code, line.mid(FtpReplyTextOffset));
            }
        }
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
            QRegularExpression rx("\"(.*)\"");
            auto match = rx.match(text);
            if (match.hasMatch()) {
                currentDir_ = match.captured(1);
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
            QString dataHost;
            quint16 dataPort = 0;
            if (parsePassiveResponse(text, dataHost, dataPort)) {
                // Use the control socket's peer address instead of the IP from PASV
                // Many FTP servers return internal IPs that aren't reachable
                QString actualHost = controlSocket_->peerAddress().toString();
                qDebug() << "FTP: PASV response host:" << dataHost << "port:" << dataPort;
                qDebug() << "FTP: Using actual host:" << actualHost << "port:" << dataPort;
                dataSocket_->connectToHost(actualHost, dataPort);
                // Send the next command (LIST/RETR/STOR) immediately
                // The server expects the command before sending data
                processNextCommand();
            } else {
                emit error(tr("Data transfer failed: unable to establish data connection"));
                processNextCommand();
            }
        } else {
            emit error(tr("Data transfer failed: server does not support passive mode. %1").arg(text));
            processNextCommand();
        }
        break;

    case Command::List:
        if (code == FtpReplyFileStatusOk || code == FtpReplyDataConnectionOpen) {
            // Transfer starting - don't clear buffer here as data may have already arrived
            // (C64U server sometimes sends data before 150 response)
            downloading_ = true;
            qDebug() << "FTP: 150 received, listBuffer_ size:" << listBuffer_.size();
        } else if (code == FtpReplyTransferComplete) {
            // Transfer complete on server side, but data may still be in flight
            QString path = currentArg_.isEmpty() ? currentDir_ : currentArg_;
            if (dataSocket_->state() == QAbstractSocket::UnconnectedState) {
                // Data socket already closed, process immediately
                qDebug() << "FTP: 226 received, data socket already closed, processing";
                qDebug() << "FTP: LIST complete, total data:" << listBuffer_.size() << "bytes";
                QList<FtpEntry> entries = parseDirectoryListing(listBuffer_);
                qDebug() << "FTP: Parsed" << entries.size() << "entries";
                emit directoryListed(path, entries);
                processNextCommand();
            } else {
                // Wait for data socket to close before processing
                qDebug() << "FTP: 226 received, waiting for data socket to finish";
                pendingList_ = PendingListState{path};
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
            downloading_ = true;
            // Parse size from response if available
            QRegularExpression rx("\\((\\d+)\\s+bytes\\)");
            auto match = rx.match(text);
            if (match.hasMatch()) {
                transferSize_ = match.captured(1).toLongLong();
            }
        } else if (code == FtpReplyTransferComplete) {
            // Transfer complete on server side, but data may still be in flight
            // Use currentRetrFile_/currentRetrIsMemory_ which were set when RETR was dequeued
            if (dataSocket_->state() == QAbstractSocket::UnconnectedState) {
                // Data socket already closed, process immediately
                qDebug() << "FTP: RETR 226 received, data socket already closed, processing"
                         << "isMemory:" << currentRetrIsMemory_ << "file:" << currentRetrFile_.get();
                if (currentRetrIsMemory_) {
                    emit downloadToMemoryFinished(currentArg_, retrBuffer_);
                    retrBuffer_.clear();
                } else if (currentRetrFile_) {
                    currentRetrFile_->close();
                    emit downloadFinished(currentArg_, currentLocalPath_);
                    currentRetrFile_.reset();
                } else {
                    qDebug() << "FTP: ERROR - RETR 226 but no file handle!";
                }
                currentRetrFile_.reset();
                currentRetrIsMemory_ = false;
                processNextCommand();
            } else {
                // Wait for data socket to close before processing
                qDebug() << "FTP: RETR 226 received, waiting for data socket to finish";
                pendingRetr_ = PendingRetrState{
                    currentArg_,
                    currentLocalPath_,
                    std::move(currentRetrFile_),
                    currentRetrIsMemory_
                };
                // Clear current state (saved in pending struct)
                currentRetrFile_.reset();
                currentRetrIsMemory_ = false;
                // Don't process next command yet - wait for data socket
            }
        } else if (code >= FtpReplyErrorThreshold) {
            emit error(tr("Download failed for '%1': %2").arg(currentArg_, text));
            if (currentRetrIsMemory_) {
                retrBuffer_.clear();
            } else if (currentRetrFile_) {
                currentRetrFile_->close();
                currentRetrFile_.reset();
            }
            currentRetrFile_.reset();
            currentRetrIsMemory_ = false;
            processNextCommand();
        }
        break;

    case Command::Stor:
        if (code == FtpReplyFileStatusOk || code == FtpReplyDataConnectionOpen) {
            // Ready to receive data, start sending
            if (transferFile_ && transferFile_->isOpen()) {
                QByteArray data = transferFile_->readAll();
                dataSocket_->write(data);
                dataSocket_->disconnectFromHost();
            }
        } else if (code == FtpReplyTransferComplete) {
            // Transfer complete
            if (transferFile_) {
                transferFile_->close();
                emit uploadFinished(currentLocalPath_, currentArg_);
                transferFile_.reset();
            }
            processNextCommand();
        } else if (code >= FtpReplyErrorThreshold) {
            emit error(tr("Upload failed for '%1': %2").arg(currentArg_, text));
            if (transferFile_) {
                transferFile_->close();
                transferFile_.reset();
            }
            processNextCommand();
        }
        break;

    case Command::Mkd:
        if (code == FtpReplyPathCreated || code == FtpReplyFileExists) {
            // 257 = created, 553 = already exists (both are success for our purposes)
            emit directoryCreated(currentArg_);
        } else {
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
            emit error(tr("Cannot rename '%1': file not found or access denied. %2").arg(currentArg_, text));
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

void C64UFtpClient::onDataConnected()
{
    qDebug() << "FTP: Data socket connected to" << dataSocket_->peerAddress().toString() << ":" << dataSocket_->peerPort();
    // Data connection established, now send the actual command
    // The command was already sent, waiting for data
}

void C64UFtpClient::onDataReadyRead()
{
    QByteArray data = dataSocket_->readAll();
    qDebug() << "FTP: Data received:" << data.size() << "bytes";

    if (currentCommand_ == Command::List) {
        listBuffer_.append(data);
    } else if (currentCommand_ == Command::Retr) {
        // Check pending state first (226 may have arrived but data still coming)
        // then fall back to current RETR state (set when RETR was dequeued)
        bool isMemory = pendingRetr_ ? pendingRetr_->isMemory : currentRetrIsMemory_;
        QFile *file = pendingRetr_ ? pendingRetr_->file.get() : currentRetrFile_.get();

        if (isMemory) {
            retrBuffer_.append(data);
            emit downloadProgress(currentArg_, retrBuffer_.size(), transferSize_);
        } else if (file) {
            file->write(data);
            emit downloadProgress(currentArg_, file->size(), transferSize_);
        }
    }
}

void C64UFtpClient::onDataDisconnected()
{
    qDebug() << "FTP: Data socket disconnected, bytes available:" << dataSocket_->bytesAvailable();
    // Read any remaining data before disconnect completes
    if (dataSocket_->bytesAvailable() > 0) {
        onDataReadyRead();
    }
    downloading_ = false;

    // If we received 226 and were waiting for data socket to close, process now
    if (pendingList_) {
        qDebug() << "FTP: Processing pending LIST, total data:" << listBuffer_.size() << "bytes";
        qDebug() << "FTP: Data buffer contents:" << listBuffer_;
        QList<FtpEntry> entries = parseDirectoryListing(listBuffer_);
        qDebug() << "FTP: Parsed" << entries.size() << "entries";
        emit directoryListed(pendingList_->path, entries);
        pendingList_.reset();  // Clear all pending LIST state with one call
        processNextCommand();
    } else if (pendingRetr_) {
        qDebug() << "FTP: Processing pending RETR for" << pendingRetr_->remotePath
                 << "isMemory:" << pendingRetr_->isMemory << "file:" << pendingRetr_->file.get();
        // Use the SAVED state, not the current global state (which may have been
        // corrupted by other operations like downloadToMemory for file preview)
        if (pendingRetr_->isMemory) {
            emit downloadToMemoryFinished(pendingRetr_->remotePath, retrBuffer_);
            retrBuffer_.clear();
        } else if (pendingRetr_->file) {
            pendingRetr_->file->close();
            emit downloadFinished(pendingRetr_->remotePath, pendingRetr_->localPath);
        } else {
            qDebug() << "FTP: ERROR - pending RETR has no file handle!";
        }
        pendingRetr_.reset();  // Clear all pending RETR state with one call
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

bool C64UFtpClient::parsePassiveResponse(const QString &text, QString &host, quint16 &port)
{
    // Parse response like: 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)
    QRegularExpression rx("\\((\\d+),(\\d+),(\\d+),(\\d+),(\\d+),(\\d+)\\)");
    auto match = rx.match(text);

    if (!match.hasMatch()) {
        return false;
    }

    host = QString("%1.%2.%3.%4")
               .arg(match.captured(1))
               .arg(match.captured(2))
               .arg(match.captured(3))
               .arg(match.captured(4));

    int p1 = match.captured(5).toInt();
    int p2 = match.captured(6).toInt();
    port = static_cast<quint16>((p1 * PassivePortMultiplier) + p2);

    return true;
}

QList<FtpEntry> C64UFtpClient::parseDirectoryListing(const QByteArray &data)
{
    QList<FtpEntry> entries;
    QString listing = QString::fromUtf8(data);
    QStringList lines = listing.split("\r\n", Qt::SkipEmptyParts);

    // Unix-style listing: drwxr-xr-x 2 user group 4096 Jan 1 12:00 dirname
    // Or simple listing: filename
    QRegularExpression unixRx(
        "^([d\\-])([rwx\\-]{9})\\s+\\d+\\s+\\S+\\s+\\S+\\s+(\\d+)\\s+"
        "(\\w+\\s+\\d+\\s+[\\d:]+)\\s+(.+)$");

    for (const QString &line : lines) {
        if (line.isEmpty()) continue;

        FtpEntry entry;
        auto match = unixRx.match(line);

        if (match.hasMatch()) {
            entry.isDirectory = (match.captured(1) == "d");
            entry.permissions = match.captured(2);
            entry.size = match.captured(3).toLongLong();
            entry.name = match.captured(5);
        } else {
            // Simple listing - just filename
            entry.name = line.trimmed();
            entry.isDirectory = false;  // Can't tell from simple listing
        }

        if (!entry.name.isEmpty() && entry.name != "." && entry.name != "..") {
            entries.append(entry);
        }
    }

    return entries;
}

// Public interface methods

void C64UFtpClient::list(const QString &path)
{
    if (!loggedIn_) {
        emit error(tr("Cannot list directory: not connected to server"));
        return;
    }
    listBuffer_.clear();  // Clear buffer at start of list operation
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

    transferSize_ = 0;
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

    retrBuffer_.clear();
    transferSize_ = 0;
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

    transferFile_ = std::make_shared<QFile>(localPath);
    if (!transferFile_->open(QIODevice::ReadOnly)) {
        emit error(tr("Cannot read file '%1': file not found or access denied").arg(localPath));
        transferFile_.reset();
        return;
    }

    transferSize_ = transferFile_->size();
    queueCommand(Command::Type, "I");  // Binary mode
    queueCommand(Command::Pasv);
    queueCommand(Command::Stor, remotePath, localPath);
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
    // unique_ptr handles cleanup automatically
    while (!commandQueue_.isEmpty()) {
        PendingCommand cmd = std::move(commandQueue_.head());
        commandQueue_.dequeue();
        if (cmd.transferFile) {
            cmd.transferFile->close();
        }
        // unique_ptr automatically deletes when cmd goes out of scope
    }

    if (dataSocket_->state() != QAbstractSocket::UnconnectedState) {
        dataSocket_->abort();
    }

    // Clean up upload file handle
    if (transferFile_) {
        transferFile_->close();
        transferFile_.reset();
    }

    // Clean up current RETR state
    if (currentRetrFile_) {
        currentRetrFile_->close();
        currentRetrFile_.reset();
    }
    currentRetrIsMemory_ = false;

    // Clear pending state - RAII structs make this simple
    pendingList_.reset();
    if (pendingRetr_ && pendingRetr_->file) {
        pendingRetr_->file->close();
    }
    pendingRetr_.reset();
    listBuffer_.clear();
    retrBuffer_.clear();

    sendCommand("ABOR");
    setState(State::Ready);
}
