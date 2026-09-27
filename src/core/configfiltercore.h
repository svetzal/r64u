#ifndef CONFIGFILTERCORE_H
#define CONFIGFILTERCORE_H

#include <QList>
#include <QString>
#include <QStringList>

/**
 * @brief Pure filtering and layout rules for the Config mode panels.
 *
 * Owns the filter normalisation, the match rule, the grouped filter result,
 * the visible-category list, the column-count rule and the "category/item"
 * key format so that ConfigPanel and ConfigItemsPanel agree on all of them.
 * No I/O, no widgets.
 */
namespace configfiltercore {

/**
 * @brief A category and the names of the items loaded for it.
 */
struct CategoryItems
{
    QString category;   ///< Category name as reported by the device
    QStringList items;  ///< Loaded item names (empty while the category is unloaded)

    bool operator==(const CategoryItems &other) const
    {
        return category == other.category && items == other.items;
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
 * case-insensitive substring of the item name or of the category name;
 * values are never inspected.
 *
 * @param category Category the item belongs to.
 * @param item Item name.
 * @param filter Raw filter text.
 * @return True when the item is visible under the filter.
 */
[[nodiscard]] bool matches(const QString &category, const QString &item, const QString &filter);

/**
 * @brief Narrows a snapshot of categories and items to those matching a filter.
 *
 * Category order is preserved and item names are sorted case-insensitively
 * within each group. With an inactive filter every category is returned with
 * all its items. With an active filter only groups holding at least one
 * matching item are returned, so an unloaded category contributes nothing.
 *
 * @param snapshot Categories with their loaded item names.
 * @param filter Raw filter text.
 * @return Filtered groups in display order.
 */
[[nodiscard]] QList<CategoryItems> filterSnapshot(const QList<CategoryItems> &snapshot,
                                                  const QString &filter);

/**
 * @brief Names of the categories the category list should show.
 * @param snapshot Categories with their loaded item names.
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
 * @brief Key identifying an item across categories.
 * @param category Category name.
 * @param item Item name.
 * @return "category/item".
 */
[[nodiscard]] QString itemKey(const QString &category, const QString &item);

}  // namespace configfiltercore

#endif  // CONFIGFILTERCORE_H
