/**
 * @file test_ftpresponseparser.cpp
 * @brief Tests for pure FTP response parsing functions in ftpcore.
 */

#include "services/ftpcore.h"

#include <QtTest/QtTest>

class TestFtpResponseParser : public QObject
{
    Q_OBJECT

private slots:
    // --- parsePwdResponse ---

    void parsePwdResponse_validPath_extractsPath()
    {
        auto result = ftp::parsePwdResponse("\"/path/to/dir\" is current directory");
        QVERIFY(result.has_value());
        QCOMPARE(*result, QString("/path/to/dir"));
    }

    void parsePwdResponse_rootPath_extractsSlash()
    {
        auto result = ftp::parsePwdResponse("\"/\" is current directory");
        QVERIFY(result.has_value());
        QCOMPARE(*result, QString("/"));
    }

    void parsePwdResponse_noQuotes_returnsNullopt()
    {
        auto result = ftp::parsePwdResponse("257 no quotes here");
        QVERIFY(!result.has_value());
    }

    void parsePwdResponse_emptyString_returnsNullopt()
    {
        auto result = ftp::parsePwdResponse("");
        QVERIFY(!result.has_value());
    }

    void parsePwdResponse_emptyQuotedPath_extractsEmpty()
    {
        auto result = ftp::parsePwdResponse("\"\" is current directory");
        QVERIFY(result.has_value());
        QCOMPARE(*result, QString(""));
    }

    // --- splitResponseLines ---

    void splitResponseLines_singleCompleteLine_returnsOneLine()
    {
        auto result = ftp::splitResponseLines("220 Ready\r\n");
        QCOMPARE(result.lines.size(), 1);
        QCOMPARE(result.lines[0].code, 220);
        QCOMPARE(result.lines[0].text, QString("Ready"));
        QCOMPARE(result.remainingBuffer, QString(""));
    }

    void splitResponseLines_multiLineContinuation_skipsAndReturnsTerminalLine()
    {
        // 220-Hello is a continuation line; 220 Done is the terminal line
        auto result = ftp::splitResponseLines("220-Hello\r\n220 Done\r\n");
        QCOMPARE(result.lines.size(), 1);
        QCOMPARE(result.lines[0].code, 220);
        QCOMPARE(result.lines[0].text, QString("Done"));
        QCOMPARE(result.remainingBuffer, QString(""));
    }

    void splitResponseLines_partialLine_returnsNoLinesAndRemainder()
    {
        auto result = ftp::splitResponseLines("220 Partial");
        QCOMPARE(result.lines.size(), 0);
        QCOMPARE(result.remainingBuffer, QString("220 Partial"));
    }

    void splitResponseLines_emptyString_returnsNoLinesAndEmptyRemainder()
    {
        auto result = ftp::splitResponseLines("");
        QCOMPARE(result.lines.size(), 0);
        QCOMPARE(result.remainingBuffer, QString(""));
    }

    void splitResponseLines_twoCompleteLines_returnsTwoLines()
    {
        auto result = ftp::splitResponseLines("220 Ready\r\n331 Password required\r\n");
        QCOMPARE(result.lines.size(), 2);
        QCOMPARE(result.lines[0].code, 220);
        QCOMPARE(result.lines[0].text, QString("Ready"));
        QCOMPARE(result.lines[1].code, 331);
        QCOMPARE(result.lines[1].text, QString("Password required"));
        QCOMPARE(result.remainingBuffer, QString(""));
    }

    void splitResponseLines_completeLineFollowedByPartial_returnsOneLineWithRemainder()
    {
        auto result = ftp::splitResponseLines("220 Ready\r\n331 Pass");
        QCOMPARE(result.lines.size(), 1);
        QCOMPARE(result.lines[0].code, 220);
        QCOMPARE(result.remainingBuffer, QString("331 Pass"));
    }

    void splitResponseLines_onlyCrLf_returnsNoLines()
    {
        // A bare \r\n with no code is too short to parse
        auto result = ftp::splitResponseLines("\r\n");
        QCOMPARE(result.lines.size(), 0);
        QCOMPARE(result.remainingBuffer, QString(""));
    }
};

QTEST_MAIN(TestFtpResponseParser)
#include "test_ftpresponseparser.moc"
