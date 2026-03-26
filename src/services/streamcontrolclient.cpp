/**
 * @file streamcontrolclient.cpp
 * @brief Implementation of the stream control TCP client.
 */

#include "streamcontrolclient.h"

#include "utils/logging.h"

StreamControlClient::StreamControlClient(QObject *parent)
    : IStreamControlClient(parent), socket_(new QTcpSocket(this))
{
    connect(socket_, &QTcpSocket::connected, this, &StreamControlClient::onSocketConnected);
    connect(socket_, &QTcpSocket::disconnected, this, &StreamControlClient::onSocketDisconnected);
    connect(socket_, &QTcpSocket::errorOccurred, this, &StreamControlClient::onSocketError);
}

StreamControlClient::~StreamControlClient()
{
    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->abort();
    }
}

void StreamControlClient::setHost(const QString &host)
{
    LOG_VERBOSE() << "StreamControlClient: setHost" << host;
    host_ = host;
}

void StreamControlClient::startVideoStream(const QString &targetHost, quint16 targetPort,
                                           quint16 durationTicks)
{
    PendingCommand cmd;
    cmd.type = CommandType::StartVideo;
    cmd.description = QString("start video stream to %1:%2").arg(targetHost).arg(targetPort);
    cmd.data = streamprotocol::buildStartCommand(CommandType::StartVideo, targetHost, targetPort,
                                                 durationTicks);

    sendCommand(cmd);
}

void StreamControlClient::startAudioStream(const QString &targetHost, quint16 targetPort,
                                           quint16 durationTicks)
{
    PendingCommand cmd;
    cmd.type = CommandType::StartAudio;
    cmd.description = QString("start audio stream to %1:%2").arg(targetHost).arg(targetPort);
    cmd.data = streamprotocol::buildStartCommand(CommandType::StartAudio, targetHost, targetPort,
                                                 durationTicks);

    sendCommand(cmd);
}

void StreamControlClient::stopVideoStream()
{
    PendingCommand cmd;
    cmd.type = CommandType::StopVideo;
    cmd.description = "stop video stream";
    cmd.data = streamprotocol::buildStopCommand(CommandType::StopVideo);

    sendCommand(cmd);
}

void StreamControlClient::stopAudioStream()
{
    PendingCommand cmd;
    cmd.type = CommandType::StopAudio;
    cmd.description = "stop audio stream";
    cmd.data = streamprotocol::buildStopCommand(CommandType::StopAudio);

    sendCommand(cmd);
}

void StreamControlClient::startAllStreams(const QString &targetHost, quint16 videoPort,
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

void StreamControlClient::clearPendingCommands()
{
    pendingCommands_.clear();
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

    LOG_VERBOSE() << "StreamControlClient: Connecting to" << host_ << "port" << ControlPort;
    connecting_ = true;
    socket_->connectToHost(host_, ControlPort);
}

void StreamControlClient::onSocketConnected()
{
    LOG_VERBOSE() << "StreamControlClient: Connected to" << host_;
    connecting_ = false;

    // Send all pending commands
    while (!pendingCommands_.isEmpty()) {
        PendingCommand cmd = pendingCommands_.takeFirst();

        LOG_VERBOSE() << "StreamControlClient: Sending command:" << cmd.description
                      << "data size:" << cmd.data.size() << "bytes";

        qint64 written = socket_->write(cmd.data);
        socket_->flush();

        if (written == cmd.data.size()) {
            LOG_VERBOSE() << "StreamControlClient: Command sent successfully";
            emit commandSucceeded(cmd.description);
        } else {
            LOG_VERBOSE() << "StreamControlClient: Failed to write command, wrote" << written
                          << "of" << cmd.data.size();
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
    LOG_VERBOSE() << "StreamControlClient: Socket error connecting to" << host_ << "port"
                  << ControlPort << "- error code:" << error << socket_->errorString();
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
