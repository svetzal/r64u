/**
 * @file test_favoritescore.cpp
 * @brief Unit tests for the favorites pure core functions.
 *
 * Tests cover all pure functions in namespace favorites without any I/O
 * or QSettings access.
 */

#include "services/favoritescore.h"

#include <QtTest>

class TestFavoritesCore : public QObject
{
    Q_OBJECT

private slots:

    // -----------------------------------------------------------------------
    // sorted
    // -----------------------------------------------------------------------

    void testSorted_returnsAlphabeticallySortedList()
    {
        QStringList paths = {"/Z/file.sid", "/A/file.sid", "/m/file.sid"};
        auto result = favorites::sorted(paths);
        QCOMPARE(result[0], QString("/A/file.sid"));
        QCOMPARE(result[1], QString("/m/file.sid"));
        QCOMPARE(result[2], QString("/Z/file.sid"));
    }

    void testSorted_caseInsensitive()
    {
        QStringList paths = {"/beta/file.sid", "/Alpha/file.sid"};
        auto result = favorites::sorted(paths);
        QCOMPARE(result[0], QString("/Alpha/file.sid"));
        QCOMPARE(result[1], QString("/beta/file.sid"));
    }

    void testSorted_emptyList_returnsEmpty() { QVERIFY(favorites::sorted({}).isEmpty()); }

    void testSorted_doesNotModifyOriginal()
    {
        QStringList paths = {"/Z/file.sid", "/A/file.sid"};
        auto result = favorites::sorted(paths);
        QCOMPARE(paths[0], QString("/Z/file.sid"));  // original unchanged
    }

    // -----------------------------------------------------------------------
    // addFavorite
    // -----------------------------------------------------------------------

    void testAddFavorite_addsToPaths()
    {
        QStringList paths;
        auto [result, added] = favorites::addFavorite(paths, "/C64/Track.sid");
        QCOMPARE(result.size(), 1);
        QCOMPARE(result[0], QString("/C64/Track.sid"));
        QVERIFY(added);
    }

    void testAddFavorite_doesNotAddDuplicate()
    {
        QStringList paths = {"/C64/Track.sid"};
        auto [result, added] = favorites::addFavorite(paths, "/C64/Track.sid");
        QCOMPARE(result.size(), 1);
        QVERIFY(!added);
    }

    void testAddFavorite_doesNotAddEmptyPath()
    {
        QStringList paths;
        auto [result, added] = favorites::addFavorite(paths, "");
        QVERIFY(result.isEmpty());
        QVERIFY(!added);
    }

    void testAddFavorite_appendsInOrder()
    {
        QStringList paths = {"/first.sid"};
        auto [result, added] = favorites::addFavorite(paths, "/second.sid");
        QCOMPARE(result.size(), 2);
        QCOMPARE(result[0], QString("/first.sid"));
        QCOMPARE(result[1], QString("/second.sid"));
    }

    // -----------------------------------------------------------------------
    // removeFavorite
    // -----------------------------------------------------------------------

    void testRemoveFavorite_removesExistingPath()
    {
        QStringList paths = {"/A.sid", "/B.sid", "/C.sid"};
        auto [result, removed] = favorites::removeFavorite(paths, "/B.sid");
        QCOMPARE(result.size(), 2);
        QVERIFY(!result.contains("/B.sid"));
        QVERIFY(removed);
    }

    void testRemoveFavorite_nonExistentPath_noChange()
    {
        QStringList paths = {"/A.sid"};
        auto [result, removed] = favorites::removeFavorite(paths, "/Z.sid");
        QCOMPARE(result.size(), 1);
        QVERIFY(!removed);
    }

    void testRemoveFavorite_emptyList_noChange()
    {
        QStringList paths;
        auto [result, removed] = favorites::removeFavorite(paths, "/A.sid");
        QVERIFY(result.isEmpty());
        QVERIFY(!removed);
    }

    // -----------------------------------------------------------------------
    // moveFavorite
    // -----------------------------------------------------------------------

    void testMoveFavorite_movesForward()
    {
        QStringList paths = {"/A.sid", "/B.sid", "/C.sid"};
        auto result = favorites::moveFavorite(paths, 0, 2);
        QCOMPARE(result[0], QString("/B.sid"));
        QCOMPARE(result[1], QString("/C.sid"));
        QCOMPARE(result[2], QString("/A.sid"));
    }

    void testMoveFavorite_movesBackward()
    {
        QStringList paths = {"/A.sid", "/B.sid", "/C.sid"};
        auto result = favorites::moveFavorite(paths, 2, 0);
        QCOMPARE(result[0], QString("/C.sid"));
        QCOMPARE(result[1], QString("/A.sid"));
        QCOMPARE(result[2], QString("/B.sid"));
    }

    void testMoveFavorite_samePosition_noChange()
    {
        QStringList paths = {"/A.sid", "/B.sid"};
        auto result = favorites::moveFavorite(paths, 1, 1);
        QCOMPARE(result, paths);
    }

    void testMoveFavorite_invalidFromIndex_noChange()
    {
        QStringList paths = {"/A.sid"};
        auto result = favorites::moveFavorite(paths, -1, 0);
        QCOMPARE(result, paths);
    }

    void testMoveFavorite_invalidToIndex_noChange()
    {
        QStringList paths = {"/A.sid"};
        auto result = favorites::moveFavorite(paths, 0, 5);
        QCOMPARE(result, paths);
    }

    void testMoveFavorite_emptyList_noChange()
    {
        QStringList paths;
        auto result = favorites::moveFavorite(paths, 0, 0);
        QVERIFY(result.isEmpty());
    }
};

QTEST_MAIN(TestFavoritesCore)
#include "test_favoritescore.moc"
