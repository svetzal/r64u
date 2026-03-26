#include "mocks/mockfiledownloader.h"
#include "services/gamebase64service.h"

#include <QSignalSpy>
#include <QtTest>

class TestGameBase64Service : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        // Called before all test functions
    }

    void cleanupTestCase()
    {
        // Called after all test functions
    }

    // Initial state tests
    void testInitialStateNotLoaded()
    {
        MockFileDownloader mock;
        GameBase64Service service(&mock);

        // Should not be loaded without database file
        // Note: might be loaded if user has downloaded it, but we test structure
        QCOMPARE(service.gameCount(), service.isLoaded() ? service.gameCount() : 0);
    }

    void testCacheFilePathNotEmpty()
    {
        MockFileDownloader mock;
        GameBase64Service service(&mock);

        QString dbPath = service.databaseCacheFilePath();

        QVERIFY(!dbPath.isEmpty());
        QVERIFY(dbPath.endsWith("gamebase64.db"));
    }

    void testHasCachedDatabaseReturnsBool()
    {
        MockFileDownloader mock;
        GameBase64Service service(&mock);

        // Just verify it returns without crash
        bool hasCached = service.hasCachedDatabase();
        Q_UNUSED(hasCached);
        QVERIFY(true);
    }

    // Struct tests
    void testGameInfoDefaultValues()
    {
        GameBase64Service::GameInfo info;

        QVERIFY(!info.found);
        QCOMPARE(info.gameId, 0);
        QVERIFY(info.name.isEmpty());
        QVERIFY(info.publisher.isEmpty());
        QCOMPARE(info.year, 0);
        QVERIFY(info.genre.isEmpty());
        QVERIFY(info.parentGenre.isEmpty());
        QVERIFY(info.musician.isEmpty());
        QVERIFY(info.musicianGroup.isEmpty());
        QVERIFY(info.filename.isEmpty());
        QVERIFY(info.screenshotFilename.isEmpty());
        QVERIFY(info.sidFilename.isEmpty());
        QCOMPARE(info.rating, 0);
        QCOMPARE(info.playersFrom, 1);
        QCOMPARE(info.playersTo, 1);
        QVERIFY(info.memo.isEmpty());
        QVERIFY(info.comment.isEmpty());
    }

    void testSearchResultsDefaultValues()
    {
        GameBase64Service::SearchResults results;

        QVERIFY(!results.success);
        QVERIFY(results.error.isEmpty());
        QVERIFY(results.games.isEmpty());
    }

    // URL tests
    void testDatabaseUrlValid()
    {
        QCOMPARE(QString(GameBase64Service::DatabaseUrl),
                 QString("http://www.twinbirds.com/gamebase64browser/GBC_v18.sqlitedb.gz"));
        QCOMPARE(QString(GameBase64Service::DatabaseFilename), QString("gamebase64.db"));
    }

    // Lookup tests (without database loaded)
    void testLookupByGameIdNotLoaded()
    {
        MockFileDownloader mock;
        GameBase64Service service(&mock);

        // Even if cache exists, test without loaded data
        GameBase64Service::GameInfo info = service.lookupByGameId(1);

        // If not loaded, should return not found
        if (!service.isLoaded()) {
            QVERIFY(!info.found);
        }
    }

    void testLookupByNameNotLoaded()
    {
        MockFileDownloader mock;
        GameBase64Service service(&mock);

        GameBase64Service::GameInfo info = service.lookupByName("Commando");

        if (!service.isLoaded()) {
            QVERIFY(!info.found);
        }
    }

    void testLookupByNameEmptyString()
    {
        MockFileDownloader mock;
        GameBase64Service service(&mock);

        GameBase64Service::GameInfo info = service.lookupByName("");
        QVERIFY(!info.found);
    }

    void testLookupByFilenameNotLoaded()
    {
        MockFileDownloader mock;
        GameBase64Service service(&mock);

        GameBase64Service::GameInfo info = service.lookupByFilename("Commando.d64");

        if (!service.isLoaded()) {
            QVERIFY(!info.found);
        }
    }

    void testLookupByFilenameEmptyString()
    {
        MockFileDownloader mock;
        GameBase64Service service(&mock);

        GameBase64Service::GameInfo info = service.lookupByFilename("");
        QVERIFY(!info.found);
    }

    void testLookupBySidFilenameNotLoaded()
    {
        MockFileDownloader mock;
        GameBase64Service service(&mock);

        GameBase64Service::GameInfo info = service.lookupBySidFilename("Commando.sid");

        if (!service.isLoaded()) {
            QVERIFY(!info.found);
        }
    }

    void testLookupBySidFilenameEmptyString()
    {
        MockFileDownloader mock;
        GameBase64Service service(&mock);

        GameBase64Service::GameInfo info = service.lookupBySidFilename("");
        QVERIFY(!info.found);
    }

    // Search tests (without database loaded)
    void testSearchByNameNotLoaded()
    {
        MockFileDownloader mock;
        GameBase64Service service(&mock);

        GameBase64Service::SearchResults results = service.searchByName("Commando");

        // Empty search with no database should succeed with no results
        QVERIFY(results.success || !service.isLoaded());
    }

    void testSearchByNameEmptyQuery()
    {
        MockFileDownloader mock;
        GameBase64Service service(&mock);

        GameBase64Service::SearchResults results = service.searchByName("");
        QVERIFY(results.success);
        QVERIFY(results.games.isEmpty());
    }

    void testSearchByMusicianNotLoaded()
    {
        MockFileDownloader mock;
        GameBase64Service service(&mock);

        GameBase64Service::SearchResults results = service.searchByMusician("Rob Hubbard");

        QVERIFY(results.success || !service.isLoaded());
    }

    void testSearchByMusicianEmptyQuery()
    {
        MockFileDownloader mock;
        GameBase64Service service(&mock);

        GameBase64Service::SearchResults results = service.searchByMusician("");
        QVERIFY(results.success);
        QVERIFY(results.games.isEmpty());
    }

    void testSearchByPublisherNotLoaded()
    {
        MockFileDownloader mock;
        GameBase64Service service(&mock);

        GameBase64Service::SearchResults results = service.searchByPublisher("Ocean");

        QVERIFY(results.success || !service.isLoaded());
    }

    void testSearchByPublisherEmptyQuery()
    {
        MockFileDownloader mock;
        GameBase64Service service(&mock);

        GameBase64Service::SearchResults results = service.searchByPublisher("");
        QVERIFY(results.success);
        QVERIFY(results.games.isEmpty());
    }

    // Signal existence tests
    void testSignalsExist()
    {
        MockFileDownloader mock;
        GameBase64Service service(&mock);

        QSignalSpy progressSpy(&service, &GameBase64Service::downloadProgress);
        QSignalSpy finishedSpy(&service, &GameBase64Service::downloadFinished);
        QSignalSpy failedSpy(&service, &GameBase64Service::downloadFailed);
        QSignalSpy loadedSpy(&service, &GameBase64Service::databaseLoaded);
        QSignalSpy unloadedSpy(&service, &GameBase64Service::databaseUnloaded);

        QVERIFY(progressSpy.isValid());
        QVERIFY(finishedSpy.isValid());
        QVERIFY(failedSpy.isValid());
        QVERIFY(loadedSpy.isValid());
        QVERIFY(unloadedSpy.isValid());
    }

    // Cancel download test
    void testCancelDownloadWhenNotDownloading()
    {
        MockFileDownloader mock;
        GameBase64Service service(&mock);

        // Should not crash when canceling with no active download
        service.cancelDownload();
        QVERIFY(true);
    }

    // --- Orchestration tests ---

    void testDownloadDatabase_callsDownloaderWithCorrectUrl()
    {
        MockFileDownloader mock;
        GameBase64Service service(&mock);

        service.downloadDatabase();

        QCOMPARE(mock.mockDownloadCallCount(), 1);
        QCOMPARE(mock.mockLastUrl(), QUrl(QString::fromLatin1(GameBase64Service::DatabaseUrl)));
    }

    void testDownloadDatabase_ignoresWhenAlreadyDownloading()
    {
        MockFileDownloader mock;
        GameBase64Service service(&mock);

        service.downloadDatabase();
        service.downloadDatabase();  // second call should be ignored

        QCOMPARE(mock.mockDownloadCallCount(), 1);
    }

    void testDownloadProgress_forwardsSignal()
    {
        MockFileDownloader mock;
        GameBase64Service service(&mock);

        QSignalSpy spy(&service, &GameBase64Service::downloadProgress);

        service.downloadDatabase();
        mock.mockEmitProgress(1024, 4096);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toLongLong(), 1024LL);
        QCOMPARE(spy.at(0).at(1).toLongLong(), 4096LL);
    }

    void testDownloadFailed_forwardsError()
    {
        MockFileDownloader mock;
        GameBase64Service service(&mock);

        QSignalSpy spy(&service, &GameBase64Service::downloadFailed);

        service.downloadDatabase();
        mock.mockEmitFailed(QStringLiteral("Timeout"));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("Timeout"));
    }

    void testCancelDownload_delegatesToDownloader()
    {
        MockFileDownloader mock;
        GameBase64Service service(&mock);

        service.downloadDatabase();
        service.cancelDownload();

        QCOMPARE(mock.mockCancelCallCount(), 1);
        QVERIFY(!mock.mockIsDownloading());
    }
};

QTEST_MAIN(TestGameBase64Service)
#include "test_gamebase64service.moc"
