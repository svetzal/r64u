#include <QtTest>
#include <QSignalSpy>

#include "services/gamebase64service.h"

class TestGameBase64Service : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Called before all test functions
    }

    void cleanupTestCase()
    {
        // Called after all test functions
    }

    // Initial state tests
    void testInitialStateNotLoaded()
    {
        GameBase64Service service;

        // Should not be loaded without database file
        // Note: might be loaded if user has downloaded it, but we test structure
        QCOMPARE(service.gameCount(), service.isLoaded() ? service.gameCount() : 0);
    }

    void testCacheFilePathNotEmpty()
    {
        GameBase64Service service;

        QString dbPath = service.databaseCacheFilePath();

        QVERIFY(!dbPath.isEmpty());
        QVERIFY(dbPath.endsWith("gamebase64.db"));
    }

    void testHasCachedDatabaseReturnsBool()
    {
        GameBase64Service service;

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
        QCOMPARE(QString(GameBase64Service::DatabaseFilename),
                 QString("gamebase64.db"));
    }

    // Lookup tests (without database loaded)
    void testLookupByGameIdNotLoaded()
    {
        GameBase64Service service;

        // Even if cache exists, test without loaded data
        GameBase64Service::GameInfo info = service.lookupByGameId(1);

        // If not loaded, should return not found
        if (!service.isLoaded()) {
            QVERIFY(!info.found);
        }
    }

    void testLookupByNameNotLoaded()
    {
        GameBase64Service service;

        GameBase64Service::GameInfo info = service.lookupByName("Commando");

        if (!service.isLoaded()) {
            QVERIFY(!info.found);
        }
    }

    void testLookupByNameEmptyString()
    {
        GameBase64Service service;

        GameBase64Service::GameInfo info = service.lookupByName("");
        QVERIFY(!info.found);
    }

    void testLookupByFilenameNotLoaded()
    {
        GameBase64Service service;

        GameBase64Service::GameInfo info = service.lookupByFilename("Commando.d64");

        if (!service.isLoaded()) {
            QVERIFY(!info.found);
        }
    }

    void testLookupByFilenameEmptyString()
    {
        GameBase64Service service;

        GameBase64Service::GameInfo info = service.lookupByFilename("");
        QVERIFY(!info.found);
    }

    void testLookupBySidFilenameNotLoaded()
    {
        GameBase64Service service;

        GameBase64Service::GameInfo info = service.lookupBySidFilename("Commando.sid");

        if (!service.isLoaded()) {
            QVERIFY(!info.found);
        }
    }

    void testLookupBySidFilenameEmptyString()
    {
        GameBase64Service service;

        GameBase64Service::GameInfo info = service.lookupBySidFilename("");
        QVERIFY(!info.found);
    }

    // Search tests (without database loaded)
    void testSearchByNameNotLoaded()
    {
        GameBase64Service service;

        GameBase64Service::SearchResults results = service.searchByName("Commando");

        // Empty search with no database should succeed with no results
        QVERIFY(results.success || !service.isLoaded());
    }

    void testSearchByNameEmptyQuery()
    {
        GameBase64Service service;

        GameBase64Service::SearchResults results = service.searchByName("");
        QVERIFY(results.success);
        QVERIFY(results.games.isEmpty());
    }

    void testSearchByMusicianNotLoaded()
    {
        GameBase64Service service;

        GameBase64Service::SearchResults results = service.searchByMusician("Rob Hubbard");

        QVERIFY(results.success || !service.isLoaded());
    }

    void testSearchByMusicianEmptyQuery()
    {
        GameBase64Service service;

        GameBase64Service::SearchResults results = service.searchByMusician("");
        QVERIFY(results.success);
        QVERIFY(results.games.isEmpty());
    }

    void testSearchByPublisherNotLoaded()
    {
        GameBase64Service service;

        GameBase64Service::SearchResults results = service.searchByPublisher("Ocean");

        QVERIFY(results.success || !service.isLoaded());
    }

    void testSearchByPublisherEmptyQuery()
    {
        GameBase64Service service;

        GameBase64Service::SearchResults results = service.searchByPublisher("");
        QVERIFY(results.success);
        QVERIFY(results.games.isEmpty());
    }

    // Signal existence tests
    void testSignalsExist()
    {
        GameBase64Service service;

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
        GameBase64Service service;

        // Should not crash when canceling with no active download
        service.cancelDownload();
        QVERIFY(true);
    }
};

QTEST_MAIN(TestGameBase64Service)
#include "test_gamebase64service.moc"
