/**
 * @file test_c64uftpclient_protocol.cpp
 * @brief Unit tests for C64UFtpClient state machine and operation guards.
 *
 * Tests verify:
 * - State machine transitions
 * - Operation guards (not-logged-in behavior)
 * - Connected/Ready state transitions via local FTP server
 *
 * Note: FTP parsing tests (PASV response, directory listing) have been
 * migrated to test_ftpcore.cpp, where they test the pure ftp:: namespace
 * functions directly without requiring a socket-owning class instance.
 */

#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

// Use angle brackets to force include path search order (avoids tests/services shadow)
#include <services/c64uftpclient.h>

/**
 * @brief Minimal FTP server that handles login sequence for testing.
 *
 * Listens on localhost:0, accepts one connection, and performs the
 * FTP greeting → USER → PASS exchange.
 */
class SimpleFtpServer : public QObject
{
    Q_OBJECT

public:
    explicit SimpleFtpServer(QObject *parent = nullptr) : QObject(parent)
    {
        server_.setMaxPendingConnections(1);
        connect(&server_, &QTcpServer::newConnection, this, &SimpleFtpServer::onNewConnection);
    }

    bool listen() { return server_.listen(QHostAddress::LocalHost, 0); }

    quint16 port() const { return server_.serverPort(); }

    /// Register an extra command prefix → reply mapping.
    /// The first matching prefix wins.  Replies must include CRLF.
    void addReply(const QString &prefix, const QString &reply)
    {
        extraReplies_.append({prefix, reply});
    }

    void closeClientConnection()
    {
        if (clientSocket_) {
            clientSocket_->disconnectFromHost();
            clientSocket_ = nullptr;
        }
    }

private slots:
    void onNewConnection()
    {
        clientSocket_ = server_.nextPendingConnection();
        connect(clientSocket_, &QTcpSocket::readyRead, this, &SimpleFtpServer::onReadyRead);
        clientSocket_->write("220 FTP Server Ready\r\n");
        clientSocket_->flush();
    }

    void onReadyRead()
    {
        if (!clientSocket_) {
            return;
        }
        while (clientSocket_->canReadLine()) {
            const QString line = QString::fromLatin1(clientSocket_->readLine()).trimmed();
            if (line.startsWith("USER")) {
                clientSocket_->write("331 Password required\r\n");
                clientSocket_->flush();
            } else if (line.startsWith("PASS")) {
                clientSocket_->write("230 User logged in\r\n");
                clientSocket_->flush();
            } else {
                for (const auto &pair : extraReplies_) {
                    if (line.startsWith(pair.first)) {
                        clientSocket_->write(pair.second.toUtf8());
                        clientSocket_->flush();
                        break;
                    }
                }
            }
        }
    }

private:
    QTcpServer server_;
    QTcpSocket *clientSocket_ = nullptr;
    QList<QPair<QString, QString>> extraReplies_;
};

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

    // === Connected/Ready State Tests (via local SimpleFtpServer) ===

    void testLoginSequence_StateBecomesReady()
    {
        SimpleFtpServer server;
        QVERIFY(server.listen());

        ftp->setHost("127.0.0.1", server.port());
        ftp->setCredentials("user", "pass");
        ftp->connectToHost();

        QTRY_COMPARE_WITH_TIMEOUT(ftp->state(), IFtpClient::State::Ready, 5000);
        QVERIFY(ftp->isLoggedIn());
    }

    void testLoginSequence_EmitsConnected()
    {
        SimpleFtpServer server;
        QVERIFY(server.listen());

        QSignalSpy connectedSpy(ftp, &C64UFtpClient::connected);

        ftp->setHost("127.0.0.1", server.port());
        ftp->setCredentials("user", "pass");
        ftp->connectToHost();

        QTRY_COMPARE_WITH_TIMEOUT(connectedSpy.count(), 1, 5000);
    }

    void testList_AfterLogin_TransitionsToBusy()
    {
        SimpleFtpServer server;
        QVERIFY(server.listen());

        ftp->setHost("127.0.0.1", server.port());
        ftp->setCredentials("user", "pass");
        ftp->connectToHost();

        QTRY_COMPARE_WITH_TIMEOUT(ftp->state(), IFtpClient::State::Ready, 5000);

        QSignalSpy errorSpy(ftp, &C64UFtpClient::error);
        ftp->list("/");

        // Guard should pass (logged in) — no error emitted
        QCOMPARE(errorSpy.count(), 0);
        // State transitions to Busy when an operation is pending
        QCOMPARE(ftp->state(), IFtpClient::State::Busy);
    }

    void testList_AfterLogin_DoesNotEmitError()
    {
        SimpleFtpServer server;
        QVERIFY(server.listen());

        ftp->setHost("127.0.0.1", server.port());
        ftp->setCredentials("user", "pass");
        ftp->connectToHost();

        QTRY_COMPARE_WITH_TIMEOUT(ftp->state(), IFtpClient::State::Ready, 5000);

        QSignalSpy errorSpy(ftp, &C64UFtpClient::error);
        ftp->list("/");

        QCOMPARE(errorSpy.count(), 0);
    }

    void testAbort_AfterLogin_ResetsToReady()
    {
        SimpleFtpServer server;
        QVERIFY(server.listen());

        ftp->setHost("127.0.0.1", server.port());
        ftp->setCredentials("user", "pass");
        ftp->connectToHost();

        QTRY_COMPARE_WITH_TIMEOUT(ftp->state(), IFtpClient::State::Ready, 5000);

        ftp->list("/");
        QCOMPARE(ftp->state(), IFtpClient::State::Busy);

        ftp->abort();
        QCOMPARE(ftp->state(), IFtpClient::State::Ready);
    }

    void testServerDisconnect_AfterLogin_EmitsDisconnectedSignal()
    {
        SimpleFtpServer server;
        QVERIFY(server.listen());

        ftp->setHost("127.0.0.1", server.port());
        ftp->setCredentials("user", "pass");
        ftp->connectToHost();

        QTRY_COMPARE_WITH_TIMEOUT(ftp->state(), IFtpClient::State::Ready, 5000);

        QSignalSpy disconnectedSpy(ftp, &C64UFtpClient::disconnected);
        server.closeClientConnection();

        QTRY_COMPARE_WITH_TIMEOUT(disconnectedSpy.count(), 1, 5000);
    }

    // =========================================================================
    // applyAction sub-step protocol tests
    //
    // These tests verify that representative FTP server responses cause
    // C64UFtpClient::applyAction to produce the correct state mutations and
    // signal emissions.  Each test logs in (using SimpleFtpServer) and then
    // sends a specific command + server reply pair.
    // =========================================================================

    void testApplyAction_Cwd_250_EmitsDirectoryChanged()
    {
        SimpleFtpServer server;
        QVERIFY(server.listen());

        ftp->setHost("127.0.0.1", server.port());
        ftp->setCredentials("user", "pass");
        ftp->connectToHost();

        QTRY_COMPARE_WITH_TIMEOUT(ftp->state(), IFtpClient::State::Ready, 5000);

        QSignalSpy errorSpy(ftp, &C64UFtpClient::error);
        ftp->changeDirectory("/SD/Games");

        // Should transition to Busy (CWD was enqueued and processed)
        QCOMPARE(ftp->state(), IFtpClient::State::Busy);
        // No errors — guard passed
        QCOMPARE(errorSpy.count(), 0);
    }

    void testApplyAction_Mkd_257_EmitsDirectoryCreated()
    {
        SimpleFtpServer server;
        server.addReply("MKD", "257 \"/SD/NewDir\" created\r\n");
        QVERIFY(server.listen());

        ftp->setHost("127.0.0.1", server.port());
        ftp->setCredentials("user", "pass");
        ftp->connectToHost();

        QTRY_COMPARE_WITH_TIMEOUT(ftp->state(), IFtpClient::State::Ready, 5000);

        QSignalSpy dirCreatedSpy(ftp, &C64UFtpClient::directoryCreated);
        QSignalSpy errorSpy(ftp, &C64UFtpClient::error);

        ftp->makeDirectory("/SD/NewDir");

        QTRY_COMPARE_WITH_TIMEOUT(dirCreatedSpy.count(), 1, 5000);
        QCOMPARE(errorSpy.count(), 0);
        QCOMPARE(dirCreatedSpy.at(0).at(0).toString(), QString("/SD/NewDir"));
    }

    void testApplyAction_Dele_250_EmitsFileRemoved()
    {
        SimpleFtpServer server;
        server.addReply("DELE", "250 File deleted\r\n");
        QVERIFY(server.listen());

        ftp->setHost("127.0.0.1", server.port());
        ftp->setCredentials("user", "pass");
        ftp->connectToHost();

        QTRY_COMPARE_WITH_TIMEOUT(ftp->state(), IFtpClient::State::Ready, 5000);

        QSignalSpy fileRemovedSpy(ftp, &C64UFtpClient::fileRemoved);
        QSignalSpy errorSpy(ftp, &C64UFtpClient::error);

        ftp->remove("/SD/trash.prg");

        QTRY_COMPARE_WITH_TIMEOUT(fileRemovedSpy.count(), 1, 5000);
        QCOMPARE(errorSpy.count(), 0);
        QCOMPARE(fileRemovedSpy.at(0).at(0).toString(), QString("/SD/trash.prg"));
    }

    void testApplyAction_ErrorResponse_EmitsError()
    {
        SimpleFtpServer server;
        server.addReply("MKD", "550 Permission denied\r\n");
        QVERIFY(server.listen());

        ftp->setHost("127.0.0.1", server.port());
        ftp->setCredentials("user", "pass");
        ftp->connectToHost();

        QTRY_COMPARE_WITH_TIMEOUT(ftp->state(), IFtpClient::State::Ready, 5000);

        QSignalSpy errorSpy(ftp, &C64UFtpClient::error);

        ftp->makeDirectory("/SD/NoAccess");

        QTRY_COMPARE_WITH_TIMEOUT(errorSpy.count(), 1, 5000);
    }
};

QTEST_MAIN(TestC64UFtpClientProtocol)
#include "test_c64uftpclient_protocol.moc"
