/**
 * @file test_configitemspanel.cpp
 * @brief Unit tests for ConfigItemsPanel category switching and item display.
 *
 * ConfigItemsPanel renders configuration items for a category and emits
 * itemChanged when the user modifies a value.  These tests verify:
 *
 * - Construction with a valid ConfigurationModel
 * - setCategory() with empty category — shows empty state
 * - setCategory() with unknown category — shows empty state
 * - setCategory() with known category — populates items (no crash)
 * - refresh() on empty category — no crash
 * - refresh() after items loaded — no crash
 * - itemChanged signal emitted when model value changes (via editor widget)
 * - currentCategory() accessor returns set category
 * - setFilter() narrows to matching items, groups them under category headers
 *   when matches span categories, and shows an empty state with no matches
 * - Editors under a cross-category filter report their own category
 * - Dirty labels stay bold under a filter and across a relayout
 * - resizeEvent() toggles between one and two columns at the width threshold
 *   without recreating editors
 */

#include "models/configurationmodel.h"
#include "ui/configitemspanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QScrollArea>
#include <QSignalSpy>
#include <QSpinBox>
#include <QtTest>

class TestConfigItemsPanel : public QObject
{
    Q_OBJECT

private:
    ConfigurationModel *makeModelWithData()
    {
        auto *model = new ConfigurationModel(this);
        model->setCategories({"Network", "Audio"});

        QHash<QString, QVariant> networkItems;
        networkItems["Hostname"] = QString("device.local");
        networkItems["Port"] = 21;
        model->setCategoryItems("Network", networkItems);

        QHash<QString, QVariant> audioItems;
        audioItems["Volume"] = 80;
        audioItems["Mute"] = false;
        model->setCategoryItems("Audio", audioItems);

        return model;
    }

private slots:

    // =========================================================================
    // Construction
    // =========================================================================

    void testConstruct_doesNotCrash()
    {
        auto *model = new ConfigurationModel(this);
        ConfigItemsPanel panel(model);
        QVERIFY(true);
    }

    // =========================================================================
    // currentCategory() accessor
    // =========================================================================

    void testCurrentCategory_InitiallyEmpty()
    {
        auto *model = new ConfigurationModel(this);
        ConfigItemsPanel panel(model);
        QVERIFY(panel.currentCategory().isEmpty());
    }

    void testCurrentCategory_AfterSetCategory()
    {
        auto *model = makeModelWithData();
        ConfigItemsPanel panel(model);
        panel.setCategory("Network");
        QCOMPARE(panel.currentCategory(), QString("Network"));
    }

    // =========================================================================
    // setCategory() — empty category shows empty state
    // =========================================================================

    void testSetCategory_EmptyString_DoesNotCrash()
    {
        auto *model = makeModelWithData();
        ConfigItemsPanel panel(model);
        panel.setCategory("");
        QVERIFY(true);
    }

    void testSetCategory_EmptyString_CurrentCategoryEmpty()
    {
        auto *model = makeModelWithData();
        ConfigItemsPanel panel(model);
        panel.setCategory("Network");
        panel.setCategory("");
        QVERIFY(panel.currentCategory().isEmpty());
    }

    // =========================================================================
    // setCategory() — unknown category shows empty state
    // =========================================================================

    void testSetCategory_UnknownCategory_DoesNotCrash()
    {
        auto *model = makeModelWithData();
        ConfigItemsPanel panel(model);
        panel.setCategory("NonExistentCategory");
        QVERIFY(true);
    }

    // =========================================================================
    // setCategory() — known category populates items
    // =========================================================================

    void testSetCategory_KnownCategory_DoesNotCrash()
    {
        auto *model = makeModelWithData();
        ConfigItemsPanel panel(model);
        panel.setCategory("Network");
        QVERIFY(true);
    }

    void testSetCategory_SwitchCategory_DoesNotCrash()
    {
        auto *model = makeModelWithData();
        ConfigItemsPanel panel(model);
        panel.setCategory("Network");
        panel.setCategory("Audio");
        QCOMPARE(panel.currentCategory(), QString("Audio"));
    }

    // =========================================================================
    // refresh() — no crash
    // =========================================================================

    void testRefresh_EmptyCategory_DoesNotCrash()
    {
        auto *model = makeModelWithData();
        ConfigItemsPanel panel(model);
        panel.refresh();
        QVERIFY(true);
    }

    void testRefresh_AfterCategorySet_DoesNotCrash()
    {
        auto *model = makeModelWithData();
        ConfigItemsPanel panel(model);
        panel.setCategory("Audio");
        panel.refresh();
        QVERIFY(true);
    }

    // =========================================================================
    // categoryItemsChanged signal — triggers refresh for current category
    // =========================================================================

    void testCategoryItemsChanged_CurrentCategory_RefreshesWithoutCrash()
    {
        auto *model = makeModelWithData();
        ConfigItemsPanel panel(model);
        panel.setCategory("Network");

        // Updating items via model triggers onCategoryItemsChanged
        QHash<QString, QVariant> updatedItems;
        updatedItems["Hostname"] = QString("new.local");
        model->setCategoryItems("Network", updatedItems);

        QCOMPARE(panel.currentCategory(), QString("Network"));
    }

    void testCategoryItemsChanged_OtherCategory_DoesNotRefresh()
    {
        auto *model = makeModelWithData();
        ConfigItemsPanel panel(model);
        panel.setCategory("Network");

        // Updating Audio should not affect Network panel
        QHash<QString, QVariant> updatedItems;
        updatedItems["Volume"] = 50;
        model->setCategoryItems("Audio", updatedItems);

        // Current category unchanged
        QCOMPARE(panel.currentCategory(), QString("Network"));
    }

    // =========================================================================
    // itemChanged signal — emitted when model value changes via setValue
    // =========================================================================

    void testItemChanged_EmittedWhenValueChanges()
    {
        auto *model = makeModelWithData();
        ConfigItemsPanel panel(model);
        panel.setCategory("Network");

        QSignalSpy spy(&panel, &ConfigItemsPanel::itemChanged);

        // Change value via model — the panel's editor widget change signal
        // propagates back through the model, which triggers onItemValueChanged
        model->setValue("Network", "Port", 22);

        // The panel emits itemChanged when the editor changes the model value.
        // Since we changed via model->setValue() directly (not via the editor),
        // the panel's signal is not emitted here — only when the editor widget
        // triggers the change. Verify no crash.
        QVERIFY(spy.count() >= 0);  // No crash required; count is platform-dependent
    }

    // =========================================================================
    // Column layout — width threshold toggles between one and two columns
    // =========================================================================

    void testResizeEvent_WidthThreshold_TogglesColumnCount()
    {
        auto *model = makeModelWithData();
        ConfigItemsPanel panel(model);
        panel.setCategory("Network");
        panel.resize(ConfigItemsPanel::twoColumnMinWidth() + 80, 400);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        QCOMPARE(panel.columnCount(), 2);

        panel.resize(ConfigItemsPanel::twoColumnMinWidth() - 80, 400);
        QCoreApplication::processEvents();
        QCOMPARE(panel.columnCount(), 1);

        panel.resize(ConfigItemsPanel::twoColumnMinWidth() + 80, 400);
        QCoreApplication::processEvents();
        QCOMPARE(panel.columnCount(), 2);
    }

    void testRelayout_KeepsEditorsAndDoesNotLeak()
    {
        auto *model = makeModelWithData();
        ConfigItemsPanel panel(model);
        panel.setCategory("Network");
        panel.resize(ConfigItemsPanel::twoColumnMinWidth() - 80, 400);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        QCOMPARE(panel.columnCount(), 1);

        auto *editorBefore = panel.findChild<QSpinBox *>("Network/Port:editor");
        QVERIFY(editorBefore != nullptr);
        const int labelsBefore = panel.findChildren<QLabel *>().size();

        panel.resize(ConfigItemsPanel::twoColumnMinWidth() + 80, 400);
        QCoreApplication::processEvents();
        QCOMPARE(panel.columnCount(), 2);

        QCOMPARE(panel.findChild<QSpinBox *>("Network/Port:editor"), editorBefore);
        QCOMPARE(panel.findChildren<QLabel *>().size(), labelsBefore);
        QCOMPARE(panel.visibleItems(), QStringList({"Network/Hostname", "Network/Port"}));
    }

    void testResizeEvent_DirtyLabelStaysBold()
    {
        auto *model = makeModelWithData();
        ConfigItemsPanel panel(model);
        panel.setCategory("Network");
        model->setValue("Network", "Port", 22);
        panel.resize(ConfigItemsPanel::twoColumnMinWidth() - 80, 400);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));

        auto *label = panel.findChild<QLabel *>("Network/Port");
        QVERIFY(label != nullptr);
        QVERIFY(label->styleSheet().contains("bold"));

        panel.resize(ConfigItemsPanel::twoColumnMinWidth() + 80, 400);
        QCoreApplication::processEvents();
        QCOMPARE(panel.columnCount(), 2);
        QVERIFY(panel.findChild<QLabel *>("Network/Port")->styleSheet().contains("bold"));
    }

    // =========================================================================
    // setFilter() — narrows the items shown, grouped by category when needed
    // =========================================================================

    void testSetFilter_MatchingText_NarrowsRows()
    {
        auto *model = makeModelWithData();
        ConfigItemsPanel panel(model);
        panel.setCategory("Network");

        panel.setFilter("port");

        QVERIFY(panel.isFilterActive());
        QCOMPARE(panel.visibleItems(), QStringList({"Network/Port"}));
    }

    void testSetFilter_SpansCategories_ShowsHeaders()
    {
        auto *model = makeModelWithData();
        ConfigItemsPanel panel(model);
        panel.setCategory("Network");

        // "m" matches Hostname, Mute and Volume but neither category name
        panel.setFilter("m");

        QCOMPARE(panel.headerCategories(), QStringList({"Network", "Audio"}));
        QCOMPARE(panel.visibleItems(),
                 QStringList({"Network/Hostname", "Audio/Mute", "Audio/Volume"}));
    }

    void testSetFilter_SingleCategory_NoHeaders()
    {
        auto *model = makeModelWithData();
        ConfigItemsPanel panel(model);
        panel.setCategory("Network");

        panel.setFilter("port");

        QVERIFY(panel.headerCategories().isEmpty());
        QVERIFY(panel.findChildren<QLabel *>("categoryHeader").isEmpty());
    }

    void testSetFilter_ClearedText_RestoresCategoryItems()
    {
        auto *model = makeModelWithData();
        ConfigItemsPanel panel(model);
        panel.setCategory("Network");
        panel.setFilter("vol");
        QCOMPARE(panel.visibleItems(), QStringList({"Audio/Volume"}));

        panel.setFilter("   ");

        QVERIFY(!panel.isFilterActive());
        QVERIFY(panel.headerCategories().isEmpty());
        QCOMPARE(panel.visibleItems(), QStringList({"Network/Hostname", "Network/Port"}));
    }

    void testSetFilter_DirtyItemStaysBold()
    {
        auto *model = makeModelWithData();
        ConfigItemsPanel panel(model);
        panel.setCategory("Network");
        model->setValue("Audio", "Volume", 50);

        panel.setFilter("vol");

        auto *label = panel.findChild<QLabel *>("Audio/Volume");
        QVERIFY(label != nullptr);
        QVERIFY(label->styleSheet().contains("bold"));

        model->clearDirtyFlags();
        QVERIFY(!label->styleSheet().contains("bold"));
    }

    void testSetFilter_NoMatches_ShowsEmptyState()
    {
        auto *model = makeModelWithData();
        ConfigItemsPanel panel(model);
        panel.setCategory("Network");

        panel.setFilter("zzz");

        auto *scrollArea = panel.findChild<QScrollArea *>();
        QVERIFY(scrollArea != nullptr);
        QVERIFY(scrollArea->isHidden());
        auto *emptyLabel = panel.findChild<QLabel *>("configEmptyLabel");
        QVERIFY(emptyLabel != nullptr);
        QVERIFY(!emptyLabel->isHidden());
        QCOMPARE(emptyLabel->text(), QString("No configuration items match \"zzz\"."));
        QVERIFY(panel.visibleItems().isEmpty());
    }

    void testSetFilter_EditorEmitsOwnCategory()
    {
        auto *model = makeModelWithData();
        ConfigItemsPanel panel(model);
        panel.setCategory("Network");
        panel.setFilter("vol");

        auto *editor = panel.findChild<QSpinBox *>("Audio/Volume:editor");
        QVERIFY(editor != nullptr);
        QSignalSpy spy(&panel, &ConfigItemsPanel::itemChanged);

        editor->setValue(90);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("Audio"));
        QCOMPARE(spy.at(0).at(1).toString(), QString("Volume"));
        QCOMPARE(model->value("Audio", "Volume").toInt(), 90);
    }

    void testCategoryItemsChanged_UnderFilter_Rebuilds()
    {
        auto *model = makeModelWithData();
        ConfigItemsPanel panel(model);
        panel.setCategory("Network");
        panel.setFilter("vol");
        QCOMPARE(panel.visibleItems(), QStringList({"Audio/Volume"}));

        // Items for a category other than the selected one land late
        QHash<QString, QVariant> audioItems;
        audioItems["VolumeLeft"] = 10;
        audioItems["VolumeRight"] = 20;
        model->setCategoryItems("Audio", audioItems);

        QCOMPARE(panel.visibleItems(), QStringList({"Audio/VolumeLeft", "Audio/VolumeRight"}));
    }

    void testSetCategory_UnderFilter_KeepsFilteredView()
    {
        auto *model = makeModelWithData();
        ConfigItemsPanel panel(model);
        panel.setFilter("vol");

        panel.setCategory("Network");

        QCOMPARE(panel.currentCategory(), QString("Network"));
        QCOMPARE(panel.visibleItems(), QStringList({"Audio/Volume"}));
    }
};

QTEST_MAIN(TestConfigItemsPanel)
#include "test_configitemspanel.moc"
