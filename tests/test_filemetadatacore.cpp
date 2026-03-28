/**
 * @file test_filemetadatacore.cpp
 * @brief Tests for the pure filemetadata namespace formatting functions.
 *
 * Verifies that the correct sections are included or excluded from the
 * formatted output based on the availability flags in the context structs.
 * All tests are purely in-process with no UI, no database, no network.
 */

#include "services/filemetadatacore.h"

#include <QTest>

class TestFileMetadataCore : public QObject
{
    Q_OBJECT

private slots:
    // ── formatSidDetails ─────────────────────────────────────────────────────

    void testFormatSidDetails_includesBaseSidInfo();
    void testFormatSidDetails_songlengthsServiceAbsent_noSonglengthsSection();
    void testFormatSidDetails_songlengthsNotLoaded_showsNotLoadedMessage();
    void testFormatSidDetails_songlengthsLoadedAndFound_showsSongs();
    void testFormatSidDetails_songlengthsLoadedButNotFound_showsNotFoundMessage();
    void testFormatSidDetails_buglistLoaded_withBugs_showsBugSection();
    void testFormatSidDetails_buglistLoaded_noBugs_noBugSection();
    void testFormatSidDetails_stilLoaded_withEntries_showsStilSection();
    void testFormatSidDetails_stilLoaded_noEntries_noStilSection();
    void testFormatSidDetails_gameBase64ServiceAbsent_showsServiceNotAvailable();
    void testFormatSidDetails_gameBase64NotLoaded_showsDownloadPrompt();
    void testFormatSidDetails_gameBase64LoadedAndFound_showsGameInfo();
    void testFormatSidDetails_gameBase64LoadedButNotFound_noGameSection();

    // ── formatDiskDetails ────────────────────────────────────────────────────

    void testFormatDiskDetails_includesDirectoryListing();
    void testFormatDiskDetails_gameBase64ServiceAbsent_showsServiceNotAvailable();
    void testFormatDiskDetails_gameBase64NotLoaded_showsDownloadPrompt();
    void testFormatDiskDetails_gameBase64LoadedAndFound_showsGameInfo();
    void testFormatDiskDetails_gameBase64LoadedButNotFound_noGameSection();
    void testFormatDiskDetails_gameInfo_showsMusicianAndRating();
};

// ── Helpers ──────────────────────────────────────────────────────────────────

namespace {

SidFileParser::SidInfo makeValidSidInfo(const QString &title = "Test Tune",
                                        const QString &author = "Test Author")
{
    SidFileParser::SidInfo info;
    info.valid = true;
    info.title = title;
    info.author = author;
    info.released = "2024";
    info.songs = 1;
    info.startSong = 1;
    info.version = 2;
    return info;
}

filemetadata::SidDisplayContext makeMinimalSidContext()
{
    filemetadata::SidDisplayContext ctx;
    ctx.sidInfo = makeValidSidInfo();
    // All services absent by default — only base SID info
    return ctx;
}

filemetadata::DiskDisplayContext makeMinimalDiskContext(const QString &listing = "BUBBLE BOBBLE")
{
    filemetadata::DiskDisplayContext ctx;
    ctx.directoryListing = listing;
    return ctx;
}

GameBase64Service::GameInfo makeGameInfo(const QString &name = "Test Game", int year = 1987)
{
    GameBase64Service::GameInfo info;
    info.found = true;
    info.name = name;
    info.year = year;
    info.publisher = "Test Publisher";
    info.genre = "Action";
    info.parentGenre = "Arcade";
    info.musician = "Rob Hubbard";
    info.musicianGroup = "Freelance";
    info.rating = 9;
    info.playersFrom = 1;
    info.playersTo = 2;
    return info;
}

}  // namespace

// ── formatSidDetails ─────────────────────────────────────────────────────────

void TestFileMetadataCore::testFormatSidDetails_includesBaseSidInfo()
{
    filemetadata::SidDisplayContext ctx = makeMinimalSidContext();
    ctx.sidInfo.title = "Cybernoid II";
    ctx.sidInfo.author = "Jeroen Tel";

    QString result = filemetadata::formatSidDetails(ctx);

    QVERIFY(result.contains("Cybernoid II"));
    QVERIFY(result.contains("Jeroen Tel"));
}

void TestFileMetadataCore::testFormatSidDetails_songlengthsServiceAbsent_noSonglengthsSection()
{
    filemetadata::SidDisplayContext ctx = makeMinimalSidContext();
    ctx.songlengthsServicePresent = false;

    QString result = filemetadata::formatSidDetails(ctx);

    // Should not mention HVSC database at all when service not wired up
    QVERIFY(!result.contains("HVSC Database"));
}

void TestFileMetadataCore::testFormatSidDetails_songlengthsNotLoaded_showsNotLoadedMessage()
{
    filemetadata::SidDisplayContext ctx = makeMinimalSidContext();
    ctx.songlengthsServicePresent = true;
    ctx.songlengthsDatabaseLoaded = false;

    QString result = filemetadata::formatSidDetails(ctx);

    QVERIFY(result.contains("Not loaded"));
}

void TestFileMetadataCore::testFormatSidDetails_songlengthsLoadedAndFound_showsSongs()
{
    filemetadata::SidDisplayContext ctx = makeMinimalSidContext();
    ctx.songlengthsServicePresent = true;
    ctx.songlengthsDatabaseLoaded = true;
    ctx.songLengths.found = true;
    ctx.songLengths.hvscPath = "/MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid";
    ctx.songLengths.formattedTimes = {"3:57", "1:23"};

    QString result = filemetadata::formatSidDetails(ctx);

    QVERIFY(result.contains("HVSC Database: Found"));
    QVERIFY(result.contains("3:57"));
    QVERIFY(result.contains("1:23"));
    QVERIFY(result.contains("Song 1"));
    QVERIFY(result.contains("Song 2"));
}

void TestFileMetadataCore::testFormatSidDetails_songlengthsLoadedButNotFound_showsNotFoundMessage()
{
    filemetadata::SidDisplayContext ctx = makeMinimalSidContext();
    ctx.songlengthsServicePresent = true;
    ctx.songlengthsDatabaseLoaded = true;
    ctx.songLengths.found = false;

    QString result = filemetadata::formatSidDetails(ctx);

    QVERIFY(result.contains("HVSC Database: Not found"));
    QVERIFY(result.contains("default 3:00"));
}

void TestFileMetadataCore::testFormatSidDetails_buglistLoaded_withBugs_showsBugSection()
{
    filemetadata::SidDisplayContext ctx = makeMinimalSidContext();
    ctx.songlengthsServicePresent = true;
    ctx.songlengthsDatabaseLoaded = true;
    ctx.songLengths.found = true;
    ctx.songLengths.hvscPath = "/some/path.sid";
    ctx.buglistLoaded = true;
    ctx.bugInfo.found = true;

    HVSCMetadataService::BugEntry bug;
    bug.subtune = 0;
    bug.description = "Plays too fast on NTSC";
    ctx.bugInfo.entries.append(bug);

    QString result = filemetadata::formatSidDetails(ctx);

    QVERIFY(result.contains("KNOWN ISSUES"));
    QVERIFY(result.contains("Plays too fast on NTSC"));
}

void TestFileMetadataCore::testFormatSidDetails_buglistLoaded_noBugs_noBugSection()
{
    filemetadata::SidDisplayContext ctx = makeMinimalSidContext();
    ctx.buglistLoaded = true;
    ctx.bugInfo.found = false;

    QString result = filemetadata::formatSidDetails(ctx);

    QVERIFY(!result.contains("KNOWN ISSUES"));
}

void TestFileMetadataCore::testFormatSidDetails_stilLoaded_withEntries_showsStilSection()
{
    filemetadata::SidDisplayContext ctx = makeMinimalSidContext();
    ctx.songlengthsServicePresent = true;
    ctx.songlengthsDatabaseLoaded = true;
    ctx.songLengths.found = true;
    ctx.songLengths.hvscPath = "/some/path.sid";
    ctx.stilLoaded = true;
    ctx.stilInfo.found = true;

    HVSCMetadataService::SubtuneEntry entry;
    entry.subtune = 0;
    entry.comment = "This is a great tune!";
    ctx.stilInfo.entries.append(entry);

    QString result = filemetadata::formatSidDetails(ctx);

    QVERIFY(result.contains("STIL INFORMATION"));
    QVERIFY(result.contains("This is a great tune!"));
}

void TestFileMetadataCore::testFormatSidDetails_stilLoaded_noEntries_noStilSection()
{
    filemetadata::SidDisplayContext ctx = makeMinimalSidContext();
    ctx.stilLoaded = true;
    ctx.stilInfo.found = false;

    QString result = filemetadata::formatSidDetails(ctx);

    QVERIFY(!result.contains("STIL INFORMATION"));
}

void TestFileMetadataCore::testFormatSidDetails_gameBase64ServiceAbsent_showsServiceNotAvailable()
{
    filemetadata::SidDisplayContext ctx = makeMinimalSidContext();
    ctx.gameBase64ServicePresent = false;

    QString result = filemetadata::formatSidDetails(ctx);

    QVERIFY(result.contains("GameBase64 service not available"));
}

void TestFileMetadataCore::testFormatSidDetails_gameBase64NotLoaded_showsDownloadPrompt()
{
    filemetadata::SidDisplayContext ctx = makeMinimalSidContext();
    ctx.gameBase64ServicePresent = true;
    ctx.gameBase64DatabaseLoaded = false;

    QString result = filemetadata::formatSidDetails(ctx);

    QVERIFY(result.contains("Download GameBase64"));
}

void TestFileMetadataCore::testFormatSidDetails_gameBase64LoadedAndFound_showsGameInfo()
{
    filemetadata::SidDisplayContext ctx = makeMinimalSidContext();
    ctx.gameBase64ServicePresent = true;
    ctx.gameBase64DatabaseLoaded = true;
    ctx.gameInfo = makeGameInfo("Cybernoid", 1988);

    QString result = filemetadata::formatSidDetails(ctx);

    QVERIFY(result.contains("GAME INFO"));
    QVERIFY(result.contains("Cybernoid"));
    QVERIFY(result.contains("1988"));
}

void TestFileMetadataCore::testFormatSidDetails_gameBase64LoadedButNotFound_noGameSection()
{
    filemetadata::SidDisplayContext ctx = makeMinimalSidContext();
    ctx.gameBase64ServicePresent = true;
    ctx.gameBase64DatabaseLoaded = true;
    ctx.gameInfo.found = false;

    QString result = filemetadata::formatSidDetails(ctx);

    QVERIFY(!result.contains("GAME INFO"));
}

// ── formatDiskDetails ────────────────────────────────────────────────────────

void TestFileMetadataCore::testFormatDiskDetails_includesDirectoryListing()
{
    filemetadata::DiskDisplayContext ctx = makeMinimalDiskContext("0 \"BUBBLE BOBBLE\" BB 2A");
    ctx.gameBase64ServicePresent = true;
    ctx.gameBase64DatabaseLoaded = true;

    QString result = filemetadata::formatDiskDetails(ctx);

    QVERIFY(result.contains("BUBBLE BOBBLE"));
}

void TestFileMetadataCore::testFormatDiskDetails_gameBase64ServiceAbsent_showsServiceNotAvailable()
{
    filemetadata::DiskDisplayContext ctx = makeMinimalDiskContext();
    ctx.gameBase64ServicePresent = false;

    QString result = filemetadata::formatDiskDetails(ctx);

    QVERIFY(result.contains("GameBase64 service not available"));
}

void TestFileMetadataCore::testFormatDiskDetails_gameBase64NotLoaded_showsDownloadPrompt()
{
    filemetadata::DiskDisplayContext ctx = makeMinimalDiskContext();
    ctx.gameBase64ServicePresent = true;
    ctx.gameBase64DatabaseLoaded = false;

    QString result = filemetadata::formatDiskDetails(ctx);

    QVERIFY(result.contains("Download GameBase64"));
}

void TestFileMetadataCore::testFormatDiskDetails_gameBase64LoadedAndFound_showsGameInfo()
{
    filemetadata::DiskDisplayContext ctx = makeMinimalDiskContext();
    ctx.gameBase64ServicePresent = true;
    ctx.gameBase64DatabaseLoaded = true;
    ctx.gameInfo = makeGameInfo("Bubble Bobble", 1987);

    QString result = filemetadata::formatDiskDetails(ctx);

    QVERIFY(result.contains("GAMEBASE64 INFO"));
    QVERIFY(result.contains("Bubble Bobble"));
    QVERIFY(result.contains("1987"));
}

void TestFileMetadataCore::testFormatDiskDetails_gameBase64LoadedButNotFound_noGameSection()
{
    filemetadata::DiskDisplayContext ctx = makeMinimalDiskContext();
    ctx.gameBase64ServicePresent = true;
    ctx.gameBase64DatabaseLoaded = true;
    ctx.gameInfo.found = false;

    QString result = filemetadata::formatDiskDetails(ctx);

    QVERIFY(!result.contains("GAMEBASE64 INFO"));
}

void TestFileMetadataCore::testFormatDiskDetails_gameInfo_showsMusicianAndRating()
{
    filemetadata::DiskDisplayContext ctx = makeMinimalDiskContext();
    ctx.gameBase64ServicePresent = true;
    ctx.gameBase64DatabaseLoaded = true;
    ctx.gameInfo = makeGameInfo("Test Game");
    ctx.gameInfo.rating = 9;
    ctx.gameInfo.musician = "Rob Hubbard";
    ctx.gameInfo.playersFrom = 1;
    ctx.gameInfo.playersTo = 2;

    QString result = filemetadata::formatDiskDetails(ctx);

    QVERIFY(result.contains("Rob Hubbard"));
    QVERIFY(result.contains("9/10"));
    QVERIFY(result.contains("1-2"));
}

QTEST_GUILESS_MAIN(TestFileMetadataCore)
#include "test_filemetadatacore.moc"
