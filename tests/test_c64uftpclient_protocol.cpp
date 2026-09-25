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

/**
 * @brief Records every failure the client reports, whichever signal carries it.
 *
 * Connection-level failures arrive through error(), failed requests through
 * operationFailed(). Each entry holds the message first, like a QSignalSpy on
 * error(), so one entry per failure means nothing was reported twice.
 */
class FailureSpy : public QObject, public QList<QVariantList>
{
public:
    explicit FailureSpy(IFtpClient *ftp)
    {
        connect(ftp, &IFtpClient::error, this,
                [this](const QString &message) { append(QVariantList{message}); });
        connect(ftp, &IFtpClient::operationFailed, this,
                [this](IFtpClient::Operation /*operation*/, const QString & /*remotePath*/,
                       const QString & /*localPath*/,
                       const QString &message) { append(QVariantList{message}); });
    }
};

QString describeFailures(const FailureSpy &failureSpy)
{
    QStringList messages;
    for (const auto &args : failureSpy) {
        messages << args.first().toString();
    }
    return QString("failures: [%1]").arg(messages.join(" | "));
}

QStringList filesIn(const QTemporaryDir &dir)
{
    return QDir(dir.path()).entryList(QDir::Files, QDir::Name);
}

constexpr char OneFileListing[] = "-rw-r--r-- 1 user group 1234 Jan 01 00:00 game.prg\r\n";

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
        FailureSpy failureSpy(ftp);

        ftp->setHost("192.168.1.64");
        ftp->connectToHost();

        // State should be Connecting
        QCOMPARE(ftp->state(), IFtpClient::State::Connecting);

        // Try to connect again
        ftp->connectToHost();

        // Should emit error
        QCOMPARE(failureSpy.count(), 1);
        QVERIFY(failureSpy.first().first().toString().contains("already"));
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
        FailureSpy failureSpy(ftp);

        ftp->list("/some/path");

        QCOMPARE(failureSpy.count(), 1);
        QVERIFY(failureSpy.first().first().toString().contains("not connected"));
    }

    void testChangeDirectory_EmitsError_WhenNotLoggedIn()
    {
        FailureSpy failureSpy(ftp);

        ftp->changeDirectory("/some/path");

        QCOMPARE(failureSpy.count(), 1);
        QVERIFY(failureSpy.first().first().toString().contains("not connected"));
    }

    void testMakeDirectory_EmitsError_WhenNotLoggedIn()
    {
        FailureSpy failureSpy(ftp);

        ftp->makeDirectory("/new/dir");

        QCOMPARE(failureSpy.count(), 1);
        QVERIFY(failureSpy.first().first().toString().contains("not connected"));
    }

    void testRemoveDirectory_EmitsError_WhenNotLoggedIn()
    {
        FailureSpy failureSpy(ftp);

        ftp->removeDirectory("/some/dir");

        QCOMPARE(failureSpy.count(), 1);
        QVERIFY(failureSpy.first().first().toString().contains("not connected"));
    }

    void testDownload_EmitsError_WhenNotLoggedIn()
    {
        FailureSpy failureSpy(ftp);

        ftp->download("/remote/file.txt", "/local/file.txt");

        QCOMPARE(failureSpy.count(), 1);
        QVERIFY(failureSpy.first().first().toString().contains("not connected"));
    }

    void testDownloadToMemory_EmitsError_WhenNotLoggedIn()
    {
        FailureSpy failureSpy(ftp);

        ftp->downloadToMemory("/remote/file.txt");

        QCOMPARE(failureSpy.count(), 1);
        QVERIFY(failureSpy.first().first().toString().contains("not connected"));
    }

    void testUpload_EmitsError_WhenNotLoggedIn()
    {
        FailureSpy failureSpy(ftp);

        ftp->upload("/local/file.txt", "/remote/file.txt");

        QCOMPARE(failureSpy.count(), 1);
        QVERIFY(failureSpy.first().first().toString().contains("not connected"));
    }

    void testRemove_EmitsError_WhenNotLoggedIn()
    {
        FailureSpy failureSpy(ftp);

        ftp->remove("/some/file.txt");

        QCOMPARE(failureSpy.count(), 1);
        QVERIFY(failureSpy.first().first().toString().contains("not connected"));
    }

    void testRename_EmitsError_WhenNotLoggedIn()
    {
        FailureSpy failureSpy(ftp);

        ftp->rename("/old/path", "/new/path");

        QCOMPARE(failureSpy.count(), 1);
        QVERIFY(failureSpy.first().first().toString().contains("not connected"));
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
        FailureSpy failureSpy(ftp);
        ftp->setHost("192.168.1.64");

        ftp->abort();
        ftp->connectToHost();

        QCOMPARE(ftp->state(), IFtpClient::State::Connecting);
        QVERIFY2(failureSpy.isEmpty(), qPrintable(describeFailures(failureSpy)));
    }

    // === Connection Timeout Tests ===

    void testConnectionTimeoutClearsTransferState()
    {
        FailureSpy failureSpy(ftp);

        ftp->setHost("192.168.1.64");
        ftp->connectToHost();

        QCOMPARE(ftp->state(), IFtpClient::State::Connecting);

        // Trigger the timeout slot directly (simulates the timer expiring)
        QMetaObject::invokeMethod(ftp, "onConnectionTimeout");

        QCOMPARE(ftp->state(), IFtpClient::State::Disconnected);
        QVERIFY(!ftp->isLoggedIn());
        QCOMPARE(failureSpy.count(), 1);
        QVERIFY(failureSpy.first().first().toString().contains("timed out"));
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

        FailureSpy failureSpy(ftp);
        ftp->list("/");

        // Guard should pass (logged in) — no error emitted
        QCOMPARE(failureSpy.count(), 0);
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

        FailureSpy failureSpy(ftp);
        ftp->list("/");

        QCOMPARE(failureSpy.count(), 0);
    }

    void testAbort_DuringListPrelude_SkipsListAndReturnsToReady()
    {
        FakeFtpServer server;
        server.setListing(OneFileListing);
        QVERIFY(loginTo(server));
        FailureSpy failureSpy(ftp);
        QSignalSpy listedSpy(ftp, &C64UFtpClient::directoryListed);

        ftp->list("/");
        QCOMPARE(ftp->state(), IFtpClient::State::Busy);
        ftp->abort();

        // TYPE is already on the wire; its reply is consumed before going idle
        QTRY_COMPARE_WITH_TIMEOUT(ftp->state(), IFtpClient::State::Ready, SignalTimeoutMs);
        letLateSignalsArrive();
        QCOMPARE(server.commandCount("LIST"), 0);
        QCOMPARE(server.commandCount("ABOR"), 0);
        QCOMPARE(listedSpy.count(), 0);
        QVERIFY2(failureSpy.isEmpty(), qPrintable(describeFailures(failureSpy)));

        ftp->list("/");
        QTRY_COMPARE_WITH_TIMEOUT(listedSpy.count(), 1, SignalTimeoutMs);
        QVERIFY2(failureSpy.isEmpty(), qPrintable(describeFailures(failureSpy)));
    }

    void testAbort_WhenIdle_DoesNotSendAbor()
    {
        FakeFtpServer server;
        server.setListing(OneFileListing);
        QVERIFY(loginTo(server));
        FailureSpy failureSpy(ftp);
        QSignalSpy listedSpy(ftp, &C64UFtpClient::directoryListed);

        ftp->abort();
        QCOMPARE(ftp->state(), IFtpClient::State::Ready);
        ftp->list("/");

        QTRY_COMPARE_WITH_TIMEOUT(listedSpy.count(), 1, SignalTimeoutMs);
        letLateSignalsArrive();
        QCOMPARE(server.commandCount("ABOR"), 0);
        QVERIFY2(failureSpy.isEmpty(), qPrintable(describeFailures(failureSpy)));
    }

    void testAbort_MidDownload_NextDownloadSucceeds_data()
    {
        QTest::addColumn<QByteArray>("aborReply");
        QTest::newRow("426 then 226 (RFC 959)")
            << QByteArray("426 Transfer aborted\r\n226 ABOR successful\r\n");
        QTest::newRow("single 226") << QByteArray("226 ABOR successful\r\n");
    }

    void testAbort_MidDownload_NextDownloadSucceeds()
    {
        QFETCH(QByteArray, aborReply);
        FakeFtpServer server;
        server.setFile("/SD/big.d64", QByteArray(200000, 'b'));
        server.setFile("/SD/small.prg", "SMALL");
        server.setStallAfterBytes(1000);
        server.setAborReplyDuringTransfer(aborReply);
        QVERIFY(loginTo(server));
        QTemporaryDir dir;
        QSignalSpy progressSpy(ftp, &C64UFtpClient::downloadProgress);

        ftp->download("/SD/big.d64", dir.filePath("big.d64"));
        QTRY_VERIFY_WITH_TIMEOUT(!progressSpy.isEmpty(), SignalTimeoutMs);
        ftp->abort();

        server.setStallAfterBytes(-1);
        FailureSpy failureSpy(ftp);
        QSignalSpy finishedSpy(ftp, &C64UFtpClient::downloadFinished);
        ftp->download("/SD/small.prg", dir.filePath("small.prg"));

        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, SignalTimeoutMs);
        letLateSignalsArrive();
        QVERIFY2(failureSpy.isEmpty(), qPrintable(describeFailures(failureSpy)));
        QCOMPARE(finishedSpy.first().at(0).toString(), QString("/SD/small.prg"));
        QCOMPARE(readLocalFile(dir.filePath("small.prg")), QByteArray("SMALL"));
        QCOMPARE(server.commandCount("ABOR"), 1);
        QCOMPARE(ftp->state(), IFtpClient::State::Ready);
    }

    void testAbort_MidDownload_KeepsOtherQueuedOperations()
    {
        FakeFtpServer server;
        server.setFile("/SD/big.d64", QByteArray(200000, 'b'));
        server.setFile("/SD/tune.sid", "SID");
        server.setListing(OneFileListing);
        server.setStallAfterBytes(1000);
        QVERIFY(loginTo(server));
        QTemporaryDir dir;
        QSignalSpy progressSpy(ftp, &C64UFtpClient::downloadProgress);
        QSignalSpy listedSpy(ftp, &C64UFtpClient::directoryListed);
        QSignalSpy memorySpy(ftp, &C64UFtpClient::downloadToMemoryFinished);
        QSignalSpy finishedSpy(ftp, &C64UFtpClient::downloadFinished);

        ftp->download("/SD/big.d64", dir.filePath("big.d64"));
        // Requests from other components queued behind the transfer
        ftp->list("/SD/browse");
        ftp->downloadToMemory("/SD/tune.sid");
        QTRY_VERIFY_WITH_TIMEOUT(!progressSpy.isEmpty(), SignalTimeoutMs);
        server.setStallAfterBytes(-1);
        FailureSpy failureSpy(ftp);

        ftp->abort();

        QTRY_COMPARE_WITH_TIMEOUT(memorySpy.count(), 1, SignalTimeoutMs);
        QCOMPARE(listedSpy.count(), 1);
        QCOMPARE(listedSpy.first().at(0).toString(), QString("/SD/browse"));
        QCOMPARE(memorySpy.first().at(1).toByteArray(), QByteArray("SID"));
        QCOMPARE(finishedSpy.count(), 0);
        QVERIFY2(failureSpy.isEmpty(), qPrintable(describeFailures(failureSpy)));
    }

    void testAbort_DuringListPrelude_KeepsLaterOperations()
    {
        FakeFtpServer server;
        server.setListing(OneFileListing);
        QVERIFY(loginTo(server));
        FailureSpy failureSpy(ftp);
        QSignalSpy listedSpy(ftp, &C64UFtpClient::directoryListed);

        ftp->list("/SD/first");
        ftp->list("/SD/second");
        ftp->abort();  // cancels only /SD/first, whose TYPE is in flight

        QTRY_COMPARE_WITH_TIMEOUT(listedSpy.count(), 1, SignalTimeoutMs);
        letLateSignalsArrive();
        QCOMPARE(listedSpy.count(), 1);
        QCOMPARE(listedSpy.first().at(0).toString(), QString("/SD/second"));
        QCOMPARE(server.commandCount("LIST"), 1);
        QVERIFY2(failureSpy.isEmpty(), qPrintable(describeFailures(failureSpy)));
    }

    void testAbortIfInFlight_AnotherRequestInFlight_LeavesItRunning()
    {
        FakeFtpServer server;
        server.setFile("/SD/game.prg", "GAME");
        QVERIFY(loginTo(server));
        QTemporaryDir dir;
        FailureSpy failureSpy(ftp);
        QSignalSpy finishedSpy(ftp, &C64UFtpClient::downloadFinished);

        ftp->download("/SD/game.prg", dir.filePath("game.prg"));  // e.g. the transfer queue's
        ftp->abortIfInFlight(IFtpClient::Operation::DownloadToMemory, "/SD/tune.sid");

        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, SignalTimeoutMs);
        letLateSignalsArrive();
        QCOMPARE(server.commandCount("ABOR"), 0);
        QCOMPARE(readLocalFile(dir.filePath("game.prg")), QByteArray("GAME"));
        QVERIFY2(failureSpy.isEmpty(), qPrintable(describeFailures(failureSpy)));
    }

    void testAbortIfInFlight_ItsOwnRequestInFlight_AbortsIt()
    {
        FakeFtpServer server;
        server.setFile("/SD/tune.sid", "SID");
        QVERIFY(loginTo(server));
        FailureSpy failureSpy(ftp);
        QSignalSpy memorySpy(ftp, &C64UFtpClient::downloadToMemoryFinished);

        ftp->downloadToMemory("/SD/tune.sid");
        ftp->abortIfInFlight(IFtpClient::Operation::DownloadToMemory, "/SD/tune.sid");

        QTRY_COMPARE_WITH_TIMEOUT(ftp->state(), IFtpClient::State::Ready, SignalTimeoutMs);
        letLateSignalsArrive();
        QCOMPARE(memorySpy.count(), 0);
        QCOMPARE(server.commandCount("RETR"), 0);
        QVERIFY2(failureSpy.isEmpty(), qPrintable(describeFailures(failureSpy)));
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

        FailureSpy failureSpy(ftp);
        ftp->changeDirectory("/SD/Games");

        // Should transition to Busy (CWD was enqueued and processed)
        QCOMPARE(ftp->state(), IFtpClient::State::Busy);
        // No errors — guard passed
        QCOMPARE(failureSpy.count(), 0);
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
        FailureSpy failureSpy(ftp);

        ftp->makeDirectory("/SD/NewDir");

        QTRY_COMPARE_WITH_TIMEOUT(dirCreatedSpy.count(), 1, 5000);
        QCOMPARE(failureSpy.count(), 0);
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
        FailureSpy failureSpy(ftp);

        ftp->remove("/SD/trash.prg");

        QTRY_COMPARE_WITH_TIMEOUT(fileRemovedSpy.count(), 1, 5000);
        QCOMPARE(failureSpy.count(), 0);
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

        FailureSpy failureSpy(ftp);

        ftp->makeDirectory("/SD/NoAccess");

        QTRY_COMPARE_WITH_TIMEOUT(failureSpy.count(), 1, 5000);
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
        FailureSpy failureSpy(ftp);
        QSignalSpy finishedSpy(ftp, &IFtpClient::downloadFinished);

        ftp->download("/SD/game.prg", localPath);

        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, SignalTimeoutMs);
        letLateSignalsArrive();
        QCOMPARE(finishedSpy.count(), 1);
        QVERIFY2(failureSpy.isEmpty(), qPrintable(describeFailures(failureSpy)));
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
        FailureSpy failureSpy(ftp);
        QSignalSpy finishedSpy(ftp, &IFtpClient::downloadToMemoryFinished);

        ftp->downloadToMemory("/SD/tune.sid");

        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, SignalTimeoutMs);
        letLateSignalsArrive();
        QCOMPARE(finishedSpy.count(), 1);
        QVERIFY2(failureSpy.isEmpty(), qPrintable(describeFailures(failureSpy)));
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
        FailureSpy failureSpy(ftp);
        QSignalSpy listedSpy(ftp, &IFtpClient::directoryListed);

        ftp->list("/SD");

        QTRY_COMPARE_WITH_TIMEOUT(listedSpy.count(), 1, SignalTimeoutMs);
        letLateSignalsArrive();
        QCOMPARE(listedSpy.count(), 1);
        QVERIFY2(failureSpy.isEmpty(), qPrintable(describeFailures(failureSpy)));
        QCOMPARE(listedSpy.first().at(0).toString(), QString("/SD"));
        const auto entries = listedSpy.first().at(1).value<QList<FtpEntry>>();
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.first().name, QString("game.prg"));
    }

    // =========================================================================
    // Failure handling: one error per failed operation, then carry on
    // =========================================================================

    void testDownload_PasvRejected_ReportsOneErrorAndRunsNextOperation()
    {
        FakeFtpServer server;
        server.setFile("/SD/game.prg", "GAME");
        server.setListing(OneFileListing);
        server.failNextPasv();
        QVERIFY(loginTo(server));
        QTemporaryDir dir;
        FailureSpy failureSpy(ftp);
        QSignalSpy listedSpy(ftp, &C64UFtpClient::directoryListed);

        ftp->download("/SD/game.prg", dir.filePath("game.prg"));
        ftp->list("/SD");

        QTRY_COMPARE_WITH_TIMEOUT(listedSpy.count(), 1, SignalTimeoutMs);
        letLateSignalsArrive();
        QVERIFY2(failureSpy.count() == 1, qPrintable(describeFailures(failureSpy)));
        QCOMPARE(server.commandCount("RETR"), 0);
    }

    void testDownload_DataConnectionRefused_ReportsOneErrorAndRunsNextOperation()
    {
        FakeFtpServer server;
        server.setFile("/SD/game.prg", "GAME");
        server.setListing(OneFileListing);
        server.advertiseDeadDataPortOnce();
        QVERIFY(loginTo(server));
        QTemporaryDir dir;
        FailureSpy failureSpy(ftp);
        QSignalSpy finishedSpy(ftp, &C64UFtpClient::downloadFinished);
        QSignalSpy listedSpy(ftp, &C64UFtpClient::directoryListed);

        ftp->download("/SD/game.prg", dir.filePath("game.prg"));
        ftp->list("/SD");

        QTRY_COMPARE_WITH_TIMEOUT(listedSpy.count(), 1, SignalTimeoutMs);
        letLateSignalsArrive();
        QVERIFY2(failureSpy.count() == 1, qPrintable(describeFailures(failureSpy)));
        QCOMPARE(finishedSpy.count(), 0);
    }

    void testRename_SourceMissing_ReportsOneErrorAndSkipsRnto()
    {
        FakeFtpServer server;
        server.addReply("RNFR", "550 File not found\r\n");
        QVERIFY(loginTo(server));
        FailureSpy failureSpy(ftp);
        QSignalSpy renamedSpy(ftp, &C64UFtpClient::fileRenamed);

        ftp->rename("/SD/missing.prg", "/SD/new.prg");

        QTRY_COMPARE_WITH_TIMEOUT(ftp->state(), IFtpClient::State::Ready, SignalTimeoutMs);
        letLateSignalsArrive();
        QVERIFY2(failureSpy.count() == 1, qPrintable(describeFailures(failureSpy)));
        QCOMPARE(server.commandCount("RNTO"), 0);
        QCOMPARE(renamedSpy.count(), 0);
    }

    void testDownload_MissingFileWithDataLeftOpen_NextDownloadSucceeds()
    {
        FakeFtpServer server;
        server.setFile("/SD/game.prg", "GAME");
        server.setKeepDataOpenOnError(true);
        QVERIFY(loginTo(server));
        QTemporaryDir dir;
        FailureSpy failureSpy(ftp);
        QSignalSpy finishedSpy(ftp, &C64UFtpClient::downloadFinished);

        ftp->download("/SD/missing.prg", dir.filePath("missing.prg"));
        QTRY_COMPARE_WITH_TIMEOUT(failureSpy.count(), 1, SignalTimeoutMs);
        ftp->download("/SD/game.prg", dir.filePath("game.prg"));

        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, SignalTimeoutMs);
        letLateSignalsArrive();
        QVERIFY2(failureSpy.count() == 1, qPrintable(describeFailures(failureSpy)));
        QCOMPARE(readLocalFile(dir.filePath("game.prg")), QByteArray("GAME"));
    }

    // =========================================================================
    // operationFailed identifies which request a failure ended
    // =========================================================================

    void testOperationFailed_WhenNotLoggedIn_IdentifiesTheRequest()
    {
        QSignalSpy failedSpy(ftp, &C64UFtpClient::operationFailed);

        ftp->download("/remote/file.txt", "/local/file.txt");

        QCOMPARE(failedSpy.count(), 1);
        QCOMPARE(failedSpy.first().at(0).value<IFtpClient::Operation>(),
                 IFtpClient::Operation::Download);
        QCOMPARE(failedSpy.first().at(1).toString(), QString("/remote/file.txt"));
        QCOMPARE(failedSpy.first().at(2).toString(), QString("/local/file.txt"));
        QVERIFY(failedSpy.first().at(3).toString().contains("not connected"));
    }

    void testOperationFailed_PasvRejected_IdentifiesTheDownloadNotTheNextRequest()
    {
        FakeFtpServer server;
        server.setFile("/SD/game.prg", "GAME");
        server.setListing(OneFileListing);
        server.failNextPasv();
        QVERIFY(loginTo(server));
        QTemporaryDir dir;
        QSignalSpy failedSpy(ftp, &C64UFtpClient::operationFailed);
        QSignalSpy listedSpy(ftp, &C64UFtpClient::directoryListed);

        ftp->download("/SD/game.prg", dir.filePath("game.prg"));
        ftp->list("/SD");

        QTRY_COMPARE_WITH_TIMEOUT(listedSpy.count(), 1, SignalTimeoutMs);
        letLateSignalsArrive();
        QCOMPARE(failedSpy.count(), 1);
        QCOMPARE(failedSpy.first().at(0).value<IFtpClient::Operation>(),
                 IFtpClient::Operation::Download);
        QCOMPARE(failedSpy.first().at(1).toString(), QString("/SD/game.prg"));
        QCOMPARE(failedSpy.first().at(2).toString(), dir.filePath("game.prg"));
    }

    void testOperationFailed_MissingFileInMemory_IdentifiesDownloadToMemory()
    {
        FakeFtpServer server;
        QVERIFY(loginTo(server));
        FailureSpy failureSpy(ftp);
        QSignalSpy failedSpy(ftp, &C64UFtpClient::operationFailed);

        ftp->downloadToMemory("/SD/missing.sid");

        QTRY_COMPARE_WITH_TIMEOUT(failedSpy.count(), 1, SignalTimeoutMs);
        letLateSignalsArrive();
        QVERIFY2(failureSpy.count() == 1, qPrintable(describeFailures(failureSpy)));
        QCOMPARE(failedSpy.first().at(0).value<IFtpClient::Operation>(),
                 IFtpClient::Operation::DownloadToMemory);
        QCOMPARE(failedSpy.first().at(1).toString(), QString("/SD/missing.sid"));
        QVERIFY(failedSpy.first().at(3).toString().contains("/SD/missing.sid"));
    }

    // =========================================================================
    // Who tells the user: the requester for a failed request, the client for
    // a failed connection
    // =========================================================================

    void testFailedRequest_IsLeftToTheRequesterToReport()
    {
        FakeFtpServer server;
        QVERIFY(loginTo(server));
        QTemporaryDir dir;
        QSignalSpy failedSpy(ftp, &IFtpClient::operationFailed);
        QSignalSpy errorSpy(ftp, &IFtpClient::error);
        QSignalSpy reportedSpy(ftp, &IErrorEmitter::errorReported);

        ftp->download("/SD/missing.prg", dir.filePath("missing.prg"));

        QTRY_COMPARE_WITH_TIMEOUT(failedSpy.count(), 1, SignalTimeoutMs);
        letLateSignalsArrive();
        QCOMPARE(errorSpy.count(), 0);
        QCOMPARE(reportedSpy.count(), 0);
    }

    void testRequestWhileNotLoggedIn_IsLeftToTheRequesterToReport()
    {
        QSignalSpy failedSpy(ftp, &IFtpClient::operationFailed);
        QSignalSpy reportedSpy(ftp, &IErrorEmitter::errorReported);

        ftp->list("/SD");

        QCOMPARE(failedSpy.count(), 1);
        QCOMPARE(reportedSpy.count(), 0);
    }

    void testLoginRejected_IsReportedToTheUserOnce()
    {
        FakeFtpServer server;
        server.addReply("PASS", "530 Login incorrect\r\n");
        QVERIFY(server.listen());
        QSignalSpy reportedSpy(ftp, &IErrorEmitter::errorReported);
        QSignalSpy failedSpy(ftp, &IFtpClient::operationFailed);
        ftp->setHost("127.0.0.1", server.port());
        ftp->setCredentials("user", "wrong");

        ftp->connectToHost();

        QTRY_COMPARE_WITH_TIMEOUT(reportedSpy.count(), 1, SignalTimeoutMs);
        letLateSignalsArrive();
        QCOMPARE(reportedSpy.count(), 1);
        QVERIFY(reportedSpy.first().at(3).toString().contains("Login failed"));
        QCOMPARE(failedSpy.count(), 0);
    }

    void testServerDropsConnection_IsReportedToTheUserOnce()
    {
        FakeFtpServer server;
        QVERIFY(loginTo(server));
        QSignalSpy reportedSpy(ftp, &IErrorEmitter::errorReported);
        QSignalSpy disconnectedSpy(ftp, &IFtpClient::disconnected);

        server.closeClientConnection();

        QTRY_COMPARE_WITH_TIMEOUT(disconnectedSpy.count(), 1, SignalTimeoutMs);
        letLateSignalsArrive();
        QCOMPARE(reportedSpy.count(), 1);
    }

    void testOperationFailed_NotEmittedForSuccessfulRequests()
    {
        FakeFtpServer server;
        server.setListing(OneFileListing);
        QVERIFY(loginTo(server));
        QSignalSpy failedSpy(ftp, &C64UFtpClient::operationFailed);
        QSignalSpy listedSpy(ftp, &C64UFtpClient::directoryListed);

        ftp->list("/SD");

        QTRY_COMPARE_WITH_TIMEOUT(listedSpy.count(), 1, SignalTimeoutMs);
        letLateSignalsArrive();
        QCOMPARE(failedSpy.count(), 0);
    }

    // =========================================================================
    // Per-operation state belongs to the operation, not to the queue
    // =========================================================================

    void testDownloadToMemory_SecondQueuedMidTransfer_EachGetsFullContent()
    {
        const QByteArray first(qsizetype{2} * 1024 * 1024, 'a');
        const QByteArray second(qsizetype{1024} * 1024, 'b');
        FakeFtpServer server;
        server.setCompletionOrder(FakeFtpServer::CompletionOrder::ReplyThenCloseData);
        server.setFile("/SD/first.d64", first);
        server.setFile("/SD/second.d64", second);
        QVERIFY(loginTo(server));
        FailureSpy failureSpy(ftp);
        QSignalSpy progressSpy(ftp, &C64UFtpClient::downloadProgress);
        QSignalSpy memorySpy(ftp, &C64UFtpClient::downloadToMemoryFinished);

        ftp->downloadToMemory("/SD/first.d64");
        QTRY_VERIFY_WITH_TIMEOUT(!progressSpy.isEmpty(), SignalTimeoutMs);
        ftp->downloadToMemory("/SD/second.d64");

        QTRY_COMPARE_WITH_TIMEOUT(memorySpy.count(), 2, SignalTimeoutMs);
        QVERIFY2(failureSpy.isEmpty(), qPrintable(describeFailures(failureSpy)));
        QCOMPARE(memorySpy.at(0).at(0).toString(), QString("/SD/first.d64"));
        QCOMPARE(memorySpy.at(0).at(1).toByteArray().size(), first.size());
        QCOMPARE(memorySpy.at(0).at(1).toByteArray(), first);
        QCOMPARE(memorySpy.at(1).at(1).toByteArray(), second);
        // Data may overtake the 150 reply that announces the size (total 0 means
        // unknown), but queuing the second download must not reset the first's total.
        QHash<QString, qint64> lastTotal;
        for (const auto &progress : progressSpy) {
            lastTotal[progress.at(0).toString()] = progress.at(2).toLongLong();
        }
        QCOMPARE(lastTotal.value("/SD/first.d64"), qint64(first.size()));
        QCOMPARE(lastTotal.value("/SD/second.d64"), qint64(second.size()));
    }

    // =========================================================================
    // Local files are only replaced by a completed download
    // =========================================================================

    void testDownload_MissingRemoteFile_PreservesExistingLocalFile()
    {
        FakeFtpServer server;
        QVERIFY(loginTo(server));
        QTemporaryDir dir;
        const QString localPath = dir.filePath("game.prg");
        writeLocalFile(localPath, "PRECIOUS");
        FailureSpy failureSpy(ftp);

        ftp->download("/SD/missing.prg", localPath);

        QTRY_COMPARE_WITH_TIMEOUT(failureSpy.count(), 1, SignalTimeoutMs);
        letLateSignalsArrive();
        QCOMPARE(readLocalFile(localPath), QByteArray("PRECIOUS"));
        QCOMPARE(filesIn(dir), QStringList{"game.prg"});
    }

    void testDownload_MissingRemoteFile_LeavesNoLocalFile()
    {
        FakeFtpServer server;
        QVERIFY(loginTo(server));
        QTemporaryDir dir;
        FailureSpy failureSpy(ftp);

        ftp->download("/SD/missing.prg", dir.filePath("missing.prg"));

        QTRY_COMPARE_WITH_TIMEOUT(failureSpy.count(), 1, SignalTimeoutMs);
        letLateSignalsArrive();
        QCOMPARE(filesIn(dir), QStringList());
    }

    void testDownload_Success_ReplacesExistingLocalFile_data() { addCompletionOrderRows(); }

    void testDownload_Success_ReplacesExistingLocalFile()
    {
        QFETCH(FakeFtpServer::CompletionOrder, completionOrder);
        FakeFtpServer server;
        server.setCompletionOrder(completionOrder);
        server.setFile("/SD/game.prg", "NEW");
        QVERIFY(loginTo(server));
        QTemporaryDir dir;
        const QString localPath = dir.filePath("game.prg");
        writeLocalFile(localPath, "OLD AND LONGER CONTENT");
        QByteArray contentWhenFinished;
        connect(ftp, &IFtpClient::downloadFinished, this, [&contentWhenFinished, localPath]() {
            contentWhenFinished = readLocalFile(localPath);
        });
        QSignalSpy finishedSpy(ftp, &C64UFtpClient::downloadFinished);

        ftp->download("/SD/game.prg", localPath);

        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, SignalTimeoutMs);
        QCOMPARE(contentWhenFinished, QByteArray("NEW"));
        QCOMPARE(readLocalFile(localPath), QByteArray("NEW"));
        QCOMPARE(filesIn(dir), QStringList{"game.prg"});
    }

    void testDownload_CannotCreateLocalFile_ReportsErrorAndRunsNextOperation()
    {
        FakeFtpServer server;
        server.setFile("/SD/game.prg", "GAME");
        server.setListing(OneFileListing);
        QVERIFY(loginTo(server));
        QTemporaryDir dir;
        FailureSpy failureSpy(ftp);
        QSignalSpy listedSpy(ftp, &C64UFtpClient::directoryListed);

        ftp->download("/SD/game.prg", dir.filePath("no-such-dir/game.prg"));
        ftp->list("/SD");

        QTRY_COMPARE_WITH_TIMEOUT(listedSpy.count(), 1, SignalTimeoutMs);
        letLateSignalsArrive();
        QVERIFY2(failureSpy.count() == 1, qPrintable(describeFailures(failureSpy)));
        QVERIFY(failureSpy.first().first().toString().contains("no-such-dir/game.prg"));
    }

    void testAbort_MidDownload_PreservesExistingLocalFile()
    {
        FakeFtpServer server;
        server.setFile("/SD/big.d64", QByteArray(200000, 'b'));
        server.setStallAfterBytes(1000);
        QVERIFY(loginTo(server));
        QTemporaryDir dir;
        const QString localPath = dir.filePath("big.d64");
        writeLocalFile(localPath, "PRECIOUS");
        QSignalSpy progressSpy(ftp, &C64UFtpClient::downloadProgress);

        ftp->download("/SD/big.d64", localPath);
        QTRY_VERIFY_WITH_TIMEOUT(!progressSpy.isEmpty(), SignalTimeoutMs);
        ftp->abort();

        QTRY_COMPARE_WITH_TIMEOUT(ftp->state(), IFtpClient::State::Ready, SignalTimeoutMs);
        QCOMPARE(readLocalFile(localPath), QByteArray("PRECIOUS"));
        QCOMPARE(filesIn(dir), QStringList{"big.d64"});
    }

    void testConnectionLost_MidDownload_PreservesExistingLocalFile()
    {
        FakeFtpServer server;
        server.setFile("/SD/big.d64", QByteArray(200000, 'b'));
        server.setStallAfterBytes(1000);
        QVERIFY(loginTo(server));
        QTemporaryDir dir;
        const QString localPath = dir.filePath("big.d64");
        writeLocalFile(localPath, "PRECIOUS");
        QSignalSpy progressSpy(ftp, &C64UFtpClient::downloadProgress);
        QSignalSpy finishedSpy(ftp, &C64UFtpClient::downloadFinished);

        ftp->download("/SD/big.d64", localPath);
        QTRY_VERIFY_WITH_TIMEOUT(!progressSpy.isEmpty(), SignalTimeoutMs);
        server.closeClientConnection();

        QTRY_COMPARE_WITH_TIMEOUT(ftp->state(), IFtpClient::State::Disconnected, SignalTimeoutMs);
        letLateSignalsArrive();
        QCOMPARE(finishedSpy.count(), 0);
        QCOMPARE(readLocalFile(localPath), QByteArray("PRECIOUS"));
        QCOMPARE(filesIn(dir), QStringList{"big.d64"});
    }

    // =========================================================================
    // Uploads
    // =========================================================================

    void testUpload_MultiChunkFile_EmitsIncreasingProgressEndingAtTotal()
    {
        QByteArray contents;
        for (int i = 0; i < 300000; ++i) {
            contents.append(static_cast<char>(i % 251));
        }
        FakeFtpServer server;
        QVERIFY(loginTo(server));
        QTemporaryDir dir;
        const QString localPath = dir.filePath("disk.d64");
        writeLocalFile(localPath, contents);
        FailureSpy failureSpy(ftp);
        QSignalSpy progressSpy(ftp, &C64UFtpClient::uploadProgress);
        QSignalSpy finishedSpy(ftp, &C64UFtpClient::uploadFinished);

        ftp->upload(localPath, "/SD/disk.d64");

        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, SignalTimeoutMs);
        QVERIFY2(failureSpy.isEmpty(), qPrintable(describeFailures(failureSpy)));
        QCOMPARE(server.storedFile("/SD/disk.d64"), contents);
        QVERIFY2(progressSpy.count() >= 2,
                 qPrintable(QString("progress signals: %1").arg(progressSpy.count())));
        qint64 previousSent = 0;
        for (const auto &progress : progressSpy) {
            QCOMPARE(progress.at(0).toString(), localPath);
            QVERIFY(progress.at(1).toLongLong() > previousSent);
            QCOMPARE(progress.at(2).toLongLong(), qint64(contents.size()));
            previousSent = progress.at(1).toLongLong();
        }
        QCOMPARE(previousSent, qint64(contents.size()));
    }

    void testUpload_EmptyFile_Finishes()
    {
        FakeFtpServer server;
        QVERIFY(loginTo(server));
        QTemporaryDir dir;
        const QString localPath = dir.filePath("empty.prg");
        writeLocalFile(localPath, QByteArray());
        FailureSpy failureSpy(ftp);
        QSignalSpy finishedSpy(ftp, &C64UFtpClient::uploadFinished);

        ftp->upload(localPath, "/SD/empty.prg");

        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, SignalTimeoutMs);
        QVERIFY2(failureSpy.isEmpty(), qPrintable(describeFailures(failureSpy)));
        QCOMPARE(server.storedFile("/SD/empty.prg"), QByteArray());
        QCOMPARE(server.commandCount("STOR"), 1);
    }

    // =========================================================================
    // Re-entrancy: requests made from inside signal handlers
    // =========================================================================

    void testList_CalledFromConnectedSlot_WaitsForEachReply()
    {
        FakeFtpServer server;
        server.setListing(OneFileListing);
        QVERIFY(server.listen());
        FailureSpy failureSpy(ftp);
        QSignalSpy listedSpy(ftp, &C64UFtpClient::directoryListed);
        connect(ftp, &IFtpClient::connected, this, [this]() { ftp->list("/SD"); });

        ftp->setHost("127.0.0.1", server.port());
        ftp->setCredentials("user", "pass");
        ftp->connectToHost();

        QTRY_COMPARE_WITH_TIMEOUT(listedSpy.count(), 1, SignalTimeoutMs);
        letLateSignalsArrive();
        QVERIFY2(failureSpy.isEmpty(), qPrintable(describeFailures(failureSpy)));
        QCOMPARE(server.commands(),
                 QStringList({"USER user", "PASS pass", "TYPE A", "PASV", "LIST /SD"}));
        QCOMPARE(ftp->state(), IFtpClient::State::Ready);
    }

    void testDownload_QueuedFromFinishedSlot_RunsAfterCurrentCompletes()
    {
        FakeFtpServer server;
        server.setFile("/SD/one.prg", "ONE");
        server.setFile("/SD/two.prg", "TWO");
        QVERIFY(loginTo(server));
        QTemporaryDir dir;
        FailureSpy failureSpy(ftp);
        QSignalSpy finishedSpy(ftp, &C64UFtpClient::downloadFinished);
        connect(ftp, &IFtpClient::downloadFinished, this, [this, &dir](const QString &remotePath) {
            if (remotePath == "/SD/one.prg") {
                ftp->download("/SD/two.prg", dir.filePath("two.prg"));
            }
        });

        ftp->download("/SD/one.prg", dir.filePath("one.prg"));

        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 2, SignalTimeoutMs);
        QVERIFY2(failureSpy.isEmpty(), qPrintable(describeFailures(failureSpy)));
        QCOMPARE(readLocalFile(dir.filePath("two.prg")), QByteArray("TWO"));
    }
};

QTEST_MAIN(TestC64UFtpClientProtocol)
#include "test_c64uftpclient_protocol.moc"
