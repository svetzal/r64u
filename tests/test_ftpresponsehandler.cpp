#include "../src/ftp/ftptransferstate.h"
#include "../src/services/ftpresponsehandler.h"

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

    // --- handleBusyResponse: PWD ---

    void testBusy_Pwd_257_updatesCurrentDir()
    {
        auto ctx = makeCtx(FtpCommandQueue::Command::Pwd);
        auto action = handler->handleBusyResponse(257, "\"/SD\" is current directory", ctx);

        QCOMPARE(action.updatedCurrentDir, QString("/SD"));
        QCOMPARE(action.kind, FtpResponseAction::Kind::ProcessNext);
    }

    void testBusy_Pwd_error_doesNotUpdateDir()
    {
        auto ctx = makeCtx(FtpCommandQueue::Command::Pwd);
        auto action = handler->handleBusyResponse(550, "Error", ctx);

        QVERIFY(action.updatedCurrentDir.isEmpty());
        QCOMPARE(action.kind, FtpResponseAction::Kind::ProcessNext);
    }

    // --- handleBusyResponse: TYPE ---

    void testBusy_Type_200_justProceedsNext()
    {
        auto ctx = makeCtx(FtpCommandQueue::Command::Type, "I");
        auto action = handler->handleBusyResponse(200, "Type set to I", ctx);

        QCOMPARE(action.kind, FtpResponseAction::Kind::ProcessNext);
        QVERIFY(action.errorMessage.isEmpty());
    }

    // --- handleBusyResponse: PASV ---

    void testBusy_Pasv_227_returnsProcessNextAndConnect()
    {
        auto ctx = makeCtx(FtpCommandQueue::Command::Pasv);
        auto action =
            handler->handleBusyResponse(227, "Entering Passive Mode (192,168,1,64,4,0)", ctx);

        QCOMPARE(action.kind, FtpResponseAction::Kind::ProcessNextAndConnect);
        QCOMPARE(action.dataPort, quint16(4 * 256 + 0));
    }

    void testBusy_Pasv_500_setsError()
    {
        auto ctx = makeCtx(FtpCommandQueue::Command::Pasv);
        auto action = handler->handleBusyResponse(500, "Not supported", ctx);

        QVERIFY(!action.errorMessage.isEmpty());
    }

    // --- handleBusyResponse: LIST ---

    void testBusy_List_150_startsDownloading()
    {
        auto ctx = makeCtx(FtpCommandQueue::Command::List, "/SD");
        auto action = handler->handleBusyResponse(150, "Opening data connection", ctx);

        QVERIFY(action.setDownloading);
        QCOMPARE(action.kind, FtpResponseAction::Kind::None);
    }

    void testBusy_List_226_socketClosed_emitsDirectoryListed()
    {
        auto ctx = makeCtx(FtpCommandQueue::Command::List, "/SD", {}, IFtpClient::State::Busy, true,
                           "/", QAbstractSocket::UnconnectedState);
        auto action = handler->handleBusyResponse(226, "Transfer complete", ctx);

        QCOMPARE(action.directoryListedPath, QString("/SD"));
        QVERIFY(action.clearListBuffer);
    }

    void testBusy_List_error_setsErrorMessage()
    {
        auto ctx = makeCtx(FtpCommandQueue::Command::List, "/SD");
        auto action = handler->handleBusyResponse(550, "Permission denied", ctx);

        QVERIFY(!action.errorMessage.isEmpty());
    }

    // --- handleBusyResponse: RETR (byte count) ---

    void testBusy_Retr_150_withByteCount_setsTransferSize()
    {
        transferState->clearRetrBuffer();
        transferState->setCurrentRetrFile(nullptr, true);

        auto ctx = makeCtx(FtpCommandQueue::Command::Retr, "/SD/tune.sid");
        auto action = handler->handleBusyResponse(150, "Opening BINARY (1024 bytes)", ctx);

        QVERIFY(action.setDownloading);
        QCOMPARE(action.kind, FtpResponseAction::Kind::None);
        QCOMPARE(transferState->transferSize(), qint64(1024));
    }

    void testBusy_Retr_150_withoutByteCount_noTransferSize()
    {
        transferState->setTransferSize(0);
        transferState->setCurrentRetrFile(nullptr, true);

        auto ctx = makeCtx(FtpCommandQueue::Command::Retr, "/SD/tune.sid");
        auto action = handler->handleBusyResponse(150, "Opening BINARY connection", ctx);

        QVERIFY(action.setDownloading);
        QCOMPARE(transferState->transferSize(), qint64(0));
    }

    void testBusy_Retr_error_setsErrorMessage()
    {
        auto ctx = makeCtx(FtpCommandQueue::Command::Retr, "/SD/missing.sid");
        auto action = handler->handleBusyResponse(550, "File not found", ctx);

        QVERIFY(!action.errorMessage.isEmpty());
        QVERIFY(action.clearCurrentRetrFile);
    }

    // --- handleBusyResponse: STOR ---

    void testBusy_Stor_150_returnsNone()
    {
        auto ctx = makeCtx(FtpCommandQueue::Command::Stor, "/SD/file.prg", "/local/file.prg");
        auto action = handler->handleBusyResponse(150, "Opening data connection", ctx);

        QCOMPARE(action.kind, FtpResponseAction::Kind::None);
    }

    void testBusy_Stor_226_emitsUploadFinished()
    {
        auto ctx = makeCtx(FtpCommandQueue::Command::Stor, "/SD/file.prg", "/local/file.prg");
        auto action = handler->handleBusyResponse(226, "Transfer complete", ctx);

        QCOMPARE(action.uploadFinishedLocalPath, QString("/local/file.prg"));
        QCOMPARE(action.uploadFinishedRemotePath, QString("/SD/file.prg"));
        QVERIFY(action.clearCurrentStorFile);
    }

    void testBusy_Stor_error_setsErrorMessage()
    {
        auto ctx = makeCtx(FtpCommandQueue::Command::Stor, "/SD/file.prg", "/local/file.prg");
        auto action = handler->handleBusyResponse(553, "File exists", ctx);

        QVERIFY(!action.errorMessage.isEmpty());
        QVERIFY(action.clearCurrentStorFile);
    }

    // --- handleBusyResponse: RMD ---

    void testBusy_Rmd_250_emitsFileRemoved()
    {
        auto ctx = makeCtx(FtpCommandQueue::Command::Rmd, "/SD/olddir");
        auto action = handler->handleBusyResponse(250, "OK", ctx);

        QCOMPARE(action.fileRemovedPath, QString("/SD/olddir"));
    }

    void testBusy_Rmd_error_setsErrorMessage()
    {
        auto ctx = makeCtx(FtpCommandQueue::Command::Rmd, "/SD/noexist");
        auto action = handler->handleBusyResponse(550, "Not found", ctx);

        QVERIFY(!action.errorMessage.isEmpty());
    }

private:
    FtpTransferState *transferState = nullptr;
    FtpResponseHandler *handler = nullptr;
};

QTEST_MAIN(TestFtpResponseHandler)
#include "test_ftpresponsehandler.moc"
