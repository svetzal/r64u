/**
 * @file test_udpstreamsocket.cpp
 * @brief Unit tests for UdpStreamSocket — the shared UDP socket lifecycle helper.
 *
 * Tests cover:
 * - bind() to OS-assigned port (port 0), isActive() becomes true, port() non-zero
 * - close() makes isActive() return false
 * - packetReceived() is emitted for datagrams matching expectedPacketSize
 * - Datagrams with wrong size are not forwarded
 */

#include "services/udpstreamsocket.h"

#include <QHostAddress>
#include <QSignalSpy>
#include <QUdpSocket>
#include <QtTest>

class TestUdpStreamSocket : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        // 1 MB receive buffer, 770-byte expected packet size (audio)
        socket_ = new UdpStreamSocket(1024 * 1024, 770, this);
    }

    void cleanup()
    {
        socket_->close();
        delete socket_;
        socket_ = nullptr;
    }

    // =========================================================
    // Lifecycle: bind / isActive / port / close
    // =========================================================

    void testInitialState_NotActive() { QVERIFY(!socket_->isActive()); }

    void testInitialState_PortZero() { QCOMPARE(socket_->port(), static_cast<quint16>(0)); }

    void testBind_IsActiveAfterBind()
    {
        QVERIFY(socket_->bind(0));  // port 0: OS picks a free port
        QVERIFY(socket_->isActive());
    }

    void testBind_PortNonZeroAfterBind()
    {
        QVERIFY(socket_->bind(0));
        QVERIFY(socket_->port() != 0);
    }

    void testClose_NotActiveAfterClose()
    {
        QVERIFY(socket_->bind(0));
        QVERIFY(socket_->isActive());
        socket_->close();
        QVERIFY(!socket_->isActive());
    }

    // =========================================================
    // Packet reception
    // =========================================================

    void testPacketReceived_EmittedForCorrectSize()
    {
        QVERIFY(socket_->bind(0));
        quint16 listenPort = socket_->port();

        QSignalSpy spy(socket_, &UdpStreamSocket::packetReceived);

        // Send a 770-byte datagram to ourselves
        QUdpSocket sender;
        QByteArray pkt(770, static_cast<char>(0xAA));
        sender.writeDatagram(pkt, QHostAddress::LocalHost, listenPort);

        QVERIFY(spy.wait(500));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toByteArray(), pkt);
    }

    void testPacketReceived_NotEmittedForWrongSize()
    {
        QVERIFY(socket_->bind(0));
        quint16 listenPort = socket_->port();

        QSignalSpy spy(socket_, &UdpStreamSocket::packetReceived);

        // Send a datagram that does NOT match expected size
        QUdpSocket sender;
        QByteArray pkt(512, static_cast<char>(0xBB));
        sender.writeDatagram(pkt, QHostAddress::LocalHost, listenPort);

        // Give the event loop a moment; the signal should NOT fire
        QTest::qWait(100);
        QCOMPARE(spy.count(), 0);
    }

private:
    UdpStreamSocket *socket_ = nullptr;
};

QTEST_MAIN(TestUdpStreamSocket)
#include "test_udpstreamsocket.moc"
