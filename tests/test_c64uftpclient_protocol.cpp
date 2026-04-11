/**
 * @file test_c64uftpclient_protocol.cpp
 * @brief Unit tests for C64UFtpClient state machine and operation guards.
 *
 * Tests verify:
 * - State machine transitions
 * - Operation guards (not-logged-in behavior)
 *
 * Note: FTP parsing tests (PASV response, directory listing) have been
 * migrated to test_ftpcore.cpp, where they test the pure ftp:: namespace
 * functions directly without requiring a socket-owning class instance.
 */

#include <QSignalSpy>
#include <QTcpSocket>
#include <QtTest>

// Use angle brackets to force include path search order (avoids tests/services shadow)
#include <services/c64uftpclient.h>

class TestC64UFtpClientProtocol : public QObject
{
    Q_OBJECT

private:
    C64UFtpClient *ftp;

private slots:
    void init() { ftp = new C64UFtpClient(this); }

    void cleanup()
    {
        delete ftp;
        ftp = nullptr;
    }

    // === Initial State Tests ===

    void testInitialState_Disconnected()
    {
        QCOMPARE(ftp->state(), IFtpClient::State::Disconnected);
    }

    void testInitialState_NotConnected() { QVERIFY(!ftp->isConnected()); }

    void testInitialState_NotLoggedIn() { QVERIFY(!ftp->isLoggedIn()); }

    void testInitialState_DefaultDirectory() { QCOMPARE(ftp->currentDirectory(), QString("/")); }

    void testInitialState_NoHost() { QCOMPARE(ftp->host(), QString("")); }

    // === Host Configuration Tests ===

    void testSetHost_UpdatesHost()
    {
        ftp->setHost("192.168.1.64");
        QCOMPARE(ftp->host(), QString("192.168.1.64"));
    }

    void testSetHost_WithCustomPort()
    {
        ftp->setHost("192.168.1.64", 2121);
        QCOMPARE(ftp->host(), QString("192.168.1.64"));
        // Port is stored internally, not exposed via getter
    }

    void testSetHost_CanChangeHost()
    {
        ftp->setHost("192.168.1.1");
        QCOMPARE(ftp->host(), QString("192.168.1.1"));

        ftp->setHost("10.0.0.1");
        QCOMPARE(ftp->host(), QString("10.0.0.1"));
    }

    // === Connection State Tests ===

    void testConnectToHost_EmitsError_WhenAlreadyConnecting()
    {
        QSignalSpy errorSpy(ftp, &C64UFtpClient::error);

        ftp->setHost("192.168.1.64");
        ftp->connectToHost();

        // State should be Connecting
        QCOMPARE(ftp->state(), IFtpClient::State::Connecting);

        // Try to connect again
        ftp->connectToHost();

        // Should emit error
        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.first().first().toString().contains("already"));
    }

    void testConnectToHost_ChangesState_ToConnecting()
    {
        QSignalSpy stateSpy(ftp, &C64UFtpClient::stateChanged);

        ftp->setHost("192.168.1.64");
        ftp->connectToHost();

        QCOMPARE(ftp->state(), IFtpClient::State::Connecting);
        QCOMPARE(stateSpy.count(), 1);
    }

    void testDisconnect_WhenDisconnected_NoOp()
    {
        QSignalSpy disconnectedSpy(ftp, &C64UFtpClient::disconnected);

        // Should be a no-op when already disconnected
        ftp->disconnect();

        QCOMPARE(ftp->state(), IFtpClient::State::Disconnected);
        QCOMPARE(disconnectedSpy.count(), 0);
    }

    // === Operation Guard Tests ===

    void testList_EmitsError_WhenNotLoggedIn()
    {
        QSignalSpy errorSpy(ftp, &C64UFtpClient::error);

        ftp->list("/some/path");

        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.first().first().toString().contains("not connected"));
    }

    void testChangeDirectory_EmitsError_WhenNotLoggedIn()
    {
        QSignalSpy errorSpy(ftp, &C64UFtpClient::error);

        ftp->changeDirectory("/some/path");

        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.first().first().toString().contains("not connected"));
    }

    void testMakeDirectory_EmitsError_WhenNotLoggedIn()
    {
        QSignalSpy errorSpy(ftp, &C64UFtpClient::error);

        ftp->makeDirectory("/new/dir");

        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.first().first().toString().contains("not connected"));
    }

    void testRemoveDirectory_EmitsError_WhenNotLoggedIn()
    {
        QSignalSpy errorSpy(ftp, &C64UFtpClient::error);

        ftp->removeDirectory("/some/dir");

        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.first().first().toString().contains("not connected"));
    }

    void testDownload_EmitsError_WhenNotLoggedIn()
    {
        QSignalSpy errorSpy(ftp, &C64UFtpClient::error);

        ftp->download("/remote/file.txt", "/local/file.txt");

        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.first().first().toString().contains("not connected"));
    }

    void testDownloadToMemory_EmitsError_WhenNotLoggedIn()
    {
        QSignalSpy errorSpy(ftp, &C64UFtpClient::error);

        ftp->downloadToMemory("/remote/file.txt");

        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.first().first().toString().contains("not connected"));
    }

    void testUpload_EmitsError_WhenNotLoggedIn()
    {
        QSignalSpy errorSpy(ftp, &C64UFtpClient::error);

        ftp->upload("/local/file.txt", "/remote/file.txt");

        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.first().first().toString().contains("not connected"));
    }

    void testRemove_EmitsError_WhenNotLoggedIn()
    {
        QSignalSpy errorSpy(ftp, &C64UFtpClient::error);

        ftp->remove("/some/file.txt");

        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.first().first().toString().contains("not connected"));
    }

    void testRename_EmitsError_WhenNotLoggedIn()
    {
        QSignalSpy errorSpy(ftp, &C64UFtpClient::error);

        ftp->rename("/old/path", "/new/path");

        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.first().first().toString().contains("not connected"));
    }

    // === Abort Tests ===

    void testAbort_WhenDisconnected_SetsReady()
    {
        // Currently abort() sets state to Ready even when disconnected.
        // This may be unexpected behavior - abort when disconnected could
        // arguably be a no-op. Documenting current behavior.
        ftp->abort();
        // Note: This sets Ready even though we're not actually connected
        QCOMPARE(ftp->state(), IFtpClient::State::Ready);
    }

    // === Connection Timeout Tests ===

    void testConnectionTimeoutClearsTransferState()
    {
        QSignalSpy errorSpy(ftp, &C64UFtpClient::error);

        ftp->setHost("192.168.1.64");
        ftp->connectToHost();

        QCOMPARE(ftp->state(), IFtpClient::State::Connecting);

        // Trigger the timeout slot directly (simulates the timer expiring)
        QMetaObject::invokeMethod(ftp, "onConnectionTimeout");

        QCOMPARE(ftp->state(), IFtpClient::State::Disconnected);
        QVERIFY(!ftp->isLoggedIn());
        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.first().first().toString().contains("timed out"));
    }

    // === IsConnected Logic Tests ===

    void testIsConnected_FalseWhenDisconnected() { QVERIFY(!ftp->isConnected()); }

    void testIsConnected_FalseWhenConnecting()
    {
        ftp->setHost("192.168.1.64");
        ftp->connectToHost();

        QCOMPARE(ftp->state(), IFtpClient::State::Connecting);
        QVERIFY(!ftp->isConnected());
    }
};

QTEST_MAIN(TestC64UFtpClientProtocol)
#include "test_c64uftpclient_protocol.moc"
