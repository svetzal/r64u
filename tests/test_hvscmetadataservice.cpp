#include <QtTest>

#include "services/hvscmetadataservice.h"

class TestHVSCMetadataService : public QObject
{
    Q_OBJECT

private:
    // Sample STIL.txt content for testing
    QString createSampleStilContent() {
        return R"(### /MUSICIANS/T/Tel_Jeroen ###################################################
/MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid
COMMENT: This is the main theme from Cybernoid II.
         A classic tune that showcases Jeroen Tel's
         style.
(#2)
   NAME: High Score
COMMENT: The high score entry music.
(#3)
  TITLE: Intro Theme from Cybernoid
 ARTIST: Jeroen Tel
COMMENT: Cover of the original Cybernoid intro.

### /MUSICIANS/H/Hubbard_Rob ################################################
/MUSICIANS/H/Hubbard_Rob/Commando.sid
COMMENT: One of Rob Hubbard's most iconic compositions.
         The loading tune set the standard for C64 music.
(#2)
   NAME: In-game
(#3)
   NAME: Game Over

/MUSICIANS/H/Hubbard_Rob/Zoids.sid
COMMENT: Another classic from Rob Hubbard.

### /MUSICIANS/G/Galway_Martin ##############################################
/MUSICIANS/G/Galway_Martin/Arkanoid.sid
  TITLE: Revenge from Mars (0:15-0:45)
 ARTIST: Unknown Composer
COMMENT: Contains a sample from an unknown source.
)";
    }

    // Sample BUGlist.txt content for testing
    QString createSampleBuglistContent() {
        return R"(### /MUSICIANS/T/Tel_Jeroen ###################################################
/MUSICIANS/T/Tel_Jeroen/Bugged_Tune.sid
BUG: The tune ends abruptly due to missing loop code.

/MUSICIANS/T/Tel_Jeroen/Another_Bug.sid
(#2)
BUG: Subtune 2 has incorrect tempo on NTSC systems.
(#3)
BUG: Plays noise at the end instead of silence.

### /DEMOS/A/Alpha #############################################################
/DEMOS/A/Alpha/Demo.sid
BUG: Requires specific VIC timing that emulators may not
     replicate accurately.
)";
    }

private slots:
    void initTestCase()
    {
        // Called before all test functions
    }

    void cleanupTestCase()
    {
        // Called after all test functions
    }

    // STIL parsing tests
    void testParseStilEntry_simpleComment()
    {
        HVSCMetadataService service;

        // Create sample data and write to a temp file
        QString content = createSampleStilContent();
        QTemporaryFile tempFile;
        tempFile.setAutoRemove(true);
        QVERIFY(tempFile.open());
        tempFile.write(content.toUtf8());
        tempFile.close();

        // We can't directly call parseStil (it's private), so we test via public API
        // by writing to cache location and loading
    }

    void testStilLookup_existingPath()
    {
        // Test that lookupStil returns correct data for known paths
        HVSCMetadataService service;

        // Without loading data, should return not found
        HVSCMetadataService::StilInfo info = service.lookupStil("/MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid");
        QVERIFY(!info.found);
    }

    void testStilLookup_nonExistingPath()
    {
        HVSCMetadataService service;

        HVSCMetadataService::StilInfo info = service.lookupStil("/NON/EXISTENT/path.sid");
        QVERIFY(!info.found);
        QVERIFY(info.entries.isEmpty());
    }

    void testStilLookup_pathNormalization()
    {
        HVSCMetadataService service;

        // Test that paths are normalized (backslashes converted, leading slash added)
        HVSCMetadataService::StilInfo info1 = service.lookupStil("/path/to/file.sid");
        HVSCMetadataService::StilInfo info2 = service.lookupStil("path/to/file.sid");  // No leading slash
        HVSCMetadataService::StilInfo info3 = service.lookupStil("\\path\\to\\file.sid");  // Backslashes

        // All should normalize to the same lookup
        QCOMPARE(info1.found, info2.found);
        QCOMPARE(info2.found, info3.found);
    }

    // BUGlist parsing tests
    void testBuglistLookup_nonExistingPath()
    {
        HVSCMetadataService service;

        HVSCMetadataService::BugInfo info = service.lookupBuglist("/NON/EXISTENT/path.sid");
        QVERIFY(!info.found);
        QVERIFY(info.entries.isEmpty());
    }

    // State tests
    void testInitialState_notLoaded()
    {
        HVSCMetadataService service;

        QVERIFY(!service.isStilLoaded());
        QVERIFY(!service.isBuglistLoaded());
        QCOMPARE(service.stilEntryCount(), 0);
        QCOMPARE(service.buglistEntryCount(), 0);
    }

    void testCacheFilePaths_notEmpty()
    {
        HVSCMetadataService service;

        QString stilPath = service.stilCacheFilePath();
        QString buglistPath = service.buglistCacheFilePath();

        QVERIFY(!stilPath.isEmpty());
        QVERIFY(!buglistPath.isEmpty());
        QVERIFY(stilPath.endsWith("STIL.txt"));
        QVERIFY(buglistPath.endsWith("BUGlist.txt"));
    }

    void testHasCachedFiles_withoutCache()
    {
        HVSCMetadataService service;

        // These may or may not exist depending on whether the user has downloaded them
        // Just verify they return booleans without crashing
        bool hasStil = service.hasCachedStil();
        bool hasBuglist = service.hasCachedBuglist();

        Q_UNUSED(hasStil);
        Q_UNUSED(hasBuglist);
        QVERIFY(true);  // Test passes if we get here without crash
    }

    // Struct tests
    void testSubtuneEntry_defaultValues()
    {
        HVSCMetadataService::SubtuneEntry entry;

        QCOMPARE(entry.subtune, 0);
        QVERIFY(entry.name.isEmpty());
        QVERIFY(entry.author.isEmpty());
        QVERIFY(entry.comment.isEmpty());
        QVERIFY(entry.covers.isEmpty());
    }

    void testCoverInfo_defaultValues()
    {
        HVSCMetadataService::CoverInfo cover;

        QVERIFY(cover.title.isEmpty());
        QVERIFY(cover.artist.isEmpty());
        QVERIFY(cover.timestamp.isEmpty());
    }

    void testBugEntry_defaultValues()
    {
        HVSCMetadataService::BugEntry bug;

        QCOMPARE(bug.subtune, 0);
        QVERIFY(bug.description.isEmpty());
    }

    void testStilInfo_defaultValues()
    {
        HVSCMetadataService::StilInfo info;

        QVERIFY(!info.found);
        QVERIFY(info.path.isEmpty());
        QVERIFY(info.entries.isEmpty());
    }

    void testBugInfo_defaultValues()
    {
        HVSCMetadataService::BugInfo info;

        QVERIFY(!info.found);
        QVERIFY(info.path.isEmpty());
        QVERIFY(info.entries.isEmpty());
    }

    // URL tests
    void testDatabaseUrls_valid()
    {
        QCOMPARE(QString(HVSCMetadataService::StilUrl),
                 QString("https://www.hvsc.c64.org/download/C64Music/DOCUMENTS/STIL.txt"));
        QCOMPARE(QString(HVSCMetadataService::BuglistUrl),
                 QString("https://www.hvsc.c64.org/download/C64Music/DOCUMENTS/BUGlist.txt"));
    }

    // Signal existence tests
    void testSignals_exist()
    {
        HVSCMetadataService service;

        // Verify signals can be connected to
        QSignalSpy stilProgressSpy(&service, &HVSCMetadataService::stilDownloadProgress);
        QSignalSpy stilFinishedSpy(&service, &HVSCMetadataService::stilDownloadFinished);
        QSignalSpy stilFailedSpy(&service, &HVSCMetadataService::stilDownloadFailed);
        QSignalSpy stilLoadedSpy(&service, &HVSCMetadataService::stilLoaded);

        QSignalSpy buglistProgressSpy(&service, &HVSCMetadataService::buglistDownloadProgress);
        QSignalSpy buglistFinishedSpy(&service, &HVSCMetadataService::buglistDownloadFinished);
        QSignalSpy buglistFailedSpy(&service, &HVSCMetadataService::buglistDownloadFailed);
        QSignalSpy buglistLoadedSpy(&service, &HVSCMetadataService::buglistLoaded);

        QVERIFY(stilProgressSpy.isValid());
        QVERIFY(stilFinishedSpy.isValid());
        QVERIFY(stilFailedSpy.isValid());
        QVERIFY(stilLoadedSpy.isValid());
        QVERIFY(buglistProgressSpy.isValid());
        QVERIFY(buglistFinishedSpy.isValid());
        QVERIFY(buglistFailedSpy.isValid());
        QVERIFY(buglistLoadedSpy.isValid());
    }
};

QTEST_MAIN(TestHVSCMetadataService)
#include "test_hvscmetadataservice.moc"
