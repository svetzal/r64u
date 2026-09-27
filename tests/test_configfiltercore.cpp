/**
 * @file test_configfiltercore.cpp
 * @brief Unit tests for the pure Config mode filter and layout rules.
 *
 * Fixture: Network{Hostname, Port}, Audio{Volume, Mute}, Video{} (unloaded).
 * Choices fixture: Video{Mode: PAL/NTSC, Scanlines: Enabled/Disabled, Border},
 * Audio{Volume}.
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

    // Video's Mode and Scanlines offer choices; Border and Volume offer none
    static QList<CategoryItems> choicesFixture()
    {
        CategoryItems video;
        video.category = "Video";
        video.items = {"Mode", "Scanlines", "Border"};
        video.choices = {{"Mode", {"PAL", "NTSC"}}, {"Scanlines", {"Enabled", "Disabled"}}};

        CategoryItems audio;
        audio.category = "Audio";
        audio.items = {"Volume"};
        return {video, audio};
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
        QVERIFY(configfiltercore::matches("Network", "Hostname", {}, ""));
        QVERIFY(configfiltercore::matches("Network", "Hostname", {}, "  "));
    }

    void testMatches_ItemName_IsCaseInsensitiveSubstring()
    {
        QVERIFY(configfiltercore::matches("Network", "Hostname", {}, "HOST"));
        QVERIFY(configfiltercore::matches("Network", "Hostname", {}, "nam"));
        QVERIFY(!configfiltercore::matches("Network", "Hostname", {}, "volume"));
    }

    void testMatches_CategoryName_IncludesAllItsItems()
    {
        QVERIFY(configfiltercore::matches("Audio", "Volume", {}, "aud"));
        QVERIFY(configfiltercore::matches("Audio", "Mute", {}, "aud"));
    }

    void testMatches_Choice_IsCaseInsensitiveSubstring()
    {
        const QStringList choices = {"PAL", "NTSC"};
        QVERIFY(configfiltercore::matches("Video", "Mode", choices, "ntsc"));
        QVERIFY(configfiltercore::matches("Video", "Mode", choices, "NTS"));
        QVERIFY(configfiltercore::matches("Video", "Mode", choices, "Pa"));
        QVERIFY(!configfiltercore::matches("Video", "Mode", choices, "secam"));
    }

    void testMatches_NoChoices_StillMatchesNames()
    {
        QVERIFY(configfiltercore::matches("Video", "Mode", {}, "mod"));
        QVERIFY(configfiltercore::matches("Video", "Mode", {}, "vid"));
        QVERIFY(!configfiltercore::matches("Video", "Mode", {}, "pal"));
    }

    // -----------------------------------------------------------------------
    // editorChoices — the choices a setting's dropdown offers
    // -----------------------------------------------------------------------

    void testEditorChoices_Options_AreReturnedAsGiven()
    {
        QCOMPARE(configfiltercore::editorChoices(QString("PAL"), {"PAL", "NTSC"}),
                 QStringList({"PAL", "NTSC"}));
    }

    void testEditorChoices_OptionsWinOverNumericValue()
    {
        QCOMPARE(configfiltercore::editorChoices(3, {"1", "2", "3"}), QStringList({"1", "2", "3"}));
    }

    void testEditorChoices_BooleanLookingString_BuildsItsPair_data()
    {
        QTest::addColumn<QString>("value");
        QTest::addColumn<QStringList>("expected");
        QTest::newRow("yes") << "yes" << QStringList({"Yes", "No"});
        QTest::newRow("No") << "No" << QStringList({"Yes", "No"});
        QTest::newRow("Enabled") << "Enabled" << QStringList({"Enabled", "Disabled"});
        QTest::newRow("DISABLED") << "DISABLED" << QStringList({"Enabled", "Disabled"});
        QTest::newRow("on") << "on" << QStringList({"On", "Off"});
        QTest::newRow("Off") << "Off" << QStringList({"On", "Off"});
        QTest::newRow("true") << "true" << QStringList({"True", "False"});
        QTest::newRow("False") << "False" << QStringList({"True", "False"});
    }

    void testEditorChoices_BooleanLookingString_BuildsItsPair()
    {
        QFETCH(QString, value);
        QFETCH(QStringList, expected);
        QCOMPARE(configfiltercore::editorChoices(value, {}), expected);
    }

    void testEditorChoices_ValuesWithoutADropdown_OfferNothing_data()
    {
        QTest::addColumn<QVariant>("value");
        QTest::newRow("free text") << QVariant(QString("device.local"));
        QTest::newRow("empty text") << QVariant(QString());
        QTest::newRow("integer") << QVariant(21);
        QTest::newRow("long long") << QVariant(qlonglong(21));
        QTest::newRow("double") << QVariant(1.5);
        QTest::newRow("bool") << QVariant(true);
        QTest::newRow("invalid") << QVariant();
    }

    void testEditorChoices_ValuesWithoutADropdown_OfferNothing()
    {
        QFETCH(QVariant, value);
        QVERIFY(configfiltercore::editorChoices(value, {}).isEmpty());
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

    void testFilterSnapshot_OptionMatch_SurfacesItemWithItsChoices()
    {
        const auto result = configfiltercore::filterSnapshot(choicesFixture(), "ntsc");
        CategoryItems expected;
        expected.category = "Video";
        expected.items = {"Mode"};
        expected.choices = {{"Mode", {"PAL", "NTSC"}}};
        QCOMPARE(result, QList<CategoryItems>({expected}));
    }

    void testFilterSnapshot_BooleanPairMatch_SurfacesItem()
    {
        // Scanlines is currently "Enabled"; its dropdown also offers "Disabled"
        const auto result = configfiltercore::filterSnapshot(choicesFixture(), "DISABLED");
        QCOMPARE(result.size(), 1);
        QCOMPARE(result.first().category, QString("Video"));
        QCOMPARE(result.first().items, QStringList({"Scanlines"}));
    }

    void testFilterSnapshot_ChoicesBelongToTheirOwnItem()
    {
        // Only Mode offers PAL; its siblings do not borrow its choices
        const auto result = configfiltercore::filterSnapshot(choicesFixture(), "pal");
        QCOMPARE(result.size(), 1);
        QCOMPARE(result.first().items, QStringList({"Mode"}));
    }

    void testCategoryItems_Equality_ComparesChoices()
    {
        CategoryItems a;
        a.category = "Video";
        a.items = {"Mode"};
        a.choices = {{"Mode", {"PAL", "NTSC"}}};
        CategoryItems b = a;
        QVERIFY(a == b);
        b.choices = {{"Mode", {"PAL"}}};
        QVERIFY(a != b);
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

    void testVisibleCategories_CategoryMatchedOnlyThroughAChoice()
    {
        QCOMPARE(configfiltercore::visibleCategories(choicesFixture(), "ntsc"),
                 QStringList({"Video"}));
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
    // scrollValueToReveal — scroll position that brings a span into view
    // -----------------------------------------------------------------------

    void testScrollValueToReveal_AlreadyVisible_KeepsPosition()
    {
        // Viewport shows 100..299; the span 150..250 fits inside it
        QCOMPARE(configfiltercore::scrollValueToReveal(100, 200, 150, 250), 100);
    }

    void testScrollValueToReveal_BelowViewport_ScrollsDownUntilBottomFits()
    {
        QCOMPARE(configfiltercore::scrollValueToReveal(0, 200, 300, 420), 220);
    }

    void testScrollValueToReveal_AboveViewport_ScrollsUpToTop()
    {
        QCOMPARE(configfiltercore::scrollValueToReveal(500, 200, 120, 260), 120);
    }

    void testScrollValueToReveal_TallerThanViewport_PutsTopAtTop()
    {
        QCOMPARE(configfiltercore::scrollValueToReveal(0, 200, 300, 800), 300);
        QCOMPARE(configfiltercore::scrollValueToReveal(900, 200, 300, 800), 300);
    }

    void testScrollValueToReveal_ExactlyViewportHigh_FitsWithoutMovingPastTop()
    {
        QCOMPARE(configfiltercore::scrollValueToReveal(0, 200, 300, 500), 300);
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
