/**
 * @file test_c64screencore.cpp
 * @brief Unit tests for the pure C64 screen layout and wrapping helpers.
 */

#include "core/c64screencore.h"

#include <QtTest>

using c64screencore::layoutScreen;
using c64screencore::ScreenGeometry;
using c64screencore::wrapToColumns;

class TestC64ScreenCore : public QObject
{
    Q_OBJECT

private slots:
    // =========================================================================
    // wrapToColumns
    // =========================================================================

    void testWrap_shortLineIsUnchanged()
    {
        QCOMPARE(wrapToColumns("READY.", 40), QStringList{"READY."});
    }

    void testWrap_splitsOnNewlinesAndKeepsEmptyLines()
    {
        QCOMPARE(wrapToColumns("A\n\nB", 40), (QStringList{"A", "", "B"}));
    }

    void testWrap_stripsCarriageReturns()
    {
        QCOMPARE(wrapToColumns("A\r\nB", 40), (QStringList{"A", "B"}));
    }

    void testWrap_breaksAtLastSpaceInWindowAndDropsIt()
    {
        QCOMPARE(wrapToColumns("HELLO WORLD AGAIN", 11), (QStringList{"HELLO WORLD", "AGAIN"}));
        QCOMPARE(wrapToColumns("HELLO WORLD AGAIN", 10), (QStringList{"HELLO", "WORLD", "AGAIN"}));
    }

    void testWrap_hardBreaksWhenWindowHasNoSpace()
    {
        QCOMPARE(wrapToColumns("ABCDEFGHIJ", 4), (QStringList{"ABCD", "EFGH", "IJ"}));
    }

    void testWrap_leadingSpaceDoesNotCountAsBreakPoint()
    {
        QCOMPARE(wrapToColumns(" ABCDEF", 4), (QStringList{" ABC", "DEF"}));
    }

    void testWrap_lineExactlyColumnsWideIsNotBroken()
    {
        const QString line(40, QLatin1Char('X'));
        QCOMPARE(wrapToColumns(line, 40), QStringList{line});
    }

    void testWrap_zeroColumnsOnlySplitsLines()
    {
        QCOMPARE(wrapToColumns("ABC DEF\nGHI", 0), (QStringList{"ABC DEF", "GHI"}));
    }

    void testWrap_emptyTextYieldsOneEmptyLine()
    {
        QCOMPARE(wrapToColumns(QString(), 40), QStringList{QString()});
    }

    // =========================================================================
    // layoutScreen
    // =========================================================================

    void testLayout_oneScaleUnitFitsExactly()
    {
        const ScreenGeometry g = layoutScreen(QSize(384, 264), 40, 25);
        QCOMPARE(g.scale, 1);
        QCOMPARE(g.border, QRect(0, 0, 384, 264));
        QCOMPARE(g.screen, QRect(32, 32, 320, 200));
    }

    void testLayout_scaleNeverBelowOne()
    {
        const ScreenGeometry g = layoutScreen(QSize(100, 100), 40, 25);
        QCOMPARE(g.scale, 1);
        QCOMPARE(g.border.size(), QSize(384, 264));
    }

    void testLayout_usesLargestIntegerScaleThatFits()
    {
        const ScreenGeometry g = layoutScreen(QSize(1000, 800), 40, 25);
        QCOMPARE(g.scale, 2);
        QCOMPARE(g.border.size(), QSize(768, 528));
        QCOMPARE(g.screen.size(), QSize(640, 400));
    }

    void testLayout_isLimitedByTheSmallerAxis()
    {
        const ScreenGeometry g = layoutScreen(QSize(2000, 300), 40, 25);
        QCOMPARE(g.scale, 1);
    }

    void testLayout_centresBorderAndScreen()
    {
        const ScreenGeometry g = layoutScreen(QSize(800, 600), 40, 25);
        QCOMPARE(g.scale, 2);
        QCOMPARE(g.border, QRect((800 - 768) / 2, (600 - 528) / 2, 768, 528));
        QCOMPARE(g.screen.topLeft(), g.border.topLeft() + QPoint(64, 64));
    }

    void testLayout_customCellAndBorder()
    {
        const ScreenGeometry g = layoutScreen(QSize(200, 200), 10, 10, 4, 1);
        // unit = (10 + 2) * 4 = 48 → scale 4 → 192
        QCOMPARE(g.scale, 4);
        QCOMPARE(g.border, QRect(4, 4, 192, 192));
        QCOMPARE(g.screen, QRect(20, 20, 160, 160));
    }

    void testLayout_degenerateColumnsGivesEmptyRects()
    {
        const ScreenGeometry g = layoutScreen(QSize(200, 200), 0, 0, 8, 0);
        QCOMPARE(g.scale, 1);
        QVERIFY(g.border.isNull());
        QVERIFY(g.screen.isNull());
    }
};

QTEST_MAIN(TestC64ScreenCore)
#include "test_c64screencore.moc"
