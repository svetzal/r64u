#include "mocks/mockftpclient.h"
#include "mocks/mockrestclient.h"
#include "services/deviceconnection.h"
#include "services/filetypecore.h"
#include "services/playlistmanager.h"
#include "ui/fileactioncontroller.h"

#include <QAction>
#include <QSignalSpy>
#include <QtTest>

class TestFileActionController : public QObject
{
    Q_OBJECT

private:
    // Helpers that build a fully-wired controller with a live (disconnected) DeviceConnection.
    // The connection has a real MockRestClient so restClient() != nullptr.
    DeviceConnection *makeConnection()
    {
        auto *restClient = new MockRestClient(this);
        auto *ftpClient = new MockFtpClient(this);
        return new DeviceConnection(restClient, ftpClient, this);
    }

private slots:

    // ==========================================================================
    // play() — dispatch
    // ==========================================================================

    void testPlay_SidMusic_EmitsStatusMessageContainingPlayingSid()
    {
        auto *connection = makeConnection();
        auto *controller = new FileActionController(connection, nullptr, this);
        QSignalSpy spy(controller, &FileActionController::statusMessage);

        controller->play("/song.sid", filetype::FileType::SidMusic);

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains("Playing SID"));
    }

    void testPlay_ModMusic_EmitsStatusMessageContainingPlayingMod()
    {
        auto *connection = makeConnection();
        auto *controller = new FileActionController(connection, nullptr, this);
        QSignalSpy spy(controller, &FileActionController::statusMessage);

        controller->play("/tune.mod", filetype::FileType::ModMusic);

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains("Playing MOD"));
    }

    void testPlay_EmptyPath_EmitsNoStatusMessage()
    {
        auto *connection = makeConnection();
        auto *controller = new FileActionController(connection, nullptr, this);
        QSignalSpy spy(controller, &FileActionController::statusMessage);

        controller->play(QString(), filetype::FileType::SidMusic);

        QCOMPARE(spy.count(), 0);
    }

    void testPlay_NullDeviceConnection_EmitsNoStatusMessage()
    {
        auto *controller = new FileActionController(nullptr, nullptr, this);
        QSignalSpy spy(controller, &FileActionController::statusMessage);

        controller->play("/song.sid", filetype::FileType::SidMusic);

        QCOMPARE(spy.count(), 0);
    }

    // ==========================================================================
    // run() — dispatch
    // ==========================================================================

    void testRun_Program_EmitsStatusMessageContainingRunningPrg()
    {
        auto *connection = makeConnection();
        auto *controller = new FileActionController(connection, nullptr, this);
        QSignalSpy spy(controller, &FileActionController::statusMessage);

        controller->run("/game.prg", filetype::FileType::Program);

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains("Running PRG"));
    }

    void testRun_Cartridge_EmitsStatusMessageContainingRunningCrt()
    {
        auto *connection = makeConnection();
        auto *controller = new FileActionController(connection, nullptr, this);
        QSignalSpy spy(controller, &FileActionController::statusMessage);

        controller->run("/game.crt", filetype::FileType::Cartridge);

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains("Running CRT"));
    }

    void testRun_EmptyPath_EmitsNoStatusMessage()
    {
        auto *connection = makeConnection();
        auto *controller = new FileActionController(connection, nullptr, this);
        QSignalSpy spy(controller, &FileActionController::statusMessage);

        controller->run(QString(), filetype::FileType::Program);

        QCOMPARE(spy.count(), 0);
    }

    void testRun_DiskImage_DoesNotEmitRunningPrgOrCrtStatusMessage()
    {
        // DiskImage delegates to DiskBootSequenceService.  The run() method
        // itself does NOT produce "Running PRG" or "Running CRT" style messages;
        // any status messages that do arrive come from the boot service.
        auto *connection = makeConnection();
        auto *controller = new FileActionController(connection, nullptr, this);
        QStringList capturedMessages;
        connect(controller, &FileActionController::statusMessage, this,
                [&capturedMessages](const QString &msg, int) { capturedMessages << msg; });

        controller->run("/disk.d64", filetype::FileType::DiskImage);

        for (const QString &msg : capturedMessages) {
            QVERIFY(!msg.contains("Running PRG"));
            QVERIFY(!msg.contains("Running CRT"));
        }
    }

    // ==========================================================================
    // loadConfig() — validation
    // ==========================================================================

    void testLoadConfig_EmptyPath_EmitsNoStatusMessage()
    {
        auto *controller = new FileActionController(nullptr, nullptr, this);
        QSignalSpy spy(controller, &FileActionController::statusMessage);

        controller->loadConfig(QString(), filetype::FileType::Config);

        QCOMPARE(spy.count(), 0);
    }

    void testLoadConfig_WrongFileType_EmitsNotAConfigurationFileMessage()
    {
        auto *controller = new FileActionController(nullptr, nullptr, this);
        QSignalSpy spy(controller, &FileActionController::statusMessage);

        controller->loadConfig("/song.sid", filetype::FileType::SidMusic);

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains("not a configuration file"));
    }

    void testLoadConfig_NoDeviceConnection_EmitsNotConnectedMessage()
    {
        // Correct type, but no device connection (nullptr) → "Not connected"
        auto *controller = new FileActionController(nullptr, nullptr, this);
        QSignalSpy spy(controller, &FileActionController::statusMessage);

        controller->loadConfig("/settings.cfg", filetype::FileType::Config);

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains("Not connected"));
    }

    // ==========================================================================
    // addToPlaylist()
    // ==========================================================================

    void testAddToPlaylist_MixedItems_OnlySidPassesFilter()
    {
        auto *playlistManager = new PlaylistManager(nullptr, this);
        auto *controller = new FileActionController(nullptr, nullptr, this);
        controller->setPlaylistManager(playlistManager);

        QSignalSpy spy(controller, &FileActionController::statusMessage);

        QList<QPair<QString, filetype::FileType>> items = {
            {"/song.sid", filetype::FileType::SidMusic},
            {"/game.prg", filetype::FileType::Program}};

        controller->addToPlaylist(items);

        QCOMPARE(spy.count(), 1);
        // "1 item(s)" — only the SID passed through
        QVERIFY(spy.at(0).at(0).toString().contains("1"));
        QCOMPARE(playlistManager->count(), 1);
    }

    void testAddToPlaylist_NoSidItems_EmitsNoSidMusicFilesMessage()
    {
        auto *playlistManager = new PlaylistManager(nullptr, this);
        auto *controller = new FileActionController(nullptr, nullptr, this);
        controller->setPlaylistManager(playlistManager);

        QSignalSpy spy(controller, &FileActionController::statusMessage);

        QList<QPair<QString, filetype::FileType>> items = {
            {"/game.prg", filetype::FileType::Program},
            {"/game.crt", filetype::FileType::Cartridge}};

        controller->addToPlaylist(items);

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains("No SID music files"));
    }

    void testAddToPlaylist_NullPlaylistManager_IsNoOp()
    {
        // No playlist manager set — must not crash and must not emit statusMessage
        auto *controller = new FileActionController(nullptr, nullptr, this);
        QSignalSpy spy(controller, &FileActionController::statusMessage);

        QList<QPair<QString, filetype::FileType>> items = {
            {"/song.sid", filetype::FileType::SidMusic}};

        controller->addToPlaylist(items);

        QCOMPARE(spy.count(), 0);
    }

    // ==========================================================================
    // download()
    // ==========================================================================

    void testDownload_AlwaysEmitsStatusMessage()
    {
        auto *controller = new FileActionController(nullptr, nullptr, this);
        QSignalSpy spy(controller, &FileActionController::statusMessage);

        controller->download("/some/file.txt");

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains("Download requested"));
    }

    // ==========================================================================
    // updateActionStates()
    // ==========================================================================

    void testUpdateActionStates_NullActions_DoesNotCrash()
    {
        auto *controller = new FileActionController(nullptr, nullptr, this);
        // No setActions() call — all action pointers remain nullptr
        // Must not crash when called
        controller->updateActionStates(filetype::FileType::SidMusic, true);
    }

    void testUpdateActionStates_SidMusicWithCanOperate_EnablesPlayDisablesRunAndMount()
    {
        auto *controller = new FileActionController(nullptr, nullptr, this);

        auto *playAction = new QAction(this);
        auto *runAction = new QAction(this);
        auto *mountAction = new QAction(this);
        controller->setActions(playAction, runAction, mountAction);

        controller->updateActionStates(filetype::FileType::SidMusic, true);

        QVERIFY(playAction->isEnabled());
        QVERIFY(!runAction->isEnabled());
        QVERIFY(!mountAction->isEnabled());
    }
};

QTEST_MAIN(TestFileActionController)
#include "test_fileactioncontroller.moc"
