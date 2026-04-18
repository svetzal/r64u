#include "../src/services/ftpresponsehandler.h"
#include "../src/services/ftptransferstate.h"

#include <QtTest>

// ============================================================
// Helper to build a minimal context
// ============================================================

static FtpResponseContext
makeCtx(FtpCommandQueue::Command cmd = FtpCommandQueue::Command::None, const QString &arg = {},
        const QString &localPath = {}, IFtpClient::State state = IFtpClient::State::Busy,
        bool loggedIn = true, const QString &dir = "/",
        QAbstractSocket::SocketState socketState = QAbstractSocket::UnconnectedState)
{
    return {cmd, arg, localPath, state, loggedIn, dir, socketState};
}

// ============================================================
// Tests
// ============================================================

class TestFtpResponseHandler : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        transferState = new FtpTransferState();
        handler = new FtpResponseHandler(*transferState, nullptr);
    }

    void cleanup()
    {
        delete handler;
        delete transferState;
    }

    // --- handleResponse: Connected state ---

    void testConnected_220_queuesUserAndProcessesNext()
    {
        auto ctx = makeCtx(FtpCommandQueue::Command::None, {}, {}, IFtpClient::State::Connected);
        auto action = handler->handleResponse(220, "Service ready", ctx);

        QCOMPARE(action.enqueueCommand, FtpCommandQueue::Command::User);
        QCOMPARE(action.kind, FtpResponseAction::Kind::ProcessNext);
    }

    // --- handleBusyResponse: USER ---

    void testBusy_User_331_queuesPass()
    {
        auto ctx = makeCtx(FtpCommandQueue::Command::User);
        auto action = handler->handleBusyResponse(331, "Password required", ctx);

        QCOMPARE(action.enqueueCommand, FtpCommandQueue::Command::Pass);
        QCOMPARE(action.kind, FtpResponseAction::Kind::ProcessNext);
    }

    void testBusy_User_230_logsIn()
    {
        auto ctx = makeCtx(FtpCommandQueue::Command::User);
        auto action = handler->handleBusyResponse(230, "Logged in", ctx);

        QVERIFY(action.transitionToReady);
        QVERIFY(action.transitionToLoggedIn);
        QVERIFY(action.emitConnected);
    }

    void testBusy_User_error_disconnects()
    {
        auto ctx = makeCtx(FtpCommandQueue::Command::User);
        auto action = handler->handleBusyResponse(530, "Login incorrect", ctx);

        QCOMPARE(action.kind, FtpResponseAction::Kind::Disconnect);
        QVERIFY(!action.errorMessage.isEmpty());
    }

    // --- handleBusyResponse: PASS ---

    void testBusy_Pass_230_logsIn()
    {
        auto ctx = makeCtx(FtpCommandQueue::Command::Pass);
        auto action = handler->handleBusyResponse(230, "User logged in", ctx);

        QVERIFY(action.transitionToReady);
        QVERIFY(action.transitionToLoggedIn);
        QVERIFY(action.emitConnected);
    }

    void testBusy_Pass_error_disconnects()
    {
        auto ctx = makeCtx(FtpCommandQueue::Command::Pass);
        auto action = handler->handleBusyResponse(530, "Invalid password", ctx);

        QCOMPARE(action.kind, FtpResponseAction::Kind::Disconnect);
        QVERIFY(!action.errorMessage.isEmpty());
    }

    // --- handleBusyResponse: CWD ---

    void testBusy_Cwd_250_emitsDirectoryChanged()
    {
        auto ctx = makeCtx(FtpCommandQueue::Command::Cwd, "/SD");
        auto action = handler->handleBusyResponse(250, "OK", ctx);

        QCOMPARE(action.directoryChangedPath, QString("/SD"));
    }

    void testBusy_Cwd_error_setsErrorMessage()
    {
        auto ctx = makeCtx(FtpCommandQueue::Command::Cwd, "/noexist");
        auto action = handler->handleBusyResponse(550, "No such file", ctx);

        QVERIFY(!action.errorMessage.isEmpty());
    }

    // --- handleBusyResponse: MKD ---

    void testBusy_Mkd_257_emitsDirectoryCreated()
    {
        auto ctx = makeCtx(FtpCommandQueue::Command::Mkd, "/SD/newdir");
        auto action = handler->handleBusyResponse(257, "Created", ctx);

        QCOMPARE(action.directoryCreatedPath, QString("/SD/newdir"));
    }

    void testBusy_Mkd_553_alreadyExists_emitsDirectoryCreated()
    {
        auto ctx = makeCtx(FtpCommandQueue::Command::Mkd, "/SD/existing");
        auto action = handler->handleBusyResponse(553, "File exists", ctx);

        QCOMPARE(action.directoryCreatedPath, QString("/SD/existing"));
    }

    // --- handleBusyResponse: DELE/RMD ---

    void testBusy_Dele_250_emitsFileRemoved()
    {
        auto ctx = makeCtx(FtpCommandQueue::Command::Dele, "/SD/file.prg");
        auto action = handler->handleBusyResponse(250, "Deleted", ctx);

        QCOMPARE(action.fileRemovedPath, QString("/SD/file.prg"));
    }

    // --- handleBusyResponse: RNFR/RNTO ---

    void testBusy_RnFr_350_noError()
    {
        auto ctx = makeCtx(FtpCommandQueue::Command::RnFr, "/SD/old.prg");
        auto action = handler->handleBusyResponse(350, "Pending", ctx);

        QVERIFY(action.errorMessage.isEmpty());
    }

    void testBusy_RnTo_250_emitsFileRenamed()
    {
        auto ctx = makeCtx(FtpCommandQueue::Command::RnTo, "/SD/new.prg", "/SD/old.prg");
        auto action = handler->handleBusyResponse(250, "OK", ctx);

        QCOMPARE(action.fileRenamedOldPath, QString("/SD/old.prg"));
        QCOMPARE(action.fileRenamedNewPath, QString("/SD/new.prg"));
    }

    // --- handleBusyResponse: RETR (226 with socket already closed) ---

    void testBusy_Retr_226_socketClosed_memoryDownload_emitsDownloadToMemory()
    {
        transferState->clearRetrBuffer();
        transferState->appendRetrData(QByteArray("hello"));
        transferState->setCurrentRetrFile(nullptr, true);  // isMemory = true

        auto ctx = makeCtx(FtpCommandQueue::Command::Retr, "/SD/tune.sid", {},
                           IFtpClient::State::Busy, true, "/", QAbstractSocket::UnconnectedState);
        auto action = handler->handleBusyResponse(226, "Transfer complete", ctx);

        QCOMPARE(action.downloadToMemoryPath, QString("/SD/tune.sid"));
        QCOMPARE(action.downloadToMemoryData, QByteArray("hello"));
        QVERIFY(action.clearCurrentRetrFile);
    }

    // --- handleDataDisconnected ---

    void testDataDisconnected_withPendingList_emitsDirectoryListed()
    {
        // Simulate 226 arriving before data socket closed: save pending list
        transferState->savePendingList("/SD", QByteArray());  // empty listing
        QVERIFY(transferState->hasPendingList());

        auto ctx = makeCtx();
        auto action = handler->handleDataDisconnected(ctx);

        QCOMPARE(action.kind, FtpResponseAction::Kind::ProcessNext);
        QCOMPARE(action.directoryListedPath, QString("/SD"));
    }

private:
    FtpTransferState *transferState = nullptr;
    FtpResponseHandler *handler = nullptr;
};

QTEST_MAIN(TestFtpResponseHandler)
#include "test_ftpresponsehandler.moc"
