#include "services/hvscparser.h"

#include <QtTest>

class TestHVSCParser : public QObject
{
    Q_OBJECT

private slots:
    // -----------------------------------------------------------------------
    // parseStilData
    // -----------------------------------------------------------------------

    void testParseStilData_EmptyData()
    {
        QByteArray data;
        auto db = hvsc::parseStilData(data);
        QVERIFY(db.isEmpty());
    }

    void testParseStilData_SingleEntry()
    {
        QByteArray data = "/MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid\n"
                          "COMMENT: Classic Cybernoid theme\n";

        auto db = hvsc::parseStilData(data);
        QCOMPARE(db.size(), 1);
        QVERIFY(db.contains("/MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid"));
    }

    void testParseStilData_MultipleEntries()
    {
        QByteArray data = "/MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid\n"
                          "COMMENT: Classic Cybernoid theme\n"
                          "\n"
                          "/MUSICIANS/H/Hubbard_Rob/Delta.sid\n"
                          "COMMENT: Delta tune\n";

        auto db = hvsc::parseStilData(data);
        QCOMPARE(db.size(), 2);
        QVERIFY(db.contains("/MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid"));
        QVERIFY(db.contains("/MUSICIANS/H/Hubbard_Rob/Delta.sid"));
    }

    void testParseStilData_SkipsSectionHeaders()
    {
        QByteArray data = "### /MUSICIANS/T/Tel_Jeroen ########################################\n"
                          "/MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid\n"
                          "COMMENT: Classic Cybernoid theme\n";

        auto db = hvsc::parseStilData(data);
        QCOMPARE(db.size(), 1);
        QVERIFY(db.contains("/MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid"));
    }

    void testParseStilData_EntryWithName()
    {
        QByteArray data = "/MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid\n"
                          "   NAME: Cybernoid II - The Revenge\n";

        auto db = hvsc::parseStilData(data);
        QVERIFY(!db.isEmpty());
        auto entries = db.value("/MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid");
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.first().name, QString("Cybernoid II - The Revenge"));
    }

    void testParseStilData_EntryWithAuthor()
    {
        // An entry needs at least name, comment, or covers to be saved.
        // Pair AUTHOR with NAME so the entry is persisted.
        QByteArray data = "/MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid\n"
                          "   NAME: Cybernoid II\n"
                          " AUTHOR: Jeroen Tel\n";

        auto db = hvsc::parseStilData(data);
        auto entries = db.value("/MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid");
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.first().author, QString("Jeroen Tel"));
    }

    void testParseStilData_EntryWithComment()
    {
        QByteArray data = "/MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid\n"
                          "COMMENT: Classic Cybernoid theme\n";

        auto db = hvsc::parseStilData(data);
        auto entries = db.value("/MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid");
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.first().comment, QString("Classic Cybernoid theme"));
    }

    void testParseStilData_EntryWithCoverTitle()
    {
        QByteArray data = "/MUSICIANS/G/Galway_Martin/Arkanoid.sid\n"
                          "  TITLE: Revenge from Mars\n";

        auto db = hvsc::parseStilData(data);
        auto entries = db.value("/MUSICIANS/G/Galway_Martin/Arkanoid.sid");
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.first().covers.size(), 1);
        QCOMPARE(entries.first().covers.first().title, QString("Revenge from Mars"));
    }

    void testParseStilData_CoverWithArtist()
    {
        QByteArray data = "/MUSICIANS/G/Galway_Martin/Arkanoid.sid\n"
                          "  TITLE: Revenge from Mars\n"
                          " ARTIST: Unknown Composer\n";

        auto db = hvsc::parseStilData(data);
        auto entries = db.value("/MUSICIANS/G/Galway_Martin/Arkanoid.sid");
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.first().covers.size(), 1);
        QCOMPARE(entries.first().covers.first().artist, QString("Unknown Composer"));
    }

    void testParseStilData_CoverWithTimestamp()
    {
        QByteArray data = "/MUSICIANS/G/Galway_Martin/Arkanoid.sid\n"
                          "  TITLE: Revenge from Mars (0:15-0:45)\n";

        auto db = hvsc::parseStilData(data);
        auto entries = db.value("/MUSICIANS/G/Galway_Martin/Arkanoid.sid");
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.first().covers.size(), 1);
        QCOMPARE(entries.first().covers.first().title, QString("Revenge from Mars"));
        QCOMPARE(entries.first().covers.first().timestamp, QString("0:15-0:45"));
    }

    void testParseStilData_SubtuneEntries()
    {
        QByteArray data = "/MUSICIANS/H/Hubbard_Rob/Delta.sid\n"
                          "(#1)\n"
                          "   NAME: Delta Main Theme\n"
                          "COMMENT: Opening tune\n"
                          "(#2)\n"
                          "   NAME: Delta Level 2\n";

        auto db = hvsc::parseStilData(data);
        auto entries = db.value("/MUSICIANS/H/Hubbard_Rob/Delta.sid");
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries.at(0).subtune, 1);
        QCOMPARE(entries.at(0).name, QString("Delta Main Theme"));
        QCOMPARE(entries.at(0).comment, QString("Opening tune"));
        QCOMPARE(entries.at(1).subtune, 2);
        QCOMPARE(entries.at(1).name, QString("Delta Level 2"));
    }

    void testParseStilData_ContinuationLines()
    {
        QByteArray data = "/MUSICIANS/H/Hubbard_Rob/Commando.sid\n"
                          "COMMENT: One of Rob Hubbard's most iconic compositions.\n"
                          "         The loading tune set the standard for C64 music.\n";

        auto db = hvsc::parseStilData(data);
        auto entries = db.value("/MUSICIANS/H/Hubbard_Rob/Commando.sid");
        QCOMPARE(entries.size(), 1);
        QVERIFY(entries.first().comment.contains("One of Rob Hubbard"));
        QVERIFY(entries.first().comment.contains("The loading tune"));
    }

    // -----------------------------------------------------------------------
    // parseBuglistData
    // -----------------------------------------------------------------------

    void testParseBuglistData_EmptyData()
    {
        QByteArray data;
        auto db = hvsc::parseBuglistData(data);
        QVERIFY(db.isEmpty());
    }

    void testParseBuglistData_SingleEntry()
    {
        QByteArray data = "/MUSICIANS/T/Tel_Jeroen/Bugged_Tune.sid\n"
                          "BUG: The tune ends abruptly due to missing loop code.\n";

        auto db = hvsc::parseBuglistData(data);
        QCOMPARE(db.size(), 1);
        QVERIFY(db.contains("/MUSICIANS/T/Tel_Jeroen/Bugged_Tune.sid"));
        auto entries = db.value("/MUSICIANS/T/Tel_Jeroen/Bugged_Tune.sid");
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.first().subtune, 0);
        QVERIFY(entries.first().description.contains("ends abruptly"));
    }

    void testParseBuglistData_BugWithSubtune()
    {
        QByteArray data = "/MUSICIANS/T/Tel_Jeroen/Another_Bug.sid\n"
                          "(#2)\n"
                          "BUG: Subtune 2 has incorrect tempo on NTSC systems.\n"
                          "(#3)\n"
                          "BUG: Plays noise at the end instead of silence.\n";

        auto db = hvsc::parseBuglistData(data);
        auto entries = db.value("/MUSICIANS/T/Tel_Jeroen/Another_Bug.sid");
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries.at(0).subtune, 2);
        QVERIFY(entries.at(0).description.contains("incorrect tempo"));
        QCOMPARE(entries.at(1).subtune, 3);
        QVERIFY(entries.at(1).description.contains("Plays noise"));
    }

    void testParseBuglistData_BugContinuation()
    {
        QByteArray data = "/DEMOS/A/Alpha/Demo.sid\n"
                          "BUG: Requires specific VIC timing that emulators may not\n"
                          "     replicate accurately.\n";

        auto db = hvsc::parseBuglistData(data);
        auto entries = db.value("/DEMOS/A/Alpha/Demo.sid");
        QCOMPARE(entries.size(), 1);
        QVERIFY(entries.first().description.contains("specific VIC timing"));
        QVERIFY(entries.first().description.contains("replicate accurately"));
    }

    // -----------------------------------------------------------------------
    // lookupStil
    // -----------------------------------------------------------------------

    void testLookupStil_Found()
    {
        QHash<QString, QList<hvsc::SubtuneEntry>> db;
        hvsc::SubtuneEntry entry;
        entry.comment = "Classic theme";
        db.insert("/MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid", {entry});

        auto result = hvsc::lookupStil(db, "/MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid");
        QVERIFY(result.found);
        QCOMPARE(result.path, QString("/MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid"));
        QCOMPARE(result.entries.size(), 1);
        QCOMPARE(result.entries.first().comment, QString("Classic theme"));
    }

    void testLookupStil_NotFound()
    {
        QHash<QString, QList<hvsc::SubtuneEntry>> db;
        auto result = hvsc::lookupStil(db, "/NON/EXISTENT/file.sid");
        QVERIFY(!result.found);
        QVERIFY(result.path.isEmpty());
        QVERIFY(result.entries.isEmpty());
    }

    void testLookupStil_NormalizesBackslashes()
    {
        QHash<QString, QList<hvsc::SubtuneEntry>> db;
        hvsc::SubtuneEntry entry;
        entry.comment = "Test";
        db.insert("/MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid", {entry});

        auto result = hvsc::lookupStil(db, "\\MUSICIANS\\T\\Tel_Jeroen\\Cybernoid_II.sid");
        QVERIFY(result.found);
    }

    void testLookupStil_AddsLeadingSlash()
    {
        QHash<QString, QList<hvsc::SubtuneEntry>> db;
        hvsc::SubtuneEntry entry;
        entry.comment = "Test";
        db.insert("/MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid", {entry});

        auto result = hvsc::lookupStil(db, "MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid");
        QVERIFY(result.found);
    }

    // -----------------------------------------------------------------------
    // lookupBuglist
    // -----------------------------------------------------------------------

    void testLookupBuglist_Found()
    {
        QHash<QString, QList<hvsc::BugEntry>> db;
        hvsc::BugEntry bug;
        bug.description = "Crashes on NTSC";
        db.insert("/MUSICIANS/T/Tel_Jeroen/Bugged_Tune.sid", {bug});

        auto result = hvsc::lookupBuglist(db, "/MUSICIANS/T/Tel_Jeroen/Bugged_Tune.sid");
        QVERIFY(result.found);
        QCOMPARE(result.path, QString("/MUSICIANS/T/Tel_Jeroen/Bugged_Tune.sid"));
        QCOMPARE(result.entries.size(), 1);
        QCOMPARE(result.entries.first().description, QString("Crashes on NTSC"));
    }

    void testLookupBuglist_NotFound()
    {
        QHash<QString, QList<hvsc::BugEntry>> db;
        auto result = hvsc::lookupBuglist(db, "/NON/EXISTENT/file.sid");
        QVERIFY(!result.found);
        QVERIFY(result.path.isEmpty());
        QVERIFY(result.entries.isEmpty());
    }
};

QTEST_MAIN(TestHVSCParser)
#include "test_hvscparser.moc"
