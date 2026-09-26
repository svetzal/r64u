#ifndef FAKESTREAMCONTROLPORT_H
#define FAKESTREAMCONTROLPORT_H

#include <QByteArray>
#include <QHostAddress>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

/**
 * @brief Stands in for a device's TCP stream control port.
 *
 * Listens on an ephemeral localhost port, accepts every connection and
 * collects all bytes clients send, in arrival order. The real device answers
 * nothing, so neither does this.
 */
class FakeStreamControlPort : public QObject
{
public:
    explicit FakeStreamControlPort(QObject *parent = nullptr) : QObject(parent)
    {
        server_.listen(QHostAddress::LocalHost, 0);
        connect(&server_, &QTcpServer::newConnection, this, [this]() {
            while (QTcpSocket *socket = server_.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, this,
                        [this, socket]() { received_ += socket->readAll(); });
                connect(socket, &QTcpSocket::disconnected, this,
                        [this, socket]() { received_ += socket->readAll(); });
            }
        });
    }

    [[nodiscard]] bool isListening() const { return server_.isListening(); }
    [[nodiscard]] quint16 port() const { return server_.serverPort(); }
    [[nodiscard]] QByteArray received() const { return received_; }

private:
    QTcpServer server_;
    QByteArray received_;
};

#endif  // FAKESTREAMCONTROLPORT_H
