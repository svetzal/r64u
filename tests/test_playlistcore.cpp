/**
 * @file test_playlistcore.cpp
 * @brief Unit tests for the playlist pure core functions.
 *
 * Tests cover all pure functions in namespace playlist without any I/O,
 * Qt signals, timers, or network access.
 */

#include "services/playlistcore.h"

#include <QtTest>

class TestPlaylistCore : public QObject
{
    Q_OBJECT

private:
    // Helper: build a state with N items (paths "/track/1.sid" etc.)
    static playlist::State makeState(int count, int currentIndex = -1, bool shuffle = false,
                                     playlist::RepeatMode repeatMode = playlist::RepeatMode::Off,
                                     int defaultDuration = 180)
    {
        playlist::State s;
        s.currentIndex = currentIndex;
        s.shuffle = shuffle;
        s.repeatMode = repeatMode;
        s.defaultDuration = defaultDuration;
        for (int i = 0; i < count; ++i) {
            playlist::PlaylistItem item;
            item.path = QString("/track/%1.sid").arg(i + 1);
            item.title = QString("Track %1").arg(i + 1);
            item.subsong = 1;
            item.durationSecs = 120;
            s.items.append(item);
        }
        if (shuffle && count > 0) {
            s.shuffleOrder = playlist::generateShuffleOrder(count, 42);
        }
        return s;
    }

private slots:

    // -----------------------------------------------------------------------
    // createItem
    // -----------------------------------------------------------------------

    void testCreateItem_extractsTitleFromFilename()
    {
        auto item = playlist::createItem("/C64/Music/Rob Hubbard - Commando.sid", 1, 180);
        QCOMPARE(item.title, QString("Rob Hubbard - Commando"));
    }

    void testCreateItem_setsSubsongAndDuration()
    {
        auto item = playlist::createItem("/some/path/track.sid", 3, 240);
        QCOMPARE(item.subsong, 3);
        QCOMPARE(item.durationSecs, 240);
        QCOMPARE(item.path, QString("/some/path/track.sid"));
    }

    void testCreateItem_defaultSubsong()
    {
        auto item = playlist::createItem("/path/file.sid", 1, 180);
        QCOMPARE(item.subsong, 1);
    }

    // -----------------------------------------------------------------------
    // addItem
    // -----------------------------------------------------------------------

    void testAddItem_appendsToEmptyList()
    {
        playlist::State s;
        playlist::PlaylistItem item;
        item.path = "/test.sid";
        item.title = "Test";

        auto result = playlist::addItem(s, item);
        QCOMPARE(result.items.size(), 1);
        QCOMPARE(result.items[0].path, QString("/test.sid"));
    }

    void testAddItem_appendsToExistingList()
    {
        auto s = makeState(3);
        playlist::PlaylistItem item;
        item.path = "/new.sid";

        auto result = playlist::addItem(s, item);
        QCOMPARE(result.items.size(), 4);
        QCOMPARE(result.items.last().path, QString("/new.sid"));
    }

    void testAddItem_preservesOtherStateFields()
    {
        playlist::State s;
        s.currentIndex = -1;
        s.shuffle = false;
        s.repeatMode = playlist::RepeatMode::All;
        s.defaultDuration = 300;

        playlist::PlaylistItem item;
        item.path = "/test.sid";
        auto result = playlist::addItem(s, item);

        QCOMPARE(result.repeatMode, playlist::RepeatMode::All);
        QCOMPARE(result.defaultDuration, 300);
    }

    void testAddItem_regeneratesShuffleOrderWhenShuffleEnabled()
    {
        auto s = makeState(3, -1, true);
        playlist::PlaylistItem item;
        item.path = "/new.sid";

        auto result = playlist::addItem(s, item, quint32(99));
        QCOMPARE(result.shuffleOrder.size(), 4);
        // Shuffle order must contain all indices exactly once
        QList<int> sorted = result.shuffleOrder;
        std::sort(sorted.begin(), sorted.end());
        QCOMPARE(sorted, QList<int>({0, 1, 2, 3}));
    }

    void testAddItem_noShuffleOrderWhenShuffleDisabled()
    {
        auto s = makeState(3, -1, false);
        playlist::PlaylistItem item;
        item.path = "/new.sid";

        auto result = playlist::addItem(s, item);
        QVERIFY(result.shuffleOrder.isEmpty());
    }

    // -----------------------------------------------------------------------
    // removeItem
    // -----------------------------------------------------------------------

    void testRemoveItem_removesCorrectItem()
    {
        auto s = makeState(3);
        auto [newState, wasPlaying] = playlist::removeItem(s, 1);
        QCOMPARE(newState.items.size(), 2);
        QCOMPARE(newState.items[0].path, QString("/track/1.sid"));
        QCOMPARE(newState.items[1].path, QString("/track/3.sid"));
    }

    void testRemoveItem_invalidIndexDoesNothing()
    {
        auto s = makeState(3, 1);
        auto [newState, wasPlaying] = playlist::removeItem(s, -1);
        QCOMPARE(newState.items.size(), 3);

        auto [newState2, wasPlaying2] = playlist::removeItem(s, 5);
        QCOMPARE(newState2.items.size(), 3);
    }

    void testRemoveItem_currentItemRemovedReturnsWasPlaying()
    {
        auto s = makeState(3, 1);
        auto [newState, wasPlaying] = playlist::removeItem(s, 1);
        QVERIFY(wasPlaying);
    }

    void testRemoveItem_nonCurrentItemRemovedDoesNotSetWasPlaying()
    {
        auto s = makeState(3, 0);
        auto [newState, wasPlaying] = playlist::removeItem(s, 2);
        QVERIFY(!wasPlaying);
    }

    void testRemoveItem_beforeCurrentIndexDecrementsCurrentIndex()
    {
        auto s = makeState(3, 2);
        auto [newState, wasPlaying] = playlist::removeItem(s, 0);
        QCOMPARE(newState.currentIndex, 1);
    }

    void testRemoveItem_afterCurrentIndexPreservesCurrentIndex()
    {
        auto s = makeState(3, 0);
        auto [newState, wasPlaying] = playlist::removeItem(s, 2);
        QCOMPARE(newState.currentIndex, 0);
    }

    void testRemoveItem_currentIndexAtEndClampsToLastItem()
    {
        auto s = makeState(3, 2);
        // Remove item at index 2 (the last / current), should clamp to new last
        auto [newState, wasPlaying] = playlist::removeItem(s, 2);
        QCOMPARE(newState.items.size(), 2);
        QCOMPARE(newState.currentIndex, 1);
    }

    void testRemoveItem_lastItemMakesCurrentIndexMinusOne()
    {
        auto s = makeState(1, 0);
        auto [newState, wasPlaying] = playlist::removeItem(s, 0);
        QCOMPARE(newState.items.size(), 0);
        QCOMPARE(newState.currentIndex, -1);
    }

    // -----------------------------------------------------------------------
    // moveItem
    // -----------------------------------------------------------------------

    void testMoveItem_movesItemForward()
    {
        auto s = makeState(3);
        auto result = playlist::moveItem(s, 0, 2);
        QCOMPARE(result.items[0].path, QString("/track/2.sid"));
        QCOMPARE(result.items[1].path, QString("/track/3.sid"));
        QCOMPARE(result.items[2].path, QString("/track/1.sid"));
    }

    void testMoveItem_movesItemBackward()
    {
        auto s = makeState(3);
        auto result = playlist::moveItem(s, 2, 0);
        QCOMPARE(result.items[0].path, QString("/track/3.sid"));
        QCOMPARE(result.items[1].path, QString("/track/1.sid"));
        QCOMPARE(result.items[2].path, QString("/track/2.sid"));
    }

    void testMoveItem_currentItemMoved_updatesCurrentIndex()
    {
        auto s = makeState(3, 0);
        auto result = playlist::moveItem(s, 0, 2);
        QCOMPARE(result.currentIndex, 2);
    }

    void testMoveItem_itemMovedBeforeCurrentIndex_incrementsCurrentIndex()
    {
        auto s = makeState(4, 2);
        auto result = playlist::moveItem(s, 0, 3);
        // Item at 0 moved after current (was 2), so currentIndex shifts left
        QCOMPARE(result.currentIndex, 1);
    }

    void testMoveItem_itemMovedAfterCurrentIndex_decrementsCurrentIndex()
    {
        auto s = makeState(4, 2);
        auto result = playlist::moveItem(s, 3, 1);
        // Item at 3 moved before current (was 2), so currentIndex shifts right
        QCOMPARE(result.currentIndex, 3);
    }

    void testMoveItem_sameFromAndTo_noChange()
    {
        auto s = makeState(3, 1);
        auto result = playlist::moveItem(s, 1, 1);
        QCOMPARE(result.items.size(), 3);
        QCOMPARE(result.currentIndex, 1);
    }

    void testMoveItem_invalidIndex_noChange()
    {
        auto s = makeState(3, 1);
        auto result = playlist::moveItem(s, -1, 1);
        QCOMPARE(result.items.size(), 3);

        auto result2 = playlist::moveItem(s, 0, 10);
        QCOMPARE(result2.items.size(), 3);
    }

    // -----------------------------------------------------------------------
    // clear
    // -----------------------------------------------------------------------

    void testClear_removesAllItems()
    {
        auto s = makeState(5, 2);
        auto result = playlist::clear(s);
        QVERIFY(result.items.isEmpty());
        QVERIFY(result.shuffleOrder.isEmpty());
        QCOMPARE(result.currentIndex, -1);
    }

    void testClear_preservesSettings()
    {
        auto s = makeState(5, 2, false, playlist::RepeatMode::All, 300);
        auto result = playlist::clear(s);
        QCOMPARE(result.repeatMode, playlist::RepeatMode::All);
        QCOMPARE(result.defaultDuration, 300);
    }

    void testClear_emptyStateUnchanged()
    {
        playlist::State s;
        auto result = playlist::clear(s);
        QVERIFY(result.items.isEmpty());
        QCOMPARE(result.currentIndex, -1);
    }

    // -----------------------------------------------------------------------
    // nextIndex / previousIndex
    // -----------------------------------------------------------------------

    void testNextIndex_normalMode_returnsNextItem()
    {
        auto s = makeState(3, 0);
        QCOMPARE(playlist::nextIndex(s), 1);
    }

    void testNextIndex_normalMode_atEnd_returnsMinusOne()
    {
        auto s = makeState(3, 2);
        QCOMPARE(playlist::nextIndex(s), -1);
    }

    void testNextIndex_emptyList_returnsMinusOne()
    {
        playlist::State s;
        QCOMPARE(playlist::nextIndex(s), -1);
    }

    void testPreviousIndex_normalMode_returnsPreviousItem()
    {
        auto s = makeState(3, 2);
        QCOMPARE(playlist::previousIndex(s), 1);
    }

    void testPreviousIndex_normalMode_atStart_returnsMinusOne()
    {
        auto s = makeState(3, 0);
        QCOMPARE(playlist::previousIndex(s), -1);
    }

    void testPreviousIndex_emptyList_returnsMinusOne()
    {
        playlist::State s;
        QCOMPARE(playlist::previousIndex(s), -1);
    }

    void testNextIndex_shuffleMode_followsShuffleOrder()
    {
        // Build a state with 3 items; force a known shuffle order
        auto s = makeState(3, -1, true);
        s.shuffleOrder = {2, 0, 1};  // shuffled: play item 2 first, then 0, then 1
        s.currentIndex = 2;          // currently playing item at position 0 in shuffle

        int next = playlist::nextIndex(s);
        QCOMPARE(next, 0);  // next in shuffle is position 1 -> item 0
    }

    void testNextIndex_shuffleMode_atEnd_returnsMinusOne()
    {
        auto s = makeState(3, -1, true);
        s.shuffleOrder = {2, 0, 1};
        s.currentIndex = 1;  // last in shuffle order

        int next = playlist::nextIndex(s);
        QCOMPARE(next, -1);
    }

    void testPreviousIndex_shuffleMode_followsShuffleOrder()
    {
        auto s = makeState(3, -1, true);
        s.shuffleOrder = {2, 0, 1};
        s.currentIndex = 0;  // position 1 in shuffle order

        int prev = playlist::previousIndex(s);
        QCOMPARE(prev, 2);  // position 0 in shuffle -> item 2
    }

    // -----------------------------------------------------------------------
    // advanceIndex
    // -----------------------------------------------------------------------

    void testAdvanceIndex_repeatOff_nextExists_returnsNext()
    {
        auto s = makeState(3, 0, false, playlist::RepeatMode::Off);
        QCOMPARE(playlist::advanceIndex(s), 1);
    }

    void testAdvanceIndex_repeatOff_atEnd_returnsMinusOne()
    {
        auto s = makeState(3, 2, false, playlist::RepeatMode::Off);
        QCOMPARE(playlist::advanceIndex(s), -1);
    }

    void testAdvanceIndex_repeatAll_atEnd_wrapsToFirst()
    {
        auto s = makeState(3, 2, false, playlist::RepeatMode::All);
        QCOMPARE(playlist::advanceIndex(s), 0);
    }

    void testAdvanceIndex_repeatAll_notAtEnd_returnsNext()
    {
        auto s = makeState(3, 0, false, playlist::RepeatMode::All);
        QCOMPARE(playlist::advanceIndex(s), 1);
    }

    void testAdvanceIndex_repeatOne_returnsCurrentIndex()
    {
        auto s = makeState(3, 1, false, playlist::RepeatMode::One);
        QCOMPARE(playlist::advanceIndex(s), 1);
    }

    void testAdvanceIndex_repeatAll_shuffle_atEnd_wrapsToFirstShuffled()
    {
        auto s = makeState(3, -1, true, playlist::RepeatMode::All);
        s.shuffleOrder = {2, 0, 1};
        s.currentIndex = 1;  // last in shuffle order

        int next = playlist::advanceIndex(s);
        QCOMPARE(next, 2);  // wraps to position 0 in shuffle -> item 2
    }

    void testAdvanceIndex_emptyList_returnsMinusOne()
    {
        playlist::State s;
        s.repeatMode = playlist::RepeatMode::All;
        QCOMPARE(playlist::advanceIndex(s), -1);
    }

    // -----------------------------------------------------------------------
    // generateShuffleOrder
    // -----------------------------------------------------------------------

    void testGenerateShuffleOrder_containsAllIndices()
    {
        auto order = playlist::generateShuffleOrder(5, 42);
        QCOMPARE(order.size(), 5);
        QList<int> sorted = order;
        std::sort(sorted.begin(), sorted.end());
        QCOMPARE(sorted, QList<int>({0, 1, 2, 3, 4}));
    }

    void testGenerateShuffleOrder_deterministicWithSameSeed()
    {
        auto order1 = playlist::generateShuffleOrder(10, 12345);
        auto order2 = playlist::generateShuffleOrder(10, 12345);
        QCOMPARE(order1, order2);
    }

    void testGenerateShuffleOrder_differentSeeds_differentOrders()
    {
        auto order1 = playlist::generateShuffleOrder(10, 1);
        auto order2 = playlist::generateShuffleOrder(10, 9999);
        // Very likely to differ (probability of collision is astronomically small)
        QVERIFY(order1 != order2);
    }

    void testGenerateShuffleOrder_zeroItems_returnsEmpty()
    {
        auto order = playlist::generateShuffleOrder(0, 42);
        QVERIFY(order.isEmpty());
    }

    void testGenerateShuffleOrder_oneItem_returnsSingleZero()
    {
        auto order = playlist::generateShuffleOrder(1, 42);
        QCOMPARE(order, QList<int>({0}));
    }

    // -----------------------------------------------------------------------
    // serialize / deserialize (round-trip)
    // -----------------------------------------------------------------------

    void testSerializeDeserialize_roundTrip()
    {
        QList<playlist::PlaylistItem> items;
        playlist::PlaylistItem a;
        a.path = "/C64/Track1.sid";
        a.title = "Track One";
        a.author = "Hubbard";
        a.subsong = 2;
        a.totalSubsongs = 4;
        a.durationSecs = 210;
        items.append(a);

        playlist::PlaylistItem b;
        b.path = "/C64/Track2.sid";
        b.title = "Track Two";
        b.subsong = 1;
        b.totalSubsongs = 1;
        b.durationSecs = 180;
        items.append(b);

        QJsonObject json = playlist::serialize(items);
        auto result = playlist::deserialize(json, 180);

        QCOMPARE(result.size(), 2);
        QCOMPARE(result[0].path, a.path);
        QCOMPARE(result[0].title, a.title);
        QCOMPARE(result[0].author, a.author);
        QCOMPARE(result[0].subsong, a.subsong);
        QCOMPARE(result[0].totalSubsongs, a.totalSubsongs);
        QCOMPARE(result[0].durationSecs, a.durationSecs);
        QCOMPARE(result[1].path, b.path);
    }

    void testDeserialize_skipsItemsWithEmptyPaths()
    {
        QList<playlist::PlaylistItem> items;
        playlist::PlaylistItem good;
        good.path = "/valid.sid";
        good.title = "Valid";
        items.append(good);

        playlist::PlaylistItem empty;
        empty.path = "";
        empty.title = "Empty path";
        items.append(empty);

        QJsonObject json = playlist::serialize(items);
        auto result = playlist::deserialize(json, 180);
        QCOMPARE(result.size(), 1);
        QCOMPARE(result[0].path, QString("/valid.sid"));
    }

    void testDeserialize_usesFallbackDurationWhenMissing()
    {
        // Manually build a JSON without a "duration" field
        QJsonObject root;
        root["version"] = 1;
        QJsonArray arr;
        QJsonObject item;
        item["path"] = "/track.sid";
        item["title"] = "No Duration";
        item["subsong"] = 1;
        item["totalSubsongs"] = 1;
        // No "duration" key
        arr.append(item);
        root["items"] = arr;

        auto result = playlist::deserialize(root, 999);
        QCOMPARE(result.size(), 1);
        QCOMPARE(result[0].durationSecs, 999);
    }

    void testSerialize_includesVersionField()
    {
        QJsonObject json = playlist::serialize({});
        QVERIFY(json.contains("version"));
        QCOMPARE(json["version"].toInt(), 1);
    }

    // -----------------------------------------------------------------------
    // updateItemDurations
    // -----------------------------------------------------------------------

    void testUpdateItemDurations_updatesMatchingPathAndSubsong()
    {
        QList<playlist::PlaylistItem> items;
        playlist::PlaylistItem item;
        item.path = "/track.sid";
        item.subsong = 1;
        item.durationSecs = 180;
        items.append(item);

        auto result = playlist::updateItemDurations(items, "/track.sid", {150});
        QVERIFY(result.anyUpdated);
        QCOMPARE(result.items[0].durationSecs, 150);
        QCOMPARE(result.updatedIndices, QList<int>({0}));
    }

    void testUpdateItemDurations_nonMatchingPath_noUpdate()
    {
        QList<playlist::PlaylistItem> items;
        playlist::PlaylistItem item;
        item.path = "/other.sid";
        item.subsong = 1;
        item.durationSecs = 180;
        items.append(item);

        auto result = playlist::updateItemDurations(items, "/track.sid", {150});
        QVERIFY(!result.anyUpdated);
        QCOMPARE(result.items[0].durationSecs, 180);
    }

    void testUpdateItemDurations_subsongOutOfRange_noUpdate()
    {
        QList<playlist::PlaylistItem> items;
        playlist::PlaylistItem item;
        item.path = "/track.sid";
        item.subsong = 5;  // 0-indexed: 4, but durations only has indices 0-1
        item.durationSecs = 180;
        items.append(item);

        auto result = playlist::updateItemDurations(items, "/track.sid", {100, 200});
        QVERIFY(!result.anyUpdated);
        QCOMPARE(result.items[0].durationSecs, 180);
    }

    void testUpdateItemDurations_durationUnchanged_notMarkedUpdated()
    {
        QList<playlist::PlaylistItem> items;
        playlist::PlaylistItem item;
        item.path = "/track.sid";
        item.subsong = 1;
        item.durationSecs = 150;  // Same as what we're setting
        items.append(item);

        auto result = playlist::updateItemDurations(items, "/track.sid", {150});
        QVERIFY(!result.anyUpdated);
        QVERIFY(result.updatedIndices.isEmpty());
    }

    void testUpdateItemDurations_multipleItemsSamePath()
    {
        QList<playlist::PlaylistItem> items;
        // Two items for same file, different subsongs
        playlist::PlaylistItem item1;
        item1.path = "/track.sid";
        item1.subsong = 1;
        item1.durationSecs = 180;
        items.append(item1);

        playlist::PlaylistItem item2;
        item2.path = "/track.sid";
        item2.subsong = 2;
        item2.durationSecs = 180;
        items.append(item2);

        auto result = playlist::updateItemDurations(items, "/track.sid", {100, 200});
        QVERIFY(result.anyUpdated);
        QCOMPARE(result.items[0].durationSecs, 100);
        QCOMPARE(result.items[1].durationSecs, 200);
        QCOMPARE(result.updatedIndices.size(), 2);
    }
};

QTEST_MAIN(TestPlaylistCore)
#include "test_playlistcore.moc"
