/**
 * @file streamcontrolclient.cpp
 * @brief Implementation of the stream control TCP client.
 */

#include "streamcontrolclient.h"

StreamControlClient::StreamControlClient(QObject *parent)
    : QObject(parent)
    , socket_(new QTcpSocket(this))
{
    connect(socket_, &QTcpSocket::connected,
            this, &StreamControlClient::onSocketConnected);
    connect(socket_, &QTcpSocket::disconnected,
            this, &StreamControlClient::onSocketDisconnected);
    connect(socket_, &QTcpSocket::errorOccurred,
            this, &StreamControlClient::onSocketError);
}

StreamControlClient::~StreamControlClient()
{
    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->abort();
    }
}

void StreamControlClient::setHost(const QString &host)
{
    host_ = host;
}

void StreamControlClient::startVideoStream(const QString &targetHost,
                                           quint16 targetPort,
                                           quint16 durationTicks)
{
    PendingCommand cmd;
    cmd.type = CommandType::StartVideo;
    cmd.description = QString("start video stream to %1:%2").arg(targetHost).arg(targetPort);
    cmd.data = buildStartCommand(CommandType::StartVideo, targetHost, targetPort, durationTicks);

    sendCommand(cmd);
}

void StreamControlClient::startAudioStream(const QString &targetHost,
                                           quint16 targetPort,
                                           quint16 durationTicks)
{
    PendingCommand cmd;
    cmd.type = CommandType::StartAudio;
    cmd.description = QString("start audio stream to %1:%2").arg(targetHost).arg(targetPort);
    cmd.data = buildStartCommand(CommandType::StartAudio, targetHost, targetPort, durationTicks);

    sendCommand(cmd);
}

void StreamControlClient::stopVideoStream()
{
    PendingCommand cmd;
    cmd.type = CommandType::StopVideo;
    cmd.description = "stop video stream";
    cmd.data = buildStopCommand(CommandType::StopVideo);

    sendCommand(cmd);
}

void StreamControlClient::stopAudioStream()
{
    PendingCommand cmd;
    cmd.type = CommandType::StopAudio;
    cmd.description = "stop audio stream";
    cmd.data = buildStopCommand(CommandType::StopAudio);

    sendCommand(cmd);
}

void StreamControlClient::startAllStreams(const QString &targetHost,
                                          quint16 videoPort,
                                          quint16 audioPort)
{
    startVideoStream(targetHost, videoPort);
    startAudioStream(targetHost, audioPort);
}

void StreamControlClient::stopAllStreams()
{
    stopVideoStream();
    stopAudioStream();
}

void StreamControlClient::sendCommand(const PendingCommand &command)
{
    if (host_.isEmpty()) {
        emit commandFailed(command.description, "No host configured");
        return;
    }

    pendingCommands_.append(command);

    // If not already connecting/connected, initiate connection
    if (socket_->state() == QAbstractSocket::UnconnectedState && !connecting_) {
        connectAndSend();
    }
}

void StreamControlClient::connectAndSend()
{
    if (pendingCommands_.isEmpty()) {
        return;
    }

    connecting_ = true;
    socket_->connectToHost(host_, ControlPort);
}

void StreamControlClient::onSocketConnected()
{
    connecting_ = false;

    // Send all pending commands
    while (!pendingCommands_.isEmpty()) {
        PendingCommand cmd = pendingCommands_.takeFirst();

        qint64 written = socket_->write(cmd.data);
        socket_->flush();

        if (written == cmd.data.size()) {
            emit commandSucceeded(cmd.description);
        } else {
            emit commandFailed(cmd.description, "Failed to write command data");
        }
    }

    // Close connection after sending (protocol doesn't expect response)
    socket_->disconnectFromHost();
}

void StreamControlClient::onSocketDisconnected()
{
    // If there are more pending commands, reconnect
    if (!pendingCommands_.isEmpty()) {
        connectAndSend();
    }
}

void StreamControlClient::onSocketError(QAbstractSocket::SocketError error)
{
    connecting_ = false;

    QString errorMsg;
    switch (error) {
    case QAbstractSocket::ConnectionRefusedError:
        errorMsg = "Connection refused";
        break;
    case QAbstractSocket::HostNotFoundError:
        errorMsg = "Host not found";
        break;
    case QAbstractSocket::SocketTimeoutError:
        errorMsg = "Connection timed out";
        break;
    case QAbstractSocket::NetworkError:
        errorMsg = "Network error";
        break;
    default:
        errorMsg = socket_->errorString();
        break;
    }

    emit connectionError(errorMsg);

    // Fail all pending commands
    while (!pendingCommands_.isEmpty()) {
        PendingCommand cmd = pendingCommands_.takeFirst();
        emit commandFailed(cmd.description, errorMsg);
    }
}

QByteArray StreamControlClient::buildStartCommand(CommandType type,
                                                   const QString &targetHost,
                                                   quint16 targetPort,
                                                   quint16 durationTicks) const
{
    // Build the destination string: "IP:PORT"
    QString destination = QString("%1:%2").arg(targetHost).arg(targetPort);
    QByteArray destBytes = destination.toLatin1();

    // Parameter length = 2 (duration) + destination string length
    quint16 paramLength = static_cast<quint16>(2 + destBytes.size());

    QByteArray command;
    command.reserve(4 + paramLength);

    // Command byte
    command.append(static_cast<char>(type));

    // Command marker
    command.append(static_cast<char>(0xFF));

    // Parameter length (little-endian)
    command.append(static_cast<char>(paramLength & 0xFF));
    command.append(static_cast<char>((paramLength >> 8) & 0xFF));

    // Duration (little-endian, 0 = infinite)
    command.append(static_cast<char>(durationTicks & 0xFF));
    command.append(static_cast<char>((durationTicks >> 8) & 0xFF));

    // Destination string
    command.append(destBytes);

    return command;
}

QByteArray StreamControlClient::buildStopCommand(CommandType type) const
{
    QByteArray command;
    command.reserve(4);

    // Command byte
    command.append(static_cast<char>(type));

    // Command marker
    command.append(static_cast<char>(0xFF));

    // Parameter length = 0 (little-endian)
    command.append(static_cast<char>(0x00));
    command.append(static_cast<char>(0x00));

    return command;
}

QString StreamControlClient::commandTypeToString(CommandType type) const
{
    switch (type) {
    case CommandType::StartVideo:
        return "StartVideo";
    case CommandType::StartAudio:
        return "StartAudio";
    case CommandType::StopVideo:
        return "StopVideo";
    case CommandType::StopAudio:
        return "StopAudio";
    default:
        return "Unknown";
    }
}
