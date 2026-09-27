/**
 * @file test_configfiltercore.cpp
 * @brief Unit tests for the pure Config mode filter and layout rules.
 *
 * Fixture: Network{Hostname, Port}, Audio{Volume, Mute}, Video{} (unloaded).
 */

#include "core/configfiltercore.h"

#include <QtTest>

using configfiltercore::CategoryItems;

class TestConfigFilterCore : public QObject
{
    Q_OBJECT

private:
    static QList<CategoryItems> fixture()
    {
        return {
            {"Network", {"Port", "Hostname"}},
            {"Audio", {"Volume", "Mute"}},
            {"Video", {}},
        };
    }

private slots:

    // -----------------------------------------------------------------------
    // normalizeFilter / isFilterActive
    // -----------------------------------------------------------------------

    void testNormalizeFilter_TrimsWhitespace()
    {
        QCOMPARE(configfiltercore::normalizeFilter("  port \t"), QString("port"));
    }

    void testIsFilterActive_EmptyText_IsInactive()
    {
        QVERIFY(!configfiltercore::isFilterActive(""));
    }

    void testIsFilterActive_WhitespaceOnly_IsInactive()
    {
        QVERIFY(!configfiltercore::isFilterActive("   "));
    }

    void testIsFilterActive_Text_IsActive() { QVERIFY(configfiltercore::isFilterActive("po")); }

    // -----------------------------------------------------------------------
    // matches
    // -----------------------------------------------------------------------

    void testMatches_InactiveFilter_MatchesEverything()
    {
        QVERIFY(configfiltercore::matches("Network", "Hostname", ""));
        QVERIFY(configfiltercore::matches("Network", "Hostname", "  "));
    }

    void testMatches_ItemName_IsCaseInsensitiveSubstring()
    {
        QVERIFY(configfiltercore::matches("Network", "Hostname", "HOST"));
        QVERIFY(configfiltercore::matches("Network", "Hostname", "nam"));
        QVERIFY(!configfiltercore::matches("Network", "Hostname", "volume"));
    }

    void testMatches_CategoryName_IncludesAllItsItems()
    {
        QVERIFY(configfiltercore::matches("Audio", "Volume", "aud"));
        QVERIFY(configfiltercore::matches("Audio", "Mute", "aud"));
    }

    // -----------------------------------------------------------------------
    // filterSnapshot
    // -----------------------------------------------------------------------

    void testFilterSnapshot_InactiveFilter_ReturnsEverythingSorted()
    {
        const auto result = configfiltercore::filterSnapshot(fixture(), "");
        const QList<CategoryItems> expected = {
            {"Network", {"Hostname", "Port"}},
            {"Audio", {"Mute", "Volume"}},
            {"Video", {}},
        };
        QCOMPARE(result, expected);
    }

    void testFilterSnapshot_ItemMatch_SurfacesItsCategory()
    {
        const auto result = configfiltercore::filterSnapshot(fixture(), "mute");
        const QList<CategoryItems> expected = {{"Audio", {"Mute"}}};
        QCOMPARE(result, expected);
    }

    void testFilterSnapshot_SpansCategories_PreservesCategoryOrder()
    {
        // "m" matches item names in both categories but neither category name
        const auto result = configfiltercore::filterSnapshot(fixture(), "m");
        const QList<CategoryItems> expected = {
            {"Network", {"Hostname"}},
            {"Audio", {"Mute", "Volume"}},
        };
        QCOMPARE(result, expected);
    }

    void testFilterSnapshot_NoMatches_ReturnsEmpty()
    {
        QVERIFY(configfiltercore::filterSnapshot(fixture(), "zzz").isEmpty());
    }

    void testFilterSnapshot_UnloadedCategory_DroppedWhileActive()
    {
        // "Video" matches its own category name but has no loaded items
        QVERIFY(configfiltercore::filterSnapshot(fixture(), "video").isEmpty());
    }

    // -----------------------------------------------------------------------
    // visibleCategories
    // -----------------------------------------------------------------------

    void testVisibleCategories_InactiveFilter_ListsAllInOrder()
    {
        QCOMPARE(configfiltercore::visibleCategories(fixture(), ""),
                 QStringList({"Network", "Audio", "Video"}));
    }

    void testVisibleCategories_ActiveFilter_ListsOnlyMatching()
    {
        QCOMPARE(configfiltercore::visibleCategories(fixture(), "vol"), QStringList({"Audio"}));
        QCOMPARE(configfiltercore::visibleCategories(fixture(), "m"),
                 QStringList({"Network", "Audio"}));
    }

    // -----------------------------------------------------------------------
    // columnCountForWidth
    // -----------------------------------------------------------------------

    void testColumnCountForWidth_BelowThreshold_IsOne()
    {
        QCOMPARE(configfiltercore::columnCountForWidth(configfiltercore::kTwoColumnMinWidth - 1),
                 1);
    }

    void testColumnCountForWidth_AtThreshold_IsTwo()
    {
        QCOMPARE(configfiltercore::columnCountForWidth(configfiltercore::kTwoColumnMinWidth), 2);
    }

    void testColumnCountForWidth_NonPositive_IsOne()
    {
        QCOMPARE(configfiltercore::columnCountForWidth(0), 1);
        QCOMPARE(configfiltercore::columnCountForWidth(-5), 1);
    }

    void testColumnCountForWidth_CustomThreshold()
    {
        QCOMPARE(configfiltercore::columnCountForWidth(400, 300), 2);
        QCOMPARE(configfiltercore::columnCountForWidth(299, 300), 1);
    }

    // -----------------------------------------------------------------------
    // itemKey
    // -----------------------------------------------------------------------

    void testItemKey_JoinsWithSlash()
    {
        QCOMPARE(configfiltercore::itemKey("Network", "Hostname"), QString("Network/Hostname"));
    }
};

QTEST_MAIN(TestConfigFilterCore)
#include "test_configfiltercore.moc"
