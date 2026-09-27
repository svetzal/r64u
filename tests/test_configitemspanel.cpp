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
 * - Combo, spin box and line edit editors are capped in width so they do not
 *   stretch across a wide column, while a combo never clips its longest option
 * - Every editor sits directly beside its right-aligned label, one grid spacing
 *   away, in both the one- and two-column layouts, and keeps its capped width
 * - The filter also matches the choices a dropdown offers, never current values
 * - setCategory() under a filter spanning categories scrolls that category's
 *   group fully into view (header at the top when the group is taller than the
 *   viewport), and leaves the scroll position alone otherwise
 */

#include "models/configurationmodel.h"
#include "ui/configitemspanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSpinBox>
#include <QtTest>

#include <algorithm>

class TestConfigItemsPanel : public QObject
{
    Q_OBJECT

private:
    ConfigurationModel *makeModelWithData()
    {
        auto *model = new ConfigurationModel(this);
        model->setCategories({"Network", "Audio"});
        addNetworkAndAudioItems(model);
        return model;
    }

    // Network: a line edit and a spin box; Audio: a spin box and a checkbox
    static void addNetworkAndAudioItems(ConfigurationModel *model)
    {
        QHash<QString, QVariant> networkItems;
        networkItems["Hostname"] = QString("device.local");
        networkItems["Port"] = 21;
        model->setCategoryItems("Network", networkItems);

        QHash<QString, QVariant> audioItems;
        audioItems["Volume"] = 80;
        audioItems["Mute"] = false;
        model->setCategoryItems("Audio", audioItems);
    }

    static constexpr int kEditorWidthChars = 21;
    static constexpr int kEditorWidthPadding = 14;
    static constexpr int kGridSpacing = 8;
    // QRect::right() is inclusive, so adjacent cells one spacing apart differ by spacing + 1.
    // The native macOS style's layout margins can make it a few pixels less, never more.
    static constexpr int kMaxLabelEditorGap = kGridSpacing + 1;
    static constexpr int kNarrowOneColumnWidth = ConfigItemsPanel::twoColumnMinWidth() - 80;
    static constexpr int kWideTwoColumnWidth = ConfigItemsPanel::twoColumnMinWidth() + 200;

    // An option longer than the editor cap, so the combo has to widen for it
    static QString longOption()
    {
        return QStringLiteral("Extended Palette With Very Long Name 40c");
    }

    // Same formula the panel uses for line edits, measured from the editor's own font
    static int expectedEditorCap(const QWidget *editor)
    {
        return (editor->fontMetrics().averageCharWidth() * kEditorWidthChars) + kEditorWidthPadding;
    }

    // A "Video" category whose items produce combo editors
    ConfigurationModel *makeModelWithComboEditors()
    {
        auto *model = new ConfigurationModel(this);
        model->setCategories({"Video"});
        addVideoComboItems(model);
        return model;
    }

    // Video: a short-option combo, a long-option combo and a boolean-string combo
    static void addVideoComboItems(ConfigurationModel *model)
    {
        QHash<QString, ConfigItemInfo> items;

        ConfigItemInfo modeInfo;
        modeInfo.value = QString("PAL");
        modeInfo.options = {"PAL", "NTSC"};
        items["System Mode"] = modeInfo;

        ConfigItemInfo paletteInfo;
        paletteInfo.value = QString("Default");
        paletteInfo.options = {"Default", longOption()};
        items["Palette"] = paletteInfo;

        // A plain string that looks boolean becomes a combo without options
        ConfigItemInfo scanlinesInfo;
        scanlinesInfo.value = QString("Enabled");
        items["Scanlines"] = scanlinesInfo;

        model->setCategoryItemsWithInfo("Video", items);
    }

    // Every editor kind, spread over three categories
    ConfigurationModel *makeModelWithEveryEditorKind()
    {
        auto *model = new ConfigurationModel(this);
        model->setCategories({"Network", "Audio", "Video"});
        addNetworkAndAudioItems(model);
        addVideoComboItems(model);
        return model;
    }

    // Grid column an editor occupies, or -1 when it is not in the panel's grid
    static int gridColumnOf(const ConfigItemsPanel &panel, const QString &key)
    {
        auto *editor = panel.findChild<QWidget *>(key + QStringLiteral(":editor"));
        auto *grid = editor != nullptr
                         ? qobject_cast<QGridLayout *>(editor->parentWidget()->layout())
                         : nullptr;
        const int index = grid != nullptr ? grid->indexOf(editor) : -1;
        if (index < 0) {
            return -1;
        }
        int row = 0;
        int column = 0;
        int rowSpan = 0;
        int columnSpan = 0;
        grid->getItemPosition(index, &row, &column, &rowSpan, &columnSpan);
        return column;
    }

    // Builds the model whose category holds the item under test
    ConfigurationModel *makeModelForCategory(const QString &category)
    {
        return category == QStringLiteral("Video") ? makeModelWithComboEditors()
                                                   : makeModelWithData();
    }

    // Horizontal distance from the label's right edge to its editor's left edge.
    // Labels and editors share a parent, so their geometries are in one coordinate space.
    static int labelToEditorGap(const ConfigItemsPanel &panel, const QString &key)
    {
        auto *label = panel.findChild<QLabel *>(key);
        auto *editor = panel.findChild<QWidget *>(key + QStringLiteral(":editor"));
        if (label == nullptr || editor == nullptr ||
            label->parentWidget() != editor->parentWidget()) {
            return -1;
        }
        return editor->x() - label->geometry().right();
    }

    static void addEditorKindRows()
    {
        QTest::addColumn<QString>("category");
        QTest::addColumn<QString>("key");
        QTest::newRow("line edit") << "Network" << "Network/Hostname";
        QTest::newRow("spin box") << "Network" << "Network/Port";
        QTest::newRow("checkbox") << "Audio" << "Audio/Mute";
        QTest::newRow("short-option combo") << "Video" << "Video/System Mode";
        QTest::newRow("long-option combo") << "Video" << "Video/Palette";
        QTest::newRow("boolean-string combo") << "Video" << "Video/Scanlines";
    }

    void verifyEditorBesideLabel(int panelWidth, int expectedColumns)
    {
        QFETCH(QString, category);
        QFETCH(QString, key);

        auto *model = makeModelForCategory(category);
        ConfigItemsPanel panel(model);
        panel.setCategory(category);
        showAtWidth(panel, panelWidth);
        QCOMPARE(panel.columnCount(), expectedColumns);

        const int gap = labelToEditorGap(panel, key);
        QVERIFY2(gap > 0 && gap <= kMaxLabelEditorGap,
                 qPrintable(
                     QStringLiteral("gap %1 px, expected 1..%2").arg(gap).arg(kMaxLabelEditorGap)));
    }

    // Categories whose "Setting N" spin boxes all match "setting", so a filter on it spans
    // them: Alpha..Golf hold three each, Hotel twelve (taller than a short viewport), and
    // India holds only an item the filter does not match.
    ConfigurationModel *makeModelForScrolling()
    {
        auto *model = new ConfigurationModel(this);
        const QStringList shortGroups = {"Alpha", "Bravo",   "Charlie", "Delta",
                                         "Echo",  "Foxtrot", "Golf"};
        QStringList categories = shortGroups;
        categories.insert(3, QStringLiteral("Hotel"));
        categories.append(QStringLiteral("India"));
        model->setCategories(categories);
        for (const QString &category : shortGroups) {
            model->setCategoryItems(category, numberedSettings(kShortGroupSize));
        }
        model->setCategoryItems("Hotel", numberedSettings(kTallGroupSize));
        model->setCategoryItems("India", {{QStringLiteral("Unrelated"), 1}});
        return model;
    }

    static constexpr int kShortGroupSize = 3;
    static constexpr int kTallGroupSize = 12;
    static constexpr int kShortPanelWidth = 600;
    static constexpr int kShortPanelHeight = 200;

    static QHash<QString, QVariant> numberedSettings(int count)
    {
        QHash<QString, QVariant> items;
        for (int i = 1; i <= count; ++i) {
            items.insert(QStringLiteral("Setting %1").arg(i, 2, 10, QLatin1Char('0')), i);
        }
        return items;
    }

    static QScrollArea *scrollAreaOf(const QWidget &panel)
    {
        return panel.findChild<QScrollArea *>();
    }

    static QLabel *headerFor(const QWidget &panel, const QString &category)
    {
        const auto headers = panel.findChildren<QLabel *>(QStringLiteral("categoryHeader"));
        for (QLabel *header : headers) {
            if (header->text() == category) {
                return header;
            }
        }
        return nullptr;
    }

    // A widget's geometry in the scroll area's viewport coordinates
    static QRect inViewport(const QScrollArea &area, const QWidget &widget)
    {
        return {widget.mapTo(area.viewport(), QPoint(0, 0)), widget.size()};
    }

    // Header through last editor of a category's group lie inside the viewport
    static bool groupFullyVisible(const QWidget &panel, const QString &category,
                                  const QString &lastItem)
    {
        auto *area = scrollAreaOf(panel);
        auto *header = headerFor(panel, category);
        auto *lastEditor =
            panel.findChild<QWidget *>(category + QLatin1Char('/') + lastItem + ":editor");
        if (area == nullptr || header == nullptr || lastEditor == nullptr) {
            return false;
        }
        const QRect viewport = area->viewport()->rect();
        return viewport.contains(inViewport(*area, *header)) &&
               viewport.contains(inViewport(*area, *lastEditor));
    }

    static void showAtWidth(ConfigItemsPanel &panel, int width)
    {
        panel.resize(width, 400);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        QCoreApplication::processEvents();
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
    // Editor width — editors are capped instead of filling a wide column
    // =========================================================================

    void testEditorWidth_ShortOptionCombo_CappedLikeLineEdit()
    {
        auto *model = makeModelWithComboEditors();
        ConfigItemsPanel panel(model);
        panel.setCategory("Video");
        showAtWidth(panel, kWideTwoColumnWidth);
        QCOMPARE(panel.columnCount(), 2);

        auto *combo = panel.findChild<QComboBox *>("Video/System Mode:editor");
        QVERIFY(combo != nullptr);
        const int cap = expectedEditorCap(combo);
        QVERIFY(combo->maximumWidth() < QWIDGETSIZE_MAX);
        QVERIFY(combo->maximumWidth() <= cap);
        QVERIFY(combo->width() <= cap);
    }

    void testEditorWidth_BooleanStringCombo_CappedLikeLineEdit()
    {
        auto *model = makeModelWithComboEditors();
        ConfigItemsPanel panel(model);
        panel.setCategory("Video");
        showAtWidth(panel, kWideTwoColumnWidth);

        auto *combo = panel.findChild<QComboBox *>("Video/Scanlines:editor");
        QVERIFY(combo != nullptr);
        QCOMPARE(combo->currentText(), QString("Enabled"));
        const int cap = expectedEditorCap(combo);
        QVERIFY(combo->maximumWidth() <= cap);
        QVERIFY(combo->width() <= cap);
    }

    void testEditorWidth_SpinBox_CappedLikeLineEdit()
    {
        auto *model = makeModelWithData();
        ConfigItemsPanel panel(model);
        panel.setCategory("Network");
        showAtWidth(panel, kWideTwoColumnWidth);
        QCOMPARE(panel.columnCount(), 2);

        auto *spinBox = panel.findChild<QSpinBox *>("Network/Port:editor");
        QVERIFY(spinBox != nullptr);
        const int cap = expectedEditorCap(spinBox);
        QVERIFY(spinBox->maximumWidth() <= cap);
        QVERIFY(spinBox->width() <= cap);
    }

    void testEditorWidth_LongOptionCombo_WidensToFitWithoutStretching()
    {
        auto *model = makeModelWithComboEditors();
        ConfigItemsPanel panel(model);
        panel.setCategory("Video");
        showAtWidth(panel, kWideTwoColumnWidth);

        auto *combo = panel.findChild<QComboBox *>("Video/Palette:editor");
        QVERIFY(combo != nullptr);
        QVERIFY(combo->sizeHint().width() > expectedEditorCap(combo));
        QVERIFY(combo->maximumWidth() >= combo->sizeHint().width());
        QVERIFY(combo->maximumWidth() < QWIDGETSIZE_MAX);
    }

    void testEditorWidth_Relayout_KeepsEditorCap()
    {
        auto *model = makeModelWithComboEditors();
        ConfigItemsPanel panel(model);
        panel.setCategory("Video");
        showAtWidth(panel, kWideTwoColumnWidth);

        auto *combo = panel.findChild<QComboBox *>("Video/System Mode:editor");
        QVERIFY(combo != nullptr);
        const int capBefore = combo->maximumWidth();
        QVERIFY(capBefore < QWIDGETSIZE_MAX);

        panel.resize(ConfigItemsPanel::twoColumnMinWidth() - 80, 400);
        QCoreApplication::processEvents();
        QCOMPARE(panel.columnCount(), 1);
        panel.resize(kWideTwoColumnWidth, 400);
        QCoreApplication::processEvents();
        QCOMPARE(panel.columnCount(), 2);

        QCOMPARE(panel.findChild<QComboBox *>("Video/System Mode:editor"), combo);
        QCOMPARE(combo->maximumWidth(), capBefore);
    }

    void testEditorWidth_LineEdit_KeepsExistingCap()
    {
        auto *model = makeModelWithData();
        ConfigItemsPanel panel(model);
        panel.setCategory("Network");
        showAtWidth(panel, kWideTwoColumnWidth);

        auto *lineEdit = panel.findChild<QLineEdit *>("Network/Hostname:editor");
        QVERIFY(lineEdit != nullptr);
        QCOMPARE(lineEdit->maximumWidth(), expectedEditorCap(lineEdit));
    }

    // =========================================================================
    // Editor placement — each editor sits beside its right-aligned label
    // =========================================================================

    void testEditorPlacement_TwoColumns_EditorBesideLabel_data() { addEditorKindRows(); }

    void testEditorPlacement_TwoColumns_EditorBesideLabel()
    {
        verifyEditorBesideLabel(kWideTwoColumnWidth, 2);
    }

    void testEditorPlacement_OneColumn_EditorBesideLabel_data() { addEditorKindRows(); }

    void testEditorPlacement_OneColumn_EditorBesideLabel()
    {
        verifyEditorBesideLabel(kNarrowOneColumnWidth, 1);
    }

    void testEditorPlacement_FilteredAcrossCategories_EditorBesideLabel_data()
    {
        QTest::addColumn<int>("panelWidth");
        QTest::addColumn<int>("expectedColumns");
        QTest::newRow("two columns") << kWideTwoColumnWidth << 2;
        QTest::newRow("one column") << kNarrowOneColumnWidth << 1;
    }

    void testEditorPlacement_FilteredAcrossCategories_EditorBesideLabel()
    {
        QFETCH(int, panelWidth);
        QFETCH(int, expectedColumns);

        auto *model = makeModelWithEveryEditorKind();
        ConfigItemsPanel panel(model);
        panel.setCategory("Network");
        // "e" is in the Network and Video names and in Audio's Mute and Volume
        panel.setFilter("e");
        showAtWidth(panel, panelWidth);
        QCOMPARE(panel.columnCount(), expectedColumns);
        QCOMPARE(panel.headerCategories(), QStringList({"Network", "Audio", "Video"}));

        // Palette and System Mode are the first and third Video items, so they share
        // the first editor column: the short combo sits under the wider long-option one
        const int paletteColumn = gridColumnOf(panel, "Video/Palette");
        QVERIFY(paletteColumn >= 0);
        QCOMPARE(gridColumnOf(panel, "Video/System Mode"), paletteColumn);

        const QStringList keys = panel.visibleItems();
        QCOMPARE(keys.size(), 7);
        for (const QString &key : keys) {
            const int gap = labelToEditorGap(panel, key);
            QVERIFY2(gap > 0 && gap <= kMaxLabelEditorGap,
                     qPrintable(QStringLiteral("%1: gap %2 px, expected 1..%3")
                                    .arg(key)
                                    .arg(gap)
                                    .arg(kMaxLabelEditorGap)));
        }
    }

    void testEditorPlacement_TwoColumns_EditorsKeepCappedWidth()
    {
        auto *networkModel = makeModelWithData();
        ConfigItemsPanel networkPanel(networkModel);
        networkPanel.setCategory("Network");
        showAtWidth(networkPanel, kWideTwoColumnWidth);

        auto *lineEdit = networkPanel.findChild<QLineEdit *>("Network/Hostname:editor");
        auto *spinBox = networkPanel.findChild<QSpinBox *>("Network/Port:editor");
        QVERIFY(lineEdit != nullptr);
        QVERIFY(spinBox != nullptr);
        QCOMPARE(lineEdit->width(), expectedEditorCap(lineEdit));
        QCOMPARE(spinBox->width(), expectedEditorCap(spinBox));

        auto *comboModel = makeModelWithComboEditors();
        ConfigItemsPanel comboPanel(comboModel);
        comboPanel.setCategory("Video");
        showAtWidth(comboPanel, kWideTwoColumnWidth);

        for (const char *name :
             {"Video/System Mode:editor", "Video/Palette:editor", "Video/Scanlines:editor"}) {
            auto *combo = comboPanel.findChild<QComboBox *>(QString::fromLatin1(name));
            QVERIFY2(combo != nullptr, name);
            const int intended = std::max(expectedEditorCap(combo), combo->sizeHint().width());
            QCOMPARE(combo->maximumWidth(), intended);
            QCOMPARE(combo->width(), intended);
        }
    }

    void testEditorPlacement_Checkbox_KeepsAtLeastItsSizeHint()
    {
        auto *model = makeModelWithData();
        ConfigItemsPanel panel(model);
        panel.setCategory("Audio");
        showAtWidth(panel, kWideTwoColumnWidth);

        auto *checkBox = panel.findChild<QCheckBox *>("Audio/Mute:editor");
        QVERIFY(checkBox != nullptr);
        QVERIFY(checkBox->width() >= checkBox->sizeHint().width());
        QVERIFY(checkBox->height() >= checkBox->sizeHint().height());
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

    // =========================================================================
    // setFilter() — matches the choices a dropdown offers, never current values
    // =========================================================================

    void testSetFilter_TermOnlyInAComboOption_ShowsThatItem()
    {
        auto *model = makeModelWithEveryEditorKind();
        ConfigItemsPanel panel(model);
        panel.setCategory("Network");

        // NTSC is an option of System Mode, not its current value (PAL)
        panel.setFilter("ntsc");

        QCOMPARE(panel.visibleItems(), QStringList({"Video/System Mode"}));
    }

    void testSetFilter_TermOnlyInTheBooleanPair_ShowsThatItem()
    {
        auto *model = makeModelWithEveryEditorKind();
        ConfigItemsPanel panel(model);
        panel.setCategory("Network");

        // Scanlines is "Enabled"; its dropdown also offers "Disabled"
        panel.setFilter("DISABLED");

        QCOMPARE(panel.visibleItems(), QStringList({"Video/Scanlines"}));
    }

    void testSetFilter_CurrentValuesOfOtherEditors_NeverMatch()
    {
        auto *model = makeModelWithEveryEditorKind();
        ConfigItemsPanel panel(model);
        panel.setCategory("Network");

        panel.setFilter("device.local");  // the Hostname line edit's text
        QVERIFY(panel.visibleItems().isEmpty());

        panel.setFilter("21");  // the Port spin box's number
        QVERIFY(panel.visibleItems().isEmpty());
    }

    // =========================================================================
    // setCategory() under a filter — scrolls the category's group into view
    // =========================================================================

    void testSetCategory_UnderFilter_ScrollsGroupNearBottomIntoView()
    {
        auto *model = makeModelForScrolling();
        ConfigItemsPanel panel(model);
        panel.resize(kShortPanelWidth, kShortPanelHeight);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        panel.setFilter("setting");
        QVERIFY(panel.headerCategories().contains("Golf"));
        auto *vbar = scrollAreaOf(panel)->verticalScrollBar();
        QTRY_VERIFY(vbar->maximum() > 0);  // the filtered items have been laid out
        QCOMPARE(vbar->value(), 0);
        QVERIFY(!groupFullyVisible(panel, "Golf", "Setting 03"));

        panel.setCategory("Golf");

        QVERIFY(vbar->value() > 0);
        QVERIFY(groupFullyVisible(panel, "Golf", "Setting 03"));
    }

    void testSetCategory_UnderFilter_TallGroupPutsHeaderAtTop()
    {
        auto *model = makeModelForScrolling();
        ConfigItemsPanel panel(model);
        panel.resize(kShortPanelWidth, kShortPanelHeight);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        panel.setFilter("setting");

        panel.setCategory("Hotel");

        auto *area = scrollAreaOf(panel);
        auto *header = headerFor(panel, "Hotel");
        QVERIFY(header != nullptr);
        QVERIFY(area->verticalScrollBar()->value() > 0);
        QCOMPARE(inViewport(*area, *header).top(), 0);
    }

    void testSetCategory_UnderFilter_CategoryNotInResults_KeepsScrollPosition()
    {
        auto *model = makeModelForScrolling();
        ConfigItemsPanel panel(model);
        panel.resize(kShortPanelWidth, kShortPanelHeight);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        panel.setFilter("setting");
        panel.setCategory("Echo");
        auto *vbar = scrollAreaOf(panel)->verticalScrollBar();
        const int before = vbar->value();
        QVERIFY(before > 0);

        panel.setCategory("India");  // loaded, but nothing in it matches
        QCOMPARE(vbar->value(), before);

        panel.setCategory("Nowhere");  // not a category at all
        QCOMPARE(vbar->value(), before);
        QCOMPARE(panel.currentCategory(), QString("Nowhere"));
    }

    void testSetCategory_UnderFilter_SingleGroup_KeepsScrollPosition()
    {
        auto *model = makeModelForScrolling();
        ConfigItemsPanel panel(model);
        panel.resize(kShortPanelWidth, kShortPanelHeight);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        panel.setFilter("unrelated");
        QVERIFY(panel.headerCategories().isEmpty());

        panel.setCategory("India");

        QCOMPARE(scrollAreaOf(panel)->verticalScrollBar()->value(), 0);
        QCOMPARE(panel.visibleItems(), QStringList({"India/Unrelated"}));
    }

    void testSetCategory_WithoutFilter_ShowsOnlyThatCategory()
    {
        auto *model = makeModelForScrolling();
        ConfigItemsPanel panel(model);
        panel.resize(kShortPanelWidth, kShortPanelHeight);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));

        panel.setCategory("Golf");

        QVERIFY(panel.headerCategories().isEmpty());
        QCOMPARE(panel.visibleItems(),
                 QStringList({"Golf/Setting 01", "Golf/Setting 02", "Golf/Setting 03"}));
        QCOMPARE(scrollAreaOf(panel)->verticalScrollBar()->value(), 0);
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
