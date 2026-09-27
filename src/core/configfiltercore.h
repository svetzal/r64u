#ifndef CONFIGFILTERCORE_H
#define CONFIGFILTERCORE_H

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>

/**
 * @brief Pure filtering and layout rules for the Config mode panels.
 *
 * Owns the filter normalisation, the choices a setting's dropdown offers, the
 * match rule, the grouped filter result, the visible-category list, the
 * column-count rule and the "category/item" key format so that ConfigPanel and
 * ConfigItemsPanel agree on all of them.
 * No I/O, no widgets.
 */
namespace configfiltercore {

/**
 * @brief A category, the names of the items loaded for it, and the choices
 *        each item's editor offers.
 */
struct CategoryItems
{
    QString category;   ///< Category name as reported by the device
    QStringList items;  ///< Loaded item names (empty while the category is unloaded)
    /// Choices each item's editor offers, keyed by item name; items without choices are absent
    QHash<QString, QStringList> choices;

    bool operator==(const CategoryItems &other) const
    {
        return category == other.category && items == other.items && choices == other.choices;
    }
    bool operator!=(const CategoryItems &other) const { return !(*this == other); }
};

/// Minimum panel width, in pixels, at which items are laid out in two columns.
constexpr int kTwoColumnMinWidth = 720;

/**
 * @brief Returns the filter text with surrounding whitespace removed.
 * @param text Raw text from the filter box.
 * @return Trimmed copy.
 */
[[nodiscard]] QString normalizeFilter(const QString &text);

/**
 * @brief Whether the filter text narrows anything.
 * @param text Raw text from the filter box.
 * @return True when the normalised text is non-empty.
 */
[[nodiscard]] bool isFilterActive(const QString &text);

/**
 * @brief Whether an item is shown under a filter.
 *
 * An inactive filter matches everything. Otherwise the filter must be a
 * case-insensitive substring of the item name, of the category name, or of
 * one of the choices the item's dropdown offers. Current values are never
 * inspected, so a spin box's number or a line edit's text never matches.
 *
 * @param category Category the item belongs to.
 * @param item Item name.
 * @param choices Choices the item's editor offers (see editorChoices()).
 * @param filter Raw filter text.
 * @return True when the item is visible under the filter.
 */
[[nodiscard]] bool matches(const QString &category, const QString &item, const QStringList &choices,
                           const QString &filter);

/**
 * @brief The choices a setting's dropdown offers, in display order.
 *
 * The device's options when there are any. Otherwise, for a value that is not
 * a bool, integer or double but whose text reads as a boolean (yes/no,
 * enabled/disabled, on/off, true/false in any case), the matching capitalised
 * pair, e.g. {"Enabled", "Disabled"}. Otherwise nothing: the setting is edited
 * with a checkbox, spin box or free-text field instead of a dropdown.
 *
 * @param value Current value of the setting.
 * @param options Options reported by the device, possibly empty.
 * @return The dropdown's entries, or an empty list when there is no dropdown.
 */
[[nodiscard]] QStringList editorChoices(const QVariant &value, const QStringList &options);

/**
 * @brief Narrows a snapshot of categories and items to those matching a filter.
 *
 * Category order is preserved and item names are sorted case-insensitively
 * within each group. With an inactive filter every category is returned with
 * all its items. With an active filter only groups holding at least one
 * matching item are returned, so an unloaded category contributes nothing.
 * Each returned group carries the choices of the items it keeps.
 *
 * @param snapshot Categories with their loaded item names and choices.
 * @param filter Raw filter text.
 * @return Filtered groups in display order.
 */
[[nodiscard]] QList<CategoryItems> filterSnapshot(const QList<CategoryItems> &snapshot,
                                                  const QString &filter);

/**
 * @brief Names of the categories the category list should show.
 * @param snapshot Categories with their loaded item names and choices.
 * @param filter Raw filter text.
 * @return Every category name in order when the filter is inactive; otherwise
 *         only the categories filterSnapshot() would return.
 */
[[nodiscard]] QStringList visibleCategories(const QList<CategoryItems> &snapshot,
                                            const QString &filter);

/**
 * @brief Number of label/editor column pairs for a panel width.
 * @param width Panel width in pixels; non-positive means "not yet laid out".
 * @param threshold Width at which two columns are used.
 * @return 1 when width is non-positive or below the threshold, otherwise 2.
 */
[[nodiscard]] int columnCountForWidth(int width, int threshold = kTwoColumnMinWidth);

/**
 * @brief Scroll position that brings a vertical span into view.
 *
 * Scrolls as little as possible so the whole span is visible. A span taller
 * than the viewport is aligned with its top at the top of the viewport.
 *
 * @param value Current scroll position (content y shown at the viewport top).
 * @param viewportHeight Visible height.
 * @param top First content row of the span.
 * @param bottom One past the last content row of the span.
 * @return The scroll position to use; callers clamp it to the scroll range.
 */
[[nodiscard]] int scrollValueToReveal(int value, int viewportHeight, int top, int bottom);

/**
 * @brief Key identifying an item across categories.
 * @param category Category name.
 * @param item Item name.
 * @return "category/item".
 */
[[nodiscard]] QString itemKey(const QString &category, const QString &item);

}  // namespace configfiltercore

#endif  // CONFIGFILTERCORE_H
