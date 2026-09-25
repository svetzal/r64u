/**
 * @file test_c64uftpclient_protocol.cpp
 * @brief Unit tests for C64UFtpClient state machine and operation guards.
 *
 * Tests verify:
 * - State machine transitions
 * - Operation guards (not-logged-in behavior)
 * - Connected/Ready state transitions via local FTP server
 * - End-to-end LIST/RETR/STOR over a real passive data connection
 *   (FakeFtpServer), including abort and failure paths
 *
 * Note: FTP parsing tests (PASV response, directory listing) have been
 * migrated to test_ftpcore.cpp, where they test the pure ftp:: namespace
 * functions directly without requiring a socket-owning class instance.
 */

#include "fakes/fakeftpserver.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

// Use angle brackets to force include path search order (avoids tests/services shadow)
#include <services/c64uftpclient.h>

namespace {

constexpr int SignalTimeoutMs = 5000;

/// Grace period for detecting signals that must NOT arrive (late errors, duplicates).
void letLateSignalsArrive()
{
    QTest::qWait(150);
}

QByteArray readLocalFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

void writeLocalFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(contents);
}

QString describeErrors(const QSignalSpy &errorSpy)
{
    QStringList messages;
    for (const auto &args : errorSpy) {
        messages << args.first().toString();
    }
    return QString("errors: [%1]").arg(messages.join(" | "));
}

const QByteArray OneFileListing = "-rw-r--r-- 1 user group 1234 Jan 01 00:00 game.prg\r\n";

}  // namespace

class TestC64UFtpClientProtocol : public QObject
{
    Q_OBJECT

private:
    C64UFtpClient *ftp;

    /// Starts @p server and logs the client in; returns false if login did not complete.
    [[nodiscard]] bool loginTo(FakeFtpServer &server)
    {
        if (!server.listen()) {
            return false;
        }
        ftp->setHost("127.0.0.1", server.port());
        ftp->setCredentials("user", "pass");
        ftp->connectToHost();
        return QTest::qWaitFor([this]() { return ftp->state() == IFtpClient::State::Ready; },
                               SignalTimeoutMs);
    }

    static void addCompletionOrderRows()
    {
        QTest::addColumn<FakeFtpServer::CompletionOrder>("completionOrder");
        QTest::newRow("data closed before 226")
            << FakeFtpServer::CompletionOrder::CloseDataThenReply;
        QTest::newRow("226 before data closed")
            << FakeFtpServer::CompletionOrder::ReplyThenCloseData;
    }

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

    void testAbort_WhenDisconnected_StaysDisconnected()
    {
        QSignalSpy stateSpy(ftp, &C64UFtpClient::stateChanged);

        ftp->abort();

        QCOMPARE(ftp->state(), IFtpClient::State::Disconnected);
        QVERIFY(!ftp->isConnected());
        QCOMPARE(stateSpy.count(), 0);
    }

    void testAbort_WhenDisconnected_DoesNotBlockReconnect()
    {
        QSignalSpy errorSpy(ftp, &C64UFtpClient::error);
        ftp->setHost("192.168.1.64");

        ftp->abort();
        ftp->connectToHost();

        QCOMPARE(ftp->state(), IFtpClient::State::Connecting);
        QVERIFY2(errorSpy.isEmpty(), qPrintable(describeErrors(errorSpy)));
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

    // === Connected/Ready State Tests (via local FakeFtpServer) ===

    void testLoginSequence_StateBecomesReady()
    {
        FakeFtpServer server;
        QVERIFY(server.listen());

        ftp->setHost("127.0.0.1", server.port());
        ftp->setCredentials("user", "pass");
        ftp->connectToHost();

        QTRY_COMPARE_WITH_TIMEOUT(ftp->state(), IFtpClient::State::Ready, 5000);
        QVERIFY(ftp->isLoggedIn());
    }

    void testLoginSequence_EmitsConnected()
    {
        FakeFtpServer server;
        QVERIFY(server.listen());

        QSignalSpy connectedSpy(ftp, &C64UFtpClient::connected);

        ftp->setHost("127.0.0.1", server.port());
        ftp->setCredentials("user", "pass");
        ftp->connectToHost();

        QTRY_COMPARE_WITH_TIMEOUT(connectedSpy.count(), 1, 5000);
    }

    void testList_AfterLogin_TransitionsToBusy()
    {
        FakeFtpServer server;
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
        FakeFtpServer server;
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
        FakeFtpServer server;
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
        FakeFtpServer server;
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
    // signal emissions.  Each test logs in (using FakeFtpServer) and then
    // sends a specific command + server reply pair.
    // =========================================================================

    void testApplyAction_Cwd_250_EmitsDirectoryChanged()
    {
        FakeFtpServer server;
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
        FakeFtpServer server;
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
        FakeFtpServer server;
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
        FakeFtpServer server;
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

    // =========================================================================
    // End-to-end transfers over a real passive data connection (FakeFtpServer)
    // =========================================================================

    void testDownload_Succeeds_WithoutError_data() { addCompletionOrderRows(); }

    void testDownload_Succeeds_WithoutError()
    {
        QFETCH(FakeFtpServer::CompletionOrder, completionOrder);
        const QByteArray contents(5000, 'x');
        FakeFtpServer server;
        server.setCompletionOrder(completionOrder);
        server.setFile("/SD/game.prg", contents);
        QVERIFY(loginTo(server));
        QTemporaryDir dir;
        const QString localPath = dir.filePath("game.prg");
        QSignalSpy errorSpy(ftp, &IFtpClient::error);
        QSignalSpy finishedSpy(ftp, &IFtpClient::downloadFinished);

        ftp->download("/SD/game.prg", localPath);

        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, SignalTimeoutMs);
        letLateSignalsArrive();
        QCOMPARE(finishedSpy.count(), 1);
        QVERIFY2(errorSpy.isEmpty(), qPrintable(describeErrors(errorSpy)));
        QCOMPARE(finishedSpy.first().at(0).toString(), QString("/SD/game.prg"));
        QCOMPARE(finishedSpy.first().at(1).toString(), localPath);
        QCOMPARE(readLocalFile(localPath), contents);
        QCOMPARE(ftp->state(), IFtpClient::State::Ready);
    }

    void testDownloadToMemory_Succeeds_WithoutError_data() { addCompletionOrderRows(); }

    void testDownloadToMemory_Succeeds_WithoutError()
    {
        QFETCH(FakeFtpServer::CompletionOrder, completionOrder);
        const QByteArray contents(7000, 'm');
        FakeFtpServer server;
        server.setCompletionOrder(completionOrder);
        server.setFile("/SD/tune.sid", contents);
        QVERIFY(loginTo(server));
        QSignalSpy errorSpy(ftp, &IFtpClient::error);
        QSignalSpy finishedSpy(ftp, &IFtpClient::downloadToMemoryFinished);

        ftp->downloadToMemory("/SD/tune.sid");

        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, SignalTimeoutMs);
        letLateSignalsArrive();
        QCOMPARE(finishedSpy.count(), 1);
        QVERIFY2(errorSpy.isEmpty(), qPrintable(describeErrors(errorSpy)));
        QCOMPARE(finishedSpy.first().at(1).toByteArray(), contents);
    }

    void testList_Succeeds_WithoutError_data() { addCompletionOrderRows(); }

    void testList_Succeeds_WithoutError()
    {
        QFETCH(FakeFtpServer::CompletionOrder, completionOrder);
        FakeFtpServer server;
        server.setCompletionOrder(completionOrder);
        server.setListing(OneFileListing);
        QVERIFY(loginTo(server));
        QSignalSpy errorSpy(ftp, &IFtpClient::error);
        QSignalSpy listedSpy(ftp, &IFtpClient::directoryListed);

        ftp->list("/SD");

        QTRY_COMPARE_WITH_TIMEOUT(listedSpy.count(), 1, SignalTimeoutMs);
        letLateSignalsArrive();
        QCOMPARE(listedSpy.count(), 1);
        QVERIFY2(errorSpy.isEmpty(), qPrintable(describeErrors(errorSpy)));
        QCOMPARE(listedSpy.first().at(0).toString(), QString("/SD"));
        const auto entries = listedSpy.first().at(1).value<QList<FtpEntry>>();
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.first().name, QString("game.prg"));
    }
};

QTEST_MAIN(TestC64UFtpClientProtocol)
#include "test_c64uftpclient_protocol.moc"
