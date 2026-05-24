/**
 * @file udpstreamsocket.cpp
 * @brief Shared UDP socket lifecycle helper used by stream receivers.
 */

#include "udpstreamsocket.h"

#include <QNetworkDatagram>
#include <QVariant>

UdpStreamSocket::UdpStreamSocket(int receiveBufferSizeBytes, int expectedPacketSize,
                                 QObject *parent)
    : QObject(parent), socket_(new QUdpSocket(this)),
      receiveBufferSizeBytes_(receiveBufferSizeBytes), expectedPacketSize_(expectedPacketSize)
{
    connect(socket_, &QUdpSocket::readyRead, this, &UdpStreamSocket::onReadyRead);
    connect(socket_, &QUdpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) { emit socketError(socket_->errorString()); });
}

bool UdpStreamSocket::bind(quint16 port)
{
    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        close();
    }

    socket_->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption,
                             QVariant(receiveBufferSizeBytes_));

    if (!socket_->bind(QHostAddress::Any, port)) {
        emit socketError(
            QString("Failed to bind to port %1: %2").arg(port).arg(socket_->errorString()));
        return false;
    }

    return true;
}

void UdpStreamSocket::close()
{
    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->close();
    }
}

bool UdpStreamSocket::isActive() const
{
    return socket_->state() == QAbstractSocket::BoundState;
}

quint16 UdpStreamSocket::port() const
{
    return socket_->localPort();
}

void UdpStreamSocket::onReadyRead()
{
    while (socket_->hasPendingDatagrams()) {
        QNetworkDatagram datagram = socket_->receiveDatagram();
        if (datagram.isValid()) {
            QByteArray packet = datagram.data();
            if (packet.size() == expectedPacketSize_) {
                emit packetReceived(packet);
            }
            // Datagrams with unexpected size are ignored
        }
    }
}
