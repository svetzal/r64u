#include "c64uftpclient.h"

#include <QRegularExpression>
#include <QFileInfo>

C64UFtpClient::C64UFtpClient(QObject *parent)
    : QObject(parent)
    , controlSocket_(new QTcpSocket(this))
    , dataSocket_(new QTcpSocket(this))
{
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
}

C64UFtpClient::~C64UFtpClient()
{
    if (transferFile_) {
        transferFile_->close();
        delete transferFile_;
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
        return;
    }

    setState(State::Connecting);
    controlSocket_->connectToHost(host_, port_);
}

void C64UFtpClient::disconnect()
{
    if (state_ == State::Disconnected) {
        return;
    }

    commandQueue_.clear();
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
        return;
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

void C64UFtpClient::processNextCommand()
{
    if (commandQueue_.isEmpty()) {
        setState(State::Ready);
        return;
    }

    setState(State::Busy);
    PendingCommand pending = commandQueue_.dequeue();
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
    setState(State::Connected);
}

void C64UFtpClient::onControlDisconnected()
{
    setState(State::Disconnected);
    emit disconnected();
}

void C64UFtpClient::onControlError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError)
    emit error(controlSocket_->errorString());
    setState(State::Disconnected);
}

void C64UFtpClient::onControlReadyRead()
{
    responseBuffer_ += QString::fromUtf8(controlSocket_->readAll());

    // FTP responses end with \r\n
    while (responseBuffer_.contains("\r\n")) {
        int idx = responseBuffer_.indexOf("\r\n");
        QString line = responseBuffer_.left(idx);
        responseBuffer_ = responseBuffer_.mid(idx + 2);

        // Parse response code (first 3 digits)
        if (line.length() >= 3) {
            bool ok;
            int code = line.left(3).toInt(&ok);
            if (ok) {
                // Check if this is a multi-line response (4th char is '-')
                if (line.length() > 3 && line[3] == '-') {
                    // Multi-line response, wait for final line
                    continue;
                }
                handleResponse(code, line.mid(4));
            }
        }
    }
}

void C64UFtpClient::handleResponse(int code, const QString &text)
{
    // 1xx: Positive Preliminary
    // 2xx: Positive Completion
    // 3xx: Positive Intermediate
    // 4xx: Transient Negative
    // 5xx: Permanent Negative

    switch (state_) {
    case State::Connected:
        // Welcome message
        if (code == 220) {
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
        if (code == 331) {
            // Password required
            queueCommand(Command::Pass);
        } else if (code == 230) {
            // Logged in without password
            setState(State::Ready);
            emit connected();
        } else {
            emit error("Login failed: " + text);
            disconnect();
            return;
        }
        processNextCommand();
        break;

    case Command::Pass:
        if (code == 230) {
            setState(State::Ready);
            emit connected();
        } else {
            emit error("Login failed: " + text);
            disconnect();
            return;
        }
        processNextCommand();
        break;

    case Command::Pwd:
        if (code == 257) {
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
        if (code == 250) {
            currentDir_ = currentArg_;
            emit directoryChanged(currentDir_);
        } else {
            emit error("Failed to change directory: " + text);
        }
        processNextCommand();
        break;

    case Command::Type:
        processNextCommand();
        break;

    case Command::Pasv:
        if (code == 227) {
            QString dataHost;
            quint16 dataPort;
            if (parsePassiveResponse(text, dataHost, dataPort)) {
                dataSocket_->connectToHost(dataHost, dataPort);
            } else {
                emit error("Failed to parse passive mode response");
                processNextCommand();
            }
        } else {
            emit error("Passive mode failed: " + text);
            processNextCommand();
        }
        break;

    case Command::List:
        if (code == 150 || code == 125) {
            // Transfer starting, data will come on data socket
            dataBuffer_.clear();
            downloading_ = true;
        } else if (code == 226) {
            // Transfer complete
            QList<FtpEntry> entries = parseDirectoryListing(dataBuffer_);
            emit directoryListed(currentArg_.isEmpty() ? currentDir_ : currentArg_, entries);
            processNextCommand();
        } else if (code >= 400) {
            emit error("List failed: " + text);
            processNextCommand();
        }
        break;

    case Command::Retr:
        if (code == 150 || code == 125) {
            // Transfer starting
            downloading_ = true;
            // Parse size from response if available
            QRegularExpression rx("\\((\\d+)\\s+bytes\\)");
            auto match = rx.match(text);
            if (match.hasMatch()) {
                transferSize_ = match.captured(1).toLongLong();
            }
        } else if (code == 226) {
            // Transfer complete
            if (transferFile_) {
                transferFile_->close();
                emit downloadFinished(currentArg_, currentLocalPath_);
                delete transferFile_;
                transferFile_ = nullptr;
            }
            processNextCommand();
        } else if (code >= 400) {
            emit error("Download failed: " + text);
            if (transferFile_) {
                transferFile_->close();
                delete transferFile_;
                transferFile_ = nullptr;
            }
            processNextCommand();
        }
        break;

    case Command::Stor:
        if (code == 150 || code == 125) {
            // Ready to receive data, start sending
            if (transferFile_ && transferFile_->isOpen()) {
                QByteArray data = transferFile_->readAll();
                dataSocket_->write(data);
                dataSocket_->disconnectFromHost();
            }
        } else if (code == 226) {
            // Transfer complete
            if (transferFile_) {
                transferFile_->close();
                emit uploadFinished(currentLocalPath_, currentArg_);
                delete transferFile_;
                transferFile_ = nullptr;
            }
            processNextCommand();
        } else if (code >= 400) {
            emit error("Upload failed: " + text);
            if (transferFile_) {
                transferFile_->close();
                delete transferFile_;
                transferFile_ = nullptr;
            }
            processNextCommand();
        }
        break;

    case Command::Mkd:
        if (code == 257) {
            emit directoryCreated(currentArg_);
        } else {
            emit error("Failed to create directory: " + text);
        }
        processNextCommand();
        break;

    case Command::Rmd:
    case Command::Dele:
        if (code == 250) {
            emit fileRemoved(currentArg_);
        } else {
            emit error("Failed to delete: " + text);
        }
        processNextCommand();
        break;

    case Command::RnFr:
        if (code == 350) {
            // Ready for RNTO
        } else {
            emit error("Rename failed: " + text);
        }
        processNextCommand();
        break;

    case Command::RnTo:
        if (code == 250) {
            emit fileRenamed(currentLocalPath_, currentArg_);  // oldPath stored in localPath
        } else {
            emit error("Rename failed: " + text);
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
    // Data connection established, now send the actual command
    // The command was already sent, waiting for data
}

void C64UFtpClient::onDataReadyRead()
{
    QByteArray data = dataSocket_->readAll();

    if (currentCommand_ == Command::List) {
        dataBuffer_.append(data);
    } else if (currentCommand_ == Command::Retr && transferFile_) {
        transferFile_->write(data);
        emit downloadProgress(currentArg_, transferFile_->size(), transferSize_);
    }
}

void C64UFtpClient::onDataDisconnected()
{
    downloading_ = false;
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
    port = static_cast<quint16>(p1 * 256 + p2);

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
    queueCommand(Command::Type, "A");  // ASCII mode for listing
    queueCommand(Command::Pasv);
    queueCommand(Command::List, path);
}

void C64UFtpClient::changeDirectory(const QString &path)
{
    queueCommand(Command::Cwd, path);
}

void C64UFtpClient::makeDirectory(const QString &path)
{
    queueCommand(Command::Mkd, path);
}

void C64UFtpClient::removeDirectory(const QString &path)
{
    queueCommand(Command::Rmd, path);
}

void C64UFtpClient::download(const QString &remotePath, const QString &localPath)
{
    transferFile_ = new QFile(localPath);
    if (!transferFile_->open(QIODevice::WriteOnly)) {
        emit error("Failed to open local file for writing: " + localPath);
        delete transferFile_;
        transferFile_ = nullptr;
        return;
    }

    transferSize_ = 0;
    queueCommand(Command::Type, "I");  // Binary mode
    queueCommand(Command::Pasv);
    queueCommand(Command::Retr, remotePath, localPath);
}

void C64UFtpClient::upload(const QString &localPath, const QString &remotePath)
{
    transferFile_ = new QFile(localPath);
    if (!transferFile_->open(QIODevice::ReadOnly)) {
        emit error("Failed to open local file for reading: " + localPath);
        delete transferFile_;
        transferFile_ = nullptr;
        return;
    }

    transferSize_ = transferFile_->size();
    queueCommand(Command::Type, "I");  // Binary mode
    queueCommand(Command::Pasv);
    queueCommand(Command::Stor, remotePath, localPath);
}

void C64UFtpClient::remove(const QString &path)
{
    queueCommand(Command::Dele, path);
}

void C64UFtpClient::rename(const QString &oldPath, const QString &newPath)
{
    queueCommand(Command::RnFr, oldPath, oldPath);  // Store oldPath for signal
    queueCommand(Command::RnTo, newPath);
}

void C64UFtpClient::abort()
{
    commandQueue_.clear();
    if (dataSocket_->state() != QAbstractSocket::UnconnectedState) {
        dataSocket_->abort();
    }
    if (transferFile_) {
        transferFile_->close();
        delete transferFile_;
        transferFile_ = nullptr;
    }
    sendCommand("ABOR");
    setState(State::Ready);
}
