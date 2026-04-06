/**
 * @file test_favoritesuicore.cpp
 * @brief Unit tests for the favorites UI pure core functions.
 *
 * Tests cover all pure functions in namespace favoritesui without any I/O
 * or Qt widget dependencies.
 */

#include "services/favoritesuicore.h"

#include <QtTest>

class TestFavoritesUICore : public QObject
{
    Q_OBJECT

private slots:

    // -----------------------------------------------------------------------
    // displayNameForPath
    // -----------------------------------------------------------------------

    void testDisplayNameForPath_typicalPath_returnsLastComponent()
    {
        QCOMPARE(favoritesui::displayNameForPath("/path/to/file.sid"), QString("file.sid"));
    }

    void testDisplayNameForPath_trailingSlash_returnsSlash()
    {
        // Path ends in '/' — last component is empty, return "/"
        QCOMPARE(favoritesui::displayNameForPath("/folder/"), QString("/"));
    }

    void testDisplayNameForPath_rootPath_returnsSlash()
    {
        QCOMPARE(favoritesui::displayNameForPath("/"), QString("/"));
    }

    void testDisplayNameForPath_noSlash_returnsPathAsIs()
    {
        QCOMPARE(favoritesui::displayNameForPath("file.sid"), QString("file.sid"));
    }

    void testDisplayNameForPath_emptyPath_returnsSlash()
    {
        // Empty string: no slash found, mid(0+1) on empty gives empty → return "/"
        QCOMPARE(favoritesui::displayNameForPath(""), QString("/"));
    }

    // -----------------------------------------------------------------------
    // buildMenuEntries
    // -----------------------------------------------------------------------

    void testBuildMenuEntries_emptyFavorites_returnsPlaceholder()
    {
        auto entries = favoritesui::buildMenuEntries({});
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].displayName, QString("(No favorites)"));
        QCOMPARE(entries[0].path, QString());
    }

    void testBuildMenuEntries_singleEntry_returnsCorrectEntry()
    {
        auto entries = favoritesui::buildMenuEntries({"/C64/Music/track.sid"});
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].displayName, QString("track.sid"));
        QCOMPARE(entries[0].path, QString("/C64/Music/track.sid"));
    }

    void testBuildMenuEntries_multipleEntries_returnsCorrectDisplayNames()
    {
        QStringList favorites = {"/", "/C64/Games"};
        auto entries = favoritesui::buildMenuEntries(favorites);
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries[0].displayName, QString("/"));
        QCOMPARE(entries[0].path, QString("/"));
        QCOMPARE(entries[1].displayName, QString("Games"));
        QCOMPARE(entries[1].path, QString("/C64/Games"));
    }

    // -----------------------------------------------------------------------
    // isFavorite
    // -----------------------------------------------------------------------

    void testIsFavorite_pathInList_returnsTrue()
    {
        QVERIFY(favoritesui::isFavorite("/path", {"/path", "/other"}));
    }

    void testIsFavorite_pathNotInList_returnsFalse()
    {
        QVERIFY(!favoritesui::isFavorite("/missing", {"/path"}));
    }

    void testIsFavorite_emptyList_returnsFalse() { QVERIFY(!favoritesui::isFavorite("/path", {})); }
};

QTEST_MAIN(TestFavoritesUICore)
#include "test_favoritesuicore.moc"
