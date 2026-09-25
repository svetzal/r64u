#include "fakeftpserver.h"

#include <QHostAddress>
#include <QTimer>

#include <algorithm>

namespace {
constexpr int PassivePortMultiplier = 256;
constexpr int ReplyThenCloseDelayMs = 50;
}  // namespace

FakeFtpServer::FakeFtpServer(QObject *parent) : QObject(parent)
{
    controlServer_.setMaxPendingConnections(1);
    connect(&controlServer_, &QTcpServer::newConnection, this,
            &FakeFtpServer::onNewControlConnection);
}

bool FakeFtpServer::listen()
{
    return controlServer_.listen(QHostAddress::LocalHost, 0);
}

quint16 FakeFtpServer::port() const
{
    return controlServer_.serverPort();
}

void FakeFtpServer::addReply(const QString &prefix, const QString &reply)
{
    overrides_.append({prefix, reply});
}

void FakeFtpServer::setFile(const QString &path, const QByteArray &contents)
{
    files_.insert(path, contents);
}

void FakeFtpServer::setListing(const QByteArray &listing)
{
    listing_ = listing;
}

void FakeFtpServer::setCompletionOrder(CompletionOrder order)
{
    completionOrder_ = order;
}

void FakeFtpServer::setStallAfterBytes(qint64 bytes)
{
    stallAfterBytes_ = bytes;
}

void FakeFtpServer::setAborReplyDuringTransfer(const QByteArray &reply)
{
    aborReplyDuringTransfer_ = reply;
}

void FakeFtpServer::failNextPasv(int count)
{
    pasvFailuresRemaining_ = count;
}

void FakeFtpServer::advertiseDeadDataPortOnce()
{
    advertiseDeadPort_ = true;
}

void FakeFtpServer::setKeepDataOpenOnError(bool keep)
{
    keepDataOpenOnError_ = keep;
}

int FakeFtpServer::commandCount(const QString &prefix) const
{
    int count = 0;
    for (const auto &command : commands_) {
        if (command.startsWith(prefix)) {
            ++count;
        }
    }
    return count;
}

void FakeFtpServer::closeClientConnection()
{
    if (control_) {
        control_->disconnectFromHost();
        control_ = nullptr;
    }
}

void FakeFtpServer::onNewControlConnection()
{
    control_ = controlServer_.nextPendingConnection();
    connect(control_, &QTcpSocket::readyRead, this, &FakeFtpServer::onControlReadyRead);
    reply("220 Fake FTP server ready\r\n");
}

void FakeFtpServer::onControlReadyRead()
{
    while (control_ && control_->canReadLine()) {
        handleCommand(QString::fromUtf8(control_->readLine()).trimmed());
    }
}

void FakeFtpServer::reply(const QByteArray &text)
{
    if (control_) {
        control_->write(text);
        control_->flush();
    }
}

bool FakeFtpServer::replyFromOverride(const QString &line)
{
    const auto match =
        std::find_if(overrides_.cbegin(), overrides_.cend(),
                     [&line](const auto &entry) { return line.startsWith(entry.first); });
    if (match == overrides_.cend()) {
        return false;
    }
    reply(match->second.toUtf8());
    return true;
}

void FakeFtpServer::handleCommand(const QString &line)
{
    commands_ << line;
    if (replyFromOverride(line)) {
        return;
    }

    const QString verb = line.section(' ', 0, 0).toUpper();
    const QString argument = line.section(' ', 1);

    if (verb == "USER") {
        reply("331 Password required\r\n");
    } else if (verb == "PASS") {
        reply("230 User logged in\r\n");
    } else if (verb == "TYPE") {
        reply("200 Type set\r\n");
    } else if (verb == "PWD") {
        reply("257 \"/\" is the current directory\r\n");
    } else if (verb == "CWD" || verb == "DELE" || verb == "RMD" || verb == "RNTO") {
        reply("250 Requested file action okay\r\n");
    } else if (verb == "MKD") {
        reply(QString("257 \"%1\" created\r\n").arg(argument).toUtf8());
    } else if (verb == "RNFR") {
        reply("350 Ready for RNTO\r\n");
    } else if (verb == "PASV") {
        handlePasv();
    } else if (verb == "LIST") {
        handleList();
    } else if (verb == "RETR") {
        handleRetr(argument);
    } else if (verb == "STOR") {
        handleStor(argument);
    } else if (verb == "ABOR") {
        handleAbor();
    } else if (verb == "QUIT") {
        reply("221 Goodbye\r\n");
    } else {
        reply("500 Unknown command\r\n");
    }
}

void FakeFtpServer::handlePasv()
{
    if (pasvFailuresRemaining_ > 0) {
        --pasvFailuresRemaining_;
        reply("502 Passive mode not available\r\n");
        return;
    }

    // Stop listening on any previous passive port, but deliberately leave an
    // earlier data connection open: servers are not obliged to close it, and
    // tests rely on observing how the client copes with a lingering one.
    if (passiveServer_) {
        passiveServer_->close();
    }
    data_ = nullptr;
    onDataConnected_ = nullptr;
    deadDataPort_ = false;

    quint16 port = 0;
    if (advertiseDeadPort_) {
        advertiseDeadPort_ = false;
        QTcpServer probe;
        (void)probe.listen(QHostAddress::LocalHost, 0);
        port = probe.serverPort();
        probe.close();
        passiveServer_ = nullptr;
        deadDataPort_ = true;
    } else {
        passiveServer_ = new QTcpServer(this);
        (void)passiveServer_->listen(QHostAddress::LocalHost, 0);
        port = passiveServer_->serverPort();
        QTcpServer *server = passiveServer_;
        connect(server, &QTcpServer::newConnection, this, [this, server]() {
            data_ = server->nextPendingConnection();
            server->close();
            if (onDataConnected_) {
                auto action = std::move(onDataConnected_);
                onDataConnected_ = nullptr;
                action(data_);
            }
        });
    }

    reply(QString("227 Entering Passive Mode (127,0,0,1,%1,%2)\r\n")
              .arg(port / PassivePortMultiplier)
              .arg(port % PassivePortMultiplier)
              .toUtf8());
}

bool FakeFtpServer::hasDataChannel() const
{
    return !deadDataPort_ && (data_ || passiveServer_);
}

void FakeFtpServer::withDataConnection(const std::function<void(QTcpSocket *)> &action)
{
    if (data_) {
        action(data_);
    } else {
        onDataConnected_ = action;
    }
}

void FakeFtpServer::handleList()
{
    if (!hasDataChannel()) {
        reply("425 Can't open data connection\r\n");
        return;
    }
    reply("150 Here comes the directory listing\r\n");
    withDataConnection([this](QTcpSocket *data) {
        activeTransfer_ = data;
        data->write(listing_);
        finishTransfer(data);
    });
}

void FakeFtpServer::handleRetr(const QString &path)
{
    if (!hasDataChannel()) {
        reply("425 Can't open data connection\r\n");
        return;
    }
    if (!files_.contains(path)) {
        reply("550 File not found\r\n");
        if (!keepDataOpenOnError_) {
            withDataConnection([](QTcpSocket *data) { data->disconnectFromHost(); });
        }
        return;
    }

    const QByteArray contents = files_.value(path);
    reply(QString("150 Opening BINARY mode data connection for %1 (%2 bytes)\r\n")
              .arg(path)
              .arg(contents.size())
              .toUtf8());
    withDataConnection([this, contents](QTcpSocket *data) {
        activeTransfer_ = data;
        if (stallAfterBytes_ >= 0) {
            data->write(contents.left(stallAfterBytes_));
            return;  // Held open until ABOR
        }
        data->write(contents);
        finishTransfer(data);
    });
}

void FakeFtpServer::handleStor(const QString &path)
{
    if (!hasDataChannel()) {
        reply("425 Can't open data connection\r\n");
        return;
    }
    reply("150 Ok to send data\r\n");
    stored_.remove(path);
    withDataConnection([this, path](QTcpSocket *data) {
        activeTransfer_ = data;
        connect(data, &QTcpSocket::readyRead, this,
                [this, path, data]() { stored_[path] += data->readAll(); });
        connect(data, &QTcpSocket::disconnected, this, [this, path, data]() {
            stored_[path] += data->readAll();
            activeTransfer_ = nullptr;
            reply("226 Transfer complete\r\n");
        });
    });
}

void FakeFtpServer::handleAbor()
{
    if (activeTransfer_) {
        QTcpSocket *data = activeTransfer_;
        activeTransfer_ = nullptr;
        data->disconnect(this);
        data->abort();
        reply(aborReplyDuringTransfer_);
        return;
    }
    reply("225 No transfer to abort\r\n");
}

void FakeFtpServer::finishTransfer(QTcpSocket *data)
{
    data_ = nullptr;
    if (completionOrder_ == CompletionOrder::CloseDataThenReply) {
        connect(data, &QTcpSocket::disconnected, this, [this, data]() {
            if (activeTransfer_ == data) {
                activeTransfer_ = nullptr;
            }
            reply("226 Transfer complete\r\n");
        });
        data->disconnectFromHost();
        return;
    }

    activeTransfer_ = nullptr;
    reply("226 Transfer complete\r\n");
    QTimer::singleShot(ReplyThenCloseDelayMs, data, [data]() { data->disconnectFromHost(); });
}
