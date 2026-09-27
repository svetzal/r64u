/**
 * @file configfiltercore.cpp
 * @brief Implementation of the pure Config mode filter and layout rules.
 */

#include "configfiltercore.h"

namespace configfiltercore {

QString normalizeFilter(const QString &text)
{
    return text.trimmed();
}

bool isFilterActive(const QString &text)
{
    return !normalizeFilter(text).isEmpty();
}

bool matches(const QString &category, const QString &item, const QString &filter)
{
    const QString needle = normalizeFilter(filter);
    if (needle.isEmpty()) {
        return true;
    }
    return item.contains(needle, Qt::CaseInsensitive) ||
           category.contains(needle, Qt::CaseInsensitive);
}

QList<CategoryItems> filterSnapshot(const QList<CategoryItems> &snapshot, const QString &filter)
{
    const bool active = isFilterActive(filter);
    QList<CategoryItems> result;
    for (const CategoryItems &group : snapshot) {
        CategoryItems kept;
        kept.category = group.category;
        for (const QString &item : group.items) {
            if (matches(group.category, item, filter)) {
                kept.items.append(item);
            }
        }
        if (active && kept.items.isEmpty()) {
            continue;
        }
        kept.items.sort(Qt::CaseInsensitive);
        result.append(kept);
    }
    return result;
}

QStringList visibleCategories(const QList<CategoryItems> &snapshot, const QString &filter)
{
    QStringList names;
    if (!isFilterActive(filter)) {
        for (const CategoryItems &group : snapshot) {
            names.append(group.category);
        }
        return names;
    }
    for (const CategoryItems &group : filterSnapshot(snapshot, filter)) {
        names.append(group.category);
    }
    return names;
}

int columnCountForWidth(int width, int threshold)
{
    if (width <= 0 || width < threshold) {
        return 1;
    }
    return 2;
}

QString itemKey(const QString &category, const QString &item)
{
    return category + QLatin1Char('/') + item;
}

}  // namespace configfiltercore
