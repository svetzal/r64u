#include "services/playlistservice.h"

#include <QSignalSpy>
#include <QTemporaryFile>
#include <QtTest>

class TestPlaylistService : public QObject
{
    Q_OBJECT

private:
    PlaylistService *manager = nullptr;

    static void addTestItem(PlaylistService *m, const QString &path)
    {
        playlist::PlaylistItem item;
        item.path = path;
        item.title = "Test";
        item.subsong = 1;
        item.durationSecs = 30;
        m->addItem(item);
    }

private slots:
    void init()
    {
        QCoreApplication::setOrganizationName("r64utest");
        QCoreApplication::setApplicationName("test_playlistmanager");

        // Clear any leftover playlist settings from a previous run
        QSettings settings;
        settings.remove("playlist");

        manager = new PlaylistService(nullptr);
    }

    void cleanup()
    {
        delete manager;
        manager = nullptr;

        QSettings settings;
        settings.remove("playlist");
    }

    // =========================================================
    // List management - signals
    // =========================================================

    void testAddItemByPath_EmitsPlaylistChanged()
    {
        QSignalSpy spy(manager, &PlaylistService::playlistChanged);
        manager->addItem("/SD/test.sid");
        QCOMPARE(spy.count(), 1);
    }

    void testAddItemByPath_IncreasesCount()
    {
        manager->addItem("/SD/test.sid");
        QCOMPARE(manager->count(), 1);
    }

    void testAddItemByItem_EmitsPlaylistChanged()
    {
        QSignalSpy spy(manager, &PlaylistService::playlistChanged);
        addTestItem(manager, "/SD/music.sid");
        QCOMPARE(spy.count(), 1);
    }

    void testAddItemByItem_IncreasesCount()
    {
        addTestItem(manager, "/SD/music.sid");
        QCOMPARE(manager->count(), 1);
    }

    void testRemoveItem_EmitsPlaylistChanged()
    {
        addTestItem(manager, "/SD/a.sid");
        QSignalSpy spy(manager, &PlaylistService::playlistChanged);
        manager->removeItem(0);
        QCOMPARE(spy.count(), 1);
    }

    void testRemoveItem_DecreasesCount()
    {
        addTestItem(manager, "/SD/a.sid");
        manager->removeItem(0);
        QCOMPARE(manager->count(), 0);
    }

    void testMoveItem_EmitsPlaylistChanged()
    {
        addTestItem(manager, "/SD/a.sid");
        addTestItem(manager, "/SD/b.sid");
        QSignalSpy spy(manager, &PlaylistService::playlistChanged);
        manager->moveItem(0, 1);
        QCOMPARE(spy.count(), 1);
    }

    void testClear_EmitsPlaylistChanged()
    {
        addTestItem(manager, "/SD/a.sid");
        QSignalSpy spy(manager, &PlaylistService::playlistChanged);
        manager->clear();
        QCOMPARE(spy.count(), 1);
    }

    void testClear_SetsIsEmpty()
    {
        addTestItem(manager, "/SD/a.sid");
        manager->clear();
        QVERIFY(manager->isEmpty());
    }

    void testClear_WhenAlreadyEmpty_DoesNotEmitPlaylistChanged()
    {
        // Playlist starts empty; clear() should be a no-op and not emit playlistChanged
        QVERIFY(manager->isEmpty());
        QSignalSpy spy(manager, &PlaylistService::playlistChanged);
        manager->clear();
        QCOMPARE(spy.count(), 0);
        QVERIFY(manager->isEmpty());
    }

    void testItemAt_ValidIndex_ReturnsCorrectItem()
    {
        addTestItem(manager, "/SD/a.sid");
        QCOMPARE(manager->itemAt(0).path, QString("/SD/a.sid"));
    }

    void testItemAt_InvalidIndex_ReturnsDefaultItem()
    {
        PlaylistService::PlaylistItem item = manager->itemAt(99);
        QVERIFY(item.path.isEmpty());
    }

    // =========================================================
    // Playback state - signals
    // =========================================================

    void testPlay_WithOneItem_EmitsPlaybackStarted()
    {
        addTestItem(manager, "/SD/a.sid");
        QSignalSpy spy(manager, &PlaylistService::playbackStarted);
        manager->play(0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toInt(), 0);
    }

    void testPlay_WithOneItem_EmitsCurrentIndexChanged()
    {
        addTestItem(manager, "/SD/a.sid");
        QSignalSpy spy(manager, &PlaylistService::currentIndexChanged);
        manager->play(0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toInt(), 0);
    }

    void testPlay_EmptyPlaylist_DoesNotEmitPlaybackStarted()
    {
        QSignalSpy spy(manager, &PlaylistService::playbackStarted);
        manager->play();
        QCOMPARE(spy.count(), 0);
    }

    void testPlay_EmptyPlaylist_EmitsPlaylistIsEmptyStatusMessage()
    {
        QSignalSpy spy(manager, &PlaylistService::statusMessage);
        manager->play();
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains("Playlist is empty"));
    }

    void testPlay_OutOfBoundsIndex_EmitsInvalidTrackIndexStatusMessage()
    {
        addTestItem(manager, "/SD/a.sid");
        QSignalSpy spy(manager, &PlaylistService::statusMessage);
        manager->play(99);
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains("Invalid track index"));
    }

    void testStop_EmitsPlaybackStopped()
    {
        addTestItem(manager, "/SD/a.sid");
        manager->play(0);
        QSignalSpy spy(manager, &PlaylistService::playbackStopped);
        manager->stop();
        QCOMPARE(spy.count(), 1);
    }

    void testStop_SetsIsPlayingFalse()
    {
        addTestItem(manager, "/SD/a.sid");
        manager->play(0);
        manager->stop();
        QVERIFY(!manager->isPlaying());
    }

    void testNext_WithTwoItems_AdvancesCurrentIndex()
    {
        addTestItem(manager, "/SD/a.sid");
        addTestItem(manager, "/SD/b.sid");
        manager->play(0);
        QSignalSpy spy(manager, &PlaylistService::currentIndexChanged);
        manager->next();
        QVERIFY(spy.count() >= 1);
        QCOMPARE(manager->currentIndex(), 1);
    }

    void testNext_EmptyPlaylist_EmitsPlaylistIsEmptyStatusMessage()
    {
        QSignalSpy spy(manager, &PlaylistService::statusMessage);
        manager->next();
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains("Playlist is empty"));
    }

    void testNext_EmptyPlaylist_NoCrash()
    {
        QSignalSpy spy(manager, &PlaylistService::playbackStarted);
        // Should not crash; no playback signal should be emitted
        manager->next();
        QCOMPARE(spy.count(), 0);
    }

    void testPrevious_WithTwoItemsAtIndex1_GoesBackToIndex0()
    {
        addTestItem(manager, "/SD/a.sid");
        addTestItem(manager, "/SD/b.sid");
        manager->play(0);
        manager->next();  // advance to index 1
        QSignalSpy spy(manager, &PlaylistService::currentIndexChanged);
        manager->previous();
        QVERIFY(spy.count() >= 1);
        QCOMPARE(manager->currentIndex(), 0);
    }

    void testPrevious_EmptyPlaylist_EmitsPlaylistIsEmptyStatusMessage()
    {
        QSignalSpy spy(manager, &PlaylistService::statusMessage);
        manager->previous();
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains("Playlist is empty"));
    }

    void testPrevious_AtFirstTrack_EmitsAlreadyAtFirstTrackStatusMessage()
    {
        // Add one item and play it (at index 0); previous() has nowhere to go
        addTestItem(manager, "/SD/a.sid");
        manager->play(0);
        // Consume the statusMessage from play() (device not connected)
        QSignalSpy spy(manager, &PlaylistService::statusMessage);
        manager->previous();
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains("Already at first track"));
    }

    // =========================================================
    // setItemDuration() — guard clause
    // =========================================================

    void testSetItemDuration_InvalidIndex_EmitsInvalidTrackIndexStatusMessage()
    {
        QSignalSpy spy(manager, &PlaylistService::statusMessage);
        manager->setItemDuration(99, 60);
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains("Invalid track index"));
    }

    void testSetItemDuration_ValidIndex_EmitsPlaylistChanged()
    {
        addTestItem(manager, "/SD/a.sid");
        QSignalSpy spy(manager, &PlaylistService::playlistChanged);
        manager->setItemDuration(0, 60);
        QCOMPARE(spy.count(), 1);
    }

    // =========================================================
    // Settings - signals
    // =========================================================

    void testSetShuffle_True_EmitsShuffleChanged()
    {
        QSignalSpy spy(manager, &PlaylistService::shuffleChanged);
        manager->setShuffle(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toBool(), true);
    }

    void testSetShuffle_TwiceWithSameValue_OnlyEmitsOnce()
    {
        manager->setShuffle(true);
        QSignalSpy spy(manager, &PlaylistService::shuffleChanged);
        manager->setShuffle(true);  // second call with same value — should be no-op
        QCOMPARE(spy.count(), 0);
    }

    void testSetRepeatMode_All_EmitsRepeatModeChanged()
    {
        QSignalSpy spy(manager, &PlaylistService::repeatModeChanged);
        manager->setRepeatMode(PlaylistService::RepeatMode::All);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().value<PlaylistService::RepeatMode>(),
                 PlaylistService::RepeatMode::All);
    }

    void testSetDefaultDuration_ValidValue_EmitsDefaultDurationChanged()
    {
        QSignalSpy spy(manager, &PlaylistService::defaultDurationChanged);
        manager->setDefaultDuration(60);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toInt(), 60);
    }

    void testSetDefaultDuration_BelowMinimum_ClampsTo10()
    {
        QSignalSpy spy(manager, &PlaylistService::defaultDurationChanged);
        manager->setDefaultDuration(5);  // below minimum of 10
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toInt(), 10);
        QCOMPARE(manager->defaultDuration(), 10);
    }

    void testSetDefaultDuration_AboveMaximum_ClampsTo3600()
    {
        QSignalSpy spy(manager, &PlaylistService::defaultDurationChanged);
        manager->setDefaultDuration(7200);  // above maximum of 3600
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toInt(), 3600);
        QCOMPARE(manager->defaultDuration(), 3600);
    }

    // =========================================================
    // Persistence - save/load round-trip
    // =========================================================

    void testSaveAndLoadPlaylist_RoundTripPreservesItems()
    {
        addTestItem(manager, "/SD/song1.sid");
        addTestItem(manager, "/SD/song2.sid");

        QTemporaryFile file;
        QVERIFY(file.open());
        const QString filePath = file.fileName();
        file.close();

        QVERIFY(manager->savePlaylist(filePath));

        // Create a fresh manager and load the playlist
        QSettings settings;
        settings.remove("playlist");
        PlaylistService loader(nullptr);
        QVERIFY(loader.loadPlaylist(filePath));

        QCOMPARE(loader.count(), 2);
        QCOMPARE(loader.itemAt(0).path, QString("/SD/song1.sid"));
        QCOMPARE(loader.itemAt(1).path, QString("/SD/song2.sid"));
    }
};

QTEST_MAIN(TestPlaylistService)
#include "test_playlistservice.moc"
