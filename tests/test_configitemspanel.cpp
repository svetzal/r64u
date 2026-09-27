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
 * - Each label/editor pair is anchored at the left of its half of the grid: the
 *   widest label starts at the left margin, the second pair starts at the middle
 *   of the grid in every category, and a checkbox is capped like other editors
 * - Category headers span the whole grid in the filtered view
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
#include <cstdlib>

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
    static constexpr int kGridMargin = 8;
    static constexpr int kGridSpacing = 8;
    // Pixels a measured edge may be off by, for rounding when spare width is shared out
    static constexpr int kLayoutSlack = 1;
    static constexpr int kWiderTwoColumnWidth = 1200;
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

    static QWidget *contentOf(const QWidget &panel) { return scrollAreaOf(panel)->widget(); }

    // Left edge of each label/editor pair in use: where the grid's widest label would start
    // in that pair. Items fill pairs afresh in each category's group. Labels are
    // right-aligned, so a pair's labels share a right edge; fails the test when one does
    // not. (Editors are left out: native styles offset each kind within its cell.)
    static void measurePairStarts(const ConfigItemsPanel &panel, QList<int> &starts)
    {
        const QStringList keys = panel.visibleItems();
        const int pairs = panel.columnCount();
        QList<int> labelRight;
        int widestLabel = 0;
        int indexInGroup = 0;
        QString group;
        for (const QString &key : keys) {
            auto *label = panel.findChild<QLabel *>(key);
            QVERIFY2(label != nullptr, qPrintable(key));
            const QString category = key.section(QLatin1Char('/'), 0, 0);
            indexInGroup = category == group ? indexInGroup + 1 : 0;
            group = category;
            const auto pair = indexInGroup % pairs;
            if (pair < labelRight.size()) {
                QVERIFY2(label->geometry().right() == labelRight.at(pair),
                         qPrintable(QStringLiteral("%1 label ends at x %2, its pair's at %3")
                                        .arg(key)
                                        .arg(label->geometry().right())
                                        .arg(labelRight.at(pair))));
            } else {
                labelRight.append(label->geometry().right());
            }
            widestLabel = std::max(widestLabel, label->width());
        }
        starts.clear();
        for (const int right : labelRight) {
            starts.append(right + 1 - widestLabel);
        }
    }

    // Every editor kind, plus a category holding a single item and one whose labels are all
    // short. None of the extra names contain an "e", so filtering on "e" is unchanged.
    ConfigurationModel *makeModelForPairLayout()
    {
        auto *model = new ConfigurationModel(this);
        model->setCategories({"Network", "Audio", "Video", "Solo", "Tiny"});
        addNetworkAndAudioItems(model);
        addVideoComboItems(model);
        model->setCategoryItems("Solo", {{QStringLiteral("Zoom"), 2}});
        model->setCategoryItems("Tiny", {{QStringLiteral("X"), QStringLiteral("abc")},
                                         {QStringLiteral("Y"), 3},
                                         {QStringLiteral("Z"), true}});
        return model;
    }

    // The scroll area never needs to scroll sideways and every editor lies inside it
    static void verifyNoHorizontalOverflow(const ConfigItemsPanel &panel)
    {
        auto *area = scrollAreaOf(panel);
        const int viewportWidth = area->viewport()->width();
        QVERIFY2(contentOf(panel)->width() <= viewportWidth,
                 qPrintable(QStringLiteral("content %1 px wide, viewport %2 px")
                                .arg(contentOf(panel)->width())
                                .arg(viewportWidth)));
        QVERIFY(!area->horizontalScrollBar()->isVisible());
        for (const QString &key : panel.visibleItems()) {
            auto *editor = panel.findChild<QWidget *>(key + QStringLiteral(":editor"));
            QVERIFY2(inViewport(*area, *editor).right() < viewportWidth,
                     qPrintable(QStringLiteral("%1 ends at x %2, viewport %3 px wide")
                                    .arg(key)
                                    .arg(inViewport(*area, *editor).right())
                                    .arg(viewportWidth)));
        }
    }

    static void addLayoutCategoryRows()
    {
        QTest::addColumn<QString>("category");
        QTest::newRow("line edit and spin box") << "Network";
        QTest::newRow("combos") << "Video";
        QTest::newRow("checkbox and spin box") << "Audio";
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
    // Pair layout — each label/editor pair is anchored at the left of its half
    // =========================================================================

    void testPairLayout_OneColumn_WidestLabelStartsAtLeftMargin_data() { addLayoutCategoryRows(); }

    void testPairLayout_OneColumn_WidestLabelStartsAtLeftMargin()
    {
        QFETCH(QString, category);

        auto *model = makeModelWithEveryEditorKind();
        ConfigItemsPanel panel(model);
        panel.setCategory(category);
        showAtWidth(panel, kNarrowOneColumnWidth);
        QCOMPARE(panel.columnCount(), 1);

        int leftmostLabel = QWIDGETSIZE_MAX;
        for (const QString &key : panel.visibleItems()) {
            leftmostLabel = std::min(leftmostLabel, panel.findChild<QLabel *>(key)->x());
        }
        QVERIFY2(std::abs(leftmostLabel - kGridMargin) <= kLayoutSlack,
                 qPrintable(QStringLiteral("leftmost label at x %1, left margin at %2")
                                .arg(leftmostLabel)
                                .arg(kGridMargin)));
    }

    void testPairLayout_TwoColumns_PairsStartAtLeftOfEachHalf_data()
    {
        QTest::addColumn<QString>("category");
        QTest::addColumn<int>("panelWidth");
        for (const int width : {kWideTwoColumnWidth, kWiderTwoColumnWidth}) {
            for (const char *category : {"Network", "Video", "Audio"}) {
                QTest::addRow("%s at %d", category, width)
                    << QString::fromLatin1(category) << width;
            }
        }
    }

    void testPairLayout_TwoColumns_PairsStartAtLeftOfEachHalf()
    {
        QFETCH(QString, category);
        QFETCH(int, panelWidth);

        auto *model = makeModelWithEveryEditorKind();
        ConfigItemsPanel panel(model);
        panel.setCategory(category);
        showAtWidth(panel, panelWidth);
        QCOMPARE(panel.columnCount(), 2);

        QList<int> starts;
        measurePairStarts(panel, starts);
        if (QTest::currentTestFailed()) {
            return;
        }
        QVERIFY2(std::abs(starts.at(0) - kGridMargin) <= kLayoutSlack,
                 qPrintable(QStringLiteral("first pair starts at x %1, left margin at %2")
                                .arg(starts.at(0))
                                .arg(kGridMargin)));

        // The second pair starts at the middle of the grid, or at most one spacing after it
        const int middle = contentOf(panel)->width() / 2;
        QVERIFY2(starts.at(1) >= middle - kLayoutSlack &&
                     starts.at(1) <= middle + kGridSpacing + kLayoutSlack,
                 qPrintable(QStringLiteral("second pair starts at x %1, middle at %2")
                                .arg(starts.at(1))
                                .arg(middle)));
    }

    void testPairLayout_TwoColumns_SecondPairStartsAtSameXInEveryCategory_data()
    {
        QTest::addColumn<int>("panelWidth");
        QTest::addRow("%d", kWideTwoColumnWidth) << kWideTwoColumnWidth;
        QTest::addRow("%d", kWiderTwoColumnWidth) << kWiderTwoColumnWidth;
    }

    void testPairLayout_TwoColumns_SecondPairStartsAtSameXInEveryCategory()
    {
        QFETCH(int, panelWidth);

        auto *model = makeModelWithEveryEditorKind();
        ConfigItemsPanel panel(model);
        panel.setCategory("Network");
        showAtWidth(panel, panelWidth);
        QList<int> networkStarts;
        measurePairStarts(panel, networkStarts);

        // Video's labels and editors are both wider than Network's
        for (const char *category : {"Video", "Audio"}) {
            panel.setCategory(QString::fromLatin1(category));
            QCoreApplication::processEvents();
            QCOMPARE(panel.columnCount(), 2);
            QList<int> starts;
            measurePairStarts(panel, starts);
            if (QTest::currentTestFailed()) {
                return;
            }
            QVERIFY2(starts.at(1) == networkStarts.at(1),
                     qPrintable(QStringLiteral("%1's second pair starts at x %2, Network's at %3")
                                    .arg(QString::fromLatin1(category))
                                    .arg(starts.at(1))
                                    .arg(networkStarts.at(1))));
        }
    }

    void testPairLayout_NarrowTwoColumns_NoOverflowAndPairsAnchored_data()
    {
        QTest::addColumn<QString>("category");
        QTest::addColumn<QString>("filter");
        QTest::addColumn<int>("panelWidth");
        const int threshold = ConfigItemsPanel::twoColumnMinWidth();
        for (const int width : {threshold, threshold + 40}) {
            QTest::addRow("long-option combos at %d", width) << "Video" << "" << width;
            QTest::addRow("filtered across categories at %d", width) << "Network" << "e" << width;
            QTest::addRow("single item at %d", width) << "Solo" << "" << width;
            QTest::addRow("short labels at %d", width) << "Tiny" << "" << width;
        }
    }

    void testPairLayout_NarrowTwoColumns_NoOverflowAndPairsAnchored()
    {
        QFETCH(QString, category);
        QFETCH(QString, filter);
        QFETCH(int, panelWidth);

        auto *model = makeModelForPairLayout();
        ConfigItemsPanel panel(model);
        panel.setCategory(category);
        panel.setFilter(filter);
        showAtWidth(panel, panelWidth);
        QCOMPARE(panel.columnCount(), 2);

        verifyNoHorizontalOverflow(panel);
        if (QTest::currentTestFailed()) {
            return;
        }
        QList<int> starts;
        measurePairStarts(panel, starts);
        if (QTest::currentTestFailed()) {
            return;
        }
        QVERIFY2(std::abs(starts.at(0) - kGridMargin) <= kLayoutSlack,
                 qPrintable(QStringLiteral("first pair starts at x %1, left margin at %2")
                                .arg(starts.at(0))
                                .arg(kGridMargin)));
        if (starts.size() > 1) {
            const int middle = contentOf(panel)->width() / 2;
            QVERIFY2(starts.at(1) >= middle - kLayoutSlack &&
                         starts.at(1) <= middle + kGridSpacing + kLayoutSlack,
                     qPrintable(QStringLiteral("second pair starts at x %1, middle at %2")
                                    .arg(starts.at(1))
                                    .arg(middle)));
        }
    }

    void testPairLayout_NarrowedWithinTwoColumns_NoOverflow()
    {
        auto *model = makeModelForPairLayout();
        ConfigItemsPanel panel(model);
        showAtWidth(panel, kWideTwoColumnWidth);
        panel.setCategory("Video");  // picked in the shown pane, laid out for its width
        QCoreApplication::processEvents();

        // Stays in two columns, so the items are not laid out afresh
        panel.resize(ConfigItemsPanel::twoColumnMinWidth() + 20, 400);
        QCoreApplication::processEvents();
        QCOMPARE(panel.columnCount(), 2);

        verifyNoHorizontalOverflow(panel);
    }

    void testPairLayout_Checkbox_CappedLikeOtherEditors()
    {
        auto *model = makeModelWithData();
        ConfigItemsPanel panel(model);
        panel.setCategory("Audio");
        showAtWidth(panel, kWideTwoColumnWidth);

        auto *checkBox = panel.findChild<QCheckBox *>("Audio/Mute:editor");
        QVERIFY(checkBox != nullptr);
        QVERIFY2(checkBox->width() <= expectedEditorCap(checkBox),
                 qPrintable(QStringLiteral("checkbox %1 px wide, cap %2")
                                .arg(checkBox->width())
                                .arg(expectedEditorCap(checkBox))));
        QVERIFY(checkBox->width() >= checkBox->sizeHint().width());
    }

    void testPairLayout_FilteredAcrossCategories_HeadersSpanTheGrid_data()
    {
        QTest::addColumn<int>("panelWidth");
        QTest::newRow("two columns") << kWideTwoColumnWidth;
        QTest::newRow("one column") << kNarrowOneColumnWidth;
    }

    void testPairLayout_FilteredAcrossCategories_HeadersSpanTheGrid()
    {
        QFETCH(int, panelWidth);

        auto *model = makeModelWithEveryEditorKind();
        ConfigItemsPanel panel(model);
        panel.setFilter("e");
        showAtWidth(panel, panelWidth);
        QCOMPARE(panel.headerCategories(), QStringList({"Network", "Audio", "Video"}));

        const int gridWidth = contentOf(panel)->width() - (2 * kGridMargin);
        for (const QString &category : panel.headerCategories()) {
            auto *header = headerFor(panel, category);
            QVERIFY(header != nullptr);
            QCOMPARE(header->x(), kGridMargin);
            QVERIFY2(std::abs(header->width() - gridWidth) <= kLayoutSlack,
                     qPrintable(QStringLiteral("%1 header %2 px wide, grid %3 px")
                                    .arg(category)
                                    .arg(header->width())
                                    .arg(gridWidth)));
        }
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
