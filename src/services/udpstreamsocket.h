#ifndef UDPSTREAMSOCKET_H
#define UDPSTREAMSOCKET_H

#include <QObject>
#include <QUdpSocket>

/**
 * @brief Shared UDP socket lifecycle helper for stream receivers.
 *
 * Owns a QUdpSocket and handles bind/close/isActive/port/readyRead in one
 * place so that AudioStreamReceiver and VideoStreamReceiver do not duplicate
 * this boilerplate.
 *
 * For each received datagram whose size matches expectedPacketSize_, the
 * packetReceived() signal is emitted with the raw payload bytes.  Datagrams
 * with a different size are silently ignored.
 *
 * Socket errors are forwarded as socketError(QString).
 */
class UdpStreamSocket : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a UdpStreamSocket.
     * @param receiveBufferSizeBytes OS-level receive buffer size hint (bytes).
     * @param expectedPacketSize     Only datagrams of exactly this size are
     *                               forwarded via packetReceived().
     * @param parent                 Optional Qt parent for ownership.
     */
    explicit UdpStreamSocket(int receiveBufferSizeBytes, int expectedPacketSize,
                             QObject *parent = nullptr);

    /**
     * @brief Binds the socket to the given port on all interfaces.
     *
     * Sets the OS receive-buffer option, then calls socket->bind().
     * Emits socketError() and returns false on failure.
     *
     * @param port UDP port to listen on (0 lets the OS choose a free port).
     * @return true on success.
     */
    bool bind(quint16 port);

    /**
     * @brief Closes the socket if it is not already in UnconnectedState.
     */
    void close();

    /**
     * @brief Returns true when the socket is in BoundState.
     */
    [[nodiscard]] bool isActive() const;

    /**
     * @brief Returns the local port the socket is bound to, or 0 if unbound.
     */
    [[nodiscard]] quint16 port() const;

signals:
    /**
     * @brief Emitted for each received datagram whose size matches
     *        expectedPacketSize_.
     * @param packet Raw datagram bytes.
     */
    void packetReceived(const QByteArray &packet);

    /**
     * @brief Emitted when a socket-level error occurs.
     * @param error Human-readable error description.
     */
    void socketError(const QString &error);

private slots:
    void onReadyRead();

private:
    QUdpSocket *socket_ = nullptr;
    int receiveBufferSizeBytes_;
    int expectedPacketSize_;
};

#endif  // UDPSTREAMSOCKET_H
