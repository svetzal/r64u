#include "services/songlengthsparser.h"

#include <QtTest>

class TestSonglengthsParser : public QObject
{
    Q_OBJECT

private slots:
    // -----------------------------------------------------------------------
    // parseTime
    // -----------------------------------------------------------------------

    void testParseTime_StandardMinutesSeconds() { QCOMPARE(songlengths::parseTime("3:57"), 237); }

    void testParseTime_ZeroMinutes() { QCOMPARE(songlengths::parseTime("0:30"), 30); }

    void testParseTime_TenMinutes() { QCOMPARE(songlengths::parseTime("10:00"), 600); }

    void testParseTime_WithMilliseconds()
    {
        // Milliseconds are ignored; only mm:ss matters
        QCOMPARE(songlengths::parseTime("3:57.500"), 237);
    }

    void testParseTime_WithOneDigitMilliseconds()
    {
        QCOMPARE(songlengths::parseTime("2:23.9"), 143);
    }

    void testParseTime_Invalid_EmptyString() { QCOMPARE(songlengths::parseTime(""), 0); }

    void testParseTime_Invalid_NoColon() { QCOMPARE(songlengths::parseTime("357"), 0); }

    void testParseTime_Invalid_Letters() { QCOMPARE(songlengths::parseTime("abc"), 0); }

    void testParseTime_ZeroZero()
    {
        // 0:00 returns 0 which is excluded from parseTimeList, but parseTime itself returns 0
        QCOMPARE(songlengths::parseTime("0:00"), 0);
    }

    // -----------------------------------------------------------------------
    // parseTimeList
    // -----------------------------------------------------------------------

    void testParseTimeList_ThreeEntries()
    {
        QList<int> result = songlengths::parseTimeList("3:57 2:23 4:01");
        QCOMPARE(result.size(), 3);
        QCOMPARE(result.at(0), 237);
        QCOMPARE(result.at(1), 143);
        QCOMPARE(result.at(2), 241);
    }

    void testParseTimeList_EmptyString()
    {
        QList<int> result = songlengths::parseTimeList("");
        QVERIFY(result.isEmpty());
    }

    void testParseTimeList_SingleEntry()
    {
        QList<int> result = songlengths::parseTimeList("5:30");
        QCOMPARE(result.size(), 1);
        QCOMPARE(result.at(0), 330);
    }

    void testParseTimeList_WithMilliseconds()
    {
        // "3:57.500" should parse as 237 seconds (milliseconds ignored)
        QList<int> result = songlengths::parseTimeList("3:57.500");
        QCOMPARE(result.size(), 1);
        QCOMPARE(result.at(0), 237);
    }

    void testParseTimeList_ExcludesZeroDuration()
    {
        // 0:00 resolves to 0 seconds — should be excluded (parseTime returns 0, > 0 check filters
        // it)
        QList<int> result = songlengths::parseTimeList("0:00 3:57");
        QCOMPARE(result.size(), 1);
        QCOMPARE(result.at(0), 237);
    }

    // -----------------------------------------------------------------------
    // parseDatabase
    // -----------------------------------------------------------------------

    void testParseDatabase_EmptyData()
    {
        QByteArray data;
        auto result = songlengths::parseDatabase(data);
        QVERIFY(result.durations.isEmpty());
        QVERIFY(result.formattedTimes.isEmpty());
        QVERIFY(result.md5ToPath.isEmpty());
    }

    void testParseDatabase_ParsesMd5AndDurations()
    {
        QByteArray data = "; /MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid\n"
                          "a1b2c3d4e5f6789012345678901234ab=3:57 2:23 4:01\n";

        auto result = songlengths::parseDatabase(data);
        QVERIFY(result.durations.contains("a1b2c3d4e5f6789012345678901234ab"));
        auto durs = result.durations.value("a1b2c3d4e5f6789012345678901234ab");
        QCOMPARE(durs.size(), 3);
        QCOMPARE(durs.at(0), 237);
        QCOMPARE(durs.at(1), 143);
        QCOMPARE(durs.at(2), 241);
    }

    void testParseDatabase_StoresMd5LowerCase()
    {
        QByteArray data = "; /MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid\n"
                          "A1B2C3D4E5F6789012345678901234AB=3:57\n";

        auto result = songlengths::parseDatabase(data);
        // Hash must be stored in lowercase
        QVERIFY(result.durations.contains("a1b2c3d4e5f6789012345678901234ab"));
        QVERIFY(!result.durations.contains("A1B2C3D4E5F6789012345678901234AB"));
    }

    void testParseDatabase_ParsesHvscPath()
    {
        QByteArray data = "; /MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid\n"
                          "a1b2c3d4e5f6789012345678901234ab=3:57\n";

        auto result = songlengths::parseDatabase(data);
        QCOMPARE(result.md5ToPath.value("a1b2c3d4e5f6789012345678901234ab"),
                 QString("/MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid"));
    }

    void testParseDatabase_MultipleEntries()
    {
        QByteArray data = "; /MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid\n"
                          "a1b2c3d4e5f6789012345678901234ab=3:57 2:23 4:01\n"
                          "; /MUSICIANS/H/Hubbard_Rob/Delta.sid\n"
                          "b2c3d4e5f6789012345678901234abc1=5:30 3:15\n";

        auto result = songlengths::parseDatabase(data);
        QCOMPARE(result.durations.size(), 2);
        QVERIFY(result.durations.contains("a1b2c3d4e5f6789012345678901234ab"));
        QVERIFY(result.durations.contains("b2c3d4e5f6789012345678901234abc1"));
    }

    void testParseDatabase_SkipsCommentLines()
    {
        QByteArray data = "; This is a comment\n"
                          "; Another comment\n"
                          "a1b2c3d4e5f6789012345678901234ab=3:57\n";

        auto result = songlengths::parseDatabase(data);
        QCOMPARE(result.durations.size(), 1);
    }

    void testParseDatabase_SkipsSectionHeaders()
    {
        QByteArray data = "[Database]\n"
                          "a1b2c3d4e5f6789012345678901234ab=3:57\n";

        auto result = songlengths::parseDatabase(data);
        QCOMPARE(result.durations.size(), 1);
    }

    void testParseDatabase_SkipsEmptyLines()
    {
        QByteArray data = "\n"
                          "; /MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid\n"
                          "\n"
                          "a1b2c3d4e5f6789012345678901234ab=3:57\n"
                          "\n";

        auto result = songlengths::parseDatabase(data);
        QCOMPARE(result.durations.size(), 1);
    }

    void testParseDatabase_StoresFormattedTimes()
    {
        QByteArray data = "; /MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid\n"
                          "a1b2c3d4e5f6789012345678901234ab=3:57 2:23\n";

        auto result = songlengths::parseDatabase(data);
        auto formatted = result.formattedTimes.value("a1b2c3d4e5f6789012345678901234ab");
        QCOMPARE(formatted.size(), 2);
        QCOMPARE(formatted.at(0), QString("3:57"));
        QCOMPARE(formatted.at(1), QString("2:23"));
    }

    void testParseDatabase_FormattedTimesStripsMilliseconds()
    {
        QByteArray data = "; /MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid\n"
                          "a1b2c3d4e5f6789012345678901234ab=3:57.500\n";

        auto result = songlengths::parseDatabase(data);
        auto formatted = result.formattedTimes.value("a1b2c3d4e5f6789012345678901234ab");
        QCOMPARE(formatted.size(), 1);
        QCOMPARE(formatted.at(0), QString("3:57"));
    }

    void testParseDatabase_NoPathForEntryWithoutPrecedingComment()
    {
        // An entry without a preceding path comment should have no md5ToPath mapping
        QByteArray data = "a1b2c3d4e5f6789012345678901234ab=3:57\n";

        auto result = songlengths::parseDatabase(data);
        QCOMPARE(result.durations.size(), 1);
        QVERIFY(!result.md5ToPath.contains("a1b2c3d4e5f6789012345678901234ab"));
    }
};

QTEST_MAIN(TestSonglengthsParser)
#include "test_songlengthsparser.moc"
