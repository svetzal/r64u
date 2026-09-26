/**
 * @file test_pixeliconcore.cpp
 * @brief Unit tests for the pixel icon data in core/pixeliconcore.h.
 *
 * Tests verify:
 * - Every icon has 16 rows of exactly 16 characters, all from the palette
 * - Every icon carries the black outline and uses at most five colours
 * - AllIcons lists each Icon value once
 * - The palette lookup matches the VIC-II Pepto colours in visual.md
 */

#include "core/pixeliconcore.h"

#include <QtTest>

#include <cstring>
#include <set>

using pixeliconcore::AllIcons;
using pixeliconcore::Icon;
using pixeliconcore::Size;

class TestPixelIconCore : public QObject
{
    Q_OBJECT

private:
    static QByteArray describe(Icon icon, int row)
    {
        return QByteArray("icon ") + QByteArray::number(static_cast<int>(icon)) + " row " +
               QByteArray::number(row) + ": " + pixeliconcore::rows(icon)[row];
    }

private slots:

    void testRows_everyIcon_hasSixteenRowsOfSixteenPaletteChars()
    {
        for (Icon icon : AllIcons) {
            const pixeliconcore::Rows &rows = pixeliconcore::rows(icon);
            QCOMPARE(static_cast<int>(rows.size()), Size);
            for (int y = 0; y < Size; ++y) {
                QVERIFY2(std::strlen(rows[y]) == static_cast<std::size_t>(Size),
                         describe(icon, y).constData());
                for (int x = 0; x < Size; ++x) {
                    QVERIFY2(pixeliconcore::isPaletteChar(rows[y][x]),
                             describe(icon, y).constData());
                }
            }
        }
    }

    void testRows_everyIcon_hasAnOutlineAndAtMostFiveColours()
    {
        for (Icon icon : AllIcons) {
            std::set<char> colours;
            for (const char *row : pixeliconcore::rows(icon)) {
                for (int x = 0; x < Size; ++x) {
                    if (row[x] != pixeliconcore::Transparent) {
                        colours.insert(row[x]);
                    }
                }
            }
            QVERIFY2(colours.count(pixeliconcore::Outline) == 1, describe(icon, 0).constData());
            QVERIFY2(colours.size() <= 5, describe(icon, 0).constData());
        }
    }

    void testRows_everyIcon_leavesBreathingRoomOnAtLeastOneAxis()
    {
        // A shape may touch two opposite edges only if it has room on the other axis;
        // this catches icons that fill the whole grid.
        for (Icon icon : AllIcons) {
            const pixeliconcore::Rows &rows = pixeliconcore::rows(icon);
            bool topOrBottomClear = true;
            for (int x = 0; x < Size; ++x) {
                if (rows[0][x] != '.' || rows[Size - 1][x] != '.') {
                    topOrBottomClear = false;
                }
            }
            bool leftOrRightClear = true;
            for (int y = 0; y < Size; ++y) {
                if (rows[y][0] != '.' || rows[y][Size - 1] != '.') {
                    leftOrRightClear = false;
                }
            }
            QVERIFY2(topOrBottomClear || leftOrRightClear, describe(icon, 0).constData());
        }
    }

    void testAllIcons_listsEachIconOnce()
    {
        std::set<int> seen;
        for (Icon icon : AllIcons) {
            QVERIFY(seen.insert(static_cast<int>(icon)).second);
        }
        QCOMPARE(seen.size(), AllIcons.size());
    }

    void testColourFor_paletteMatchesVisualSpec()
    {
        const auto black = pixeliconcore::colourFor('k');
        QVERIFY(black.has_value());
        QCOMPARE(black->r, 0x00);
        QCOMPARE(black->g, 0x00);
        QCOMPARE(black->b, 0x00);

        const auto cyan = pixeliconcore::colourFor('c');
        QVERIFY(cyan.has_value());
        QCOMPARE(cyan->r, 0x6A);
        QCOMPARE(cyan->g, 0xBF);
        QCOMPARE(cyan->b, 0xC6);

        const auto yellow = pixeliconcore::colourFor('y');
        QVERIFY(yellow.has_value());
        QCOMPARE(yellow->r, 0xC9);
        QCOMPARE(yellow->g, 0xD4);
        QCOMPARE(yellow->b, 0x87);
    }

    void testColourFor_transparentAndUnknown_haveNoColour()
    {
        QVERIFY(!pixeliconcore::colourFor('.').has_value());
        QVERIFY(!pixeliconcore::colourFor('z').has_value());
        QVERIFY(pixeliconcore::isPaletteChar('.'));
        QVERIFY(!pixeliconcore::isPaletteChar('z'));
    }

    void testRows_pairedIcons_shareTheirTray()
    {
        // Download and Upload differ only in the arrow; the tray rows are identical.
        const pixeliconcore::Rows &download = pixeliconcore::rows(Icon::Download);
        const pixeliconcore::Rows &upload = pixeliconcore::rows(Icon::Upload);
        for (int y = 11; y < 15; ++y) {
            QCOMPARE(QByteArray(download[y]), QByteArray(upload[y]));
        }
    }
};

QTEST_APPLESS_MAIN(TestPixelIconCore)
#include "test_pixeliconcore.moc"
