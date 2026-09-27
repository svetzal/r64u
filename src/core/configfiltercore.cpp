/**
 * @file configfiltercore.cpp
 * @brief Implementation of the pure Config mode filter and layout rules.
 */

#include "configfiltercore.h"

#include <algorithm>
#include <array>
#include <utility>

namespace configfiltercore {

namespace {

// The dropdown pairs offered for string values that read as booleans
const std::array<std::pair<const char *, const char *>, 4> kBooleanPairs = {{
    {"Yes", "No"},
    {"Enabled", "Disabled"},
    {"On", "Off"},
    {"True", "False"},
}};

bool hasOwnEditor(const QVariant &value)
{
    switch (value.typeId()) {
    case QMetaType::Bool:
    case QMetaType::Int:
    case QMetaType::LongLong:
    case QMetaType::Double:
        return true;
    default:
        return false;
    }
}

}  // namespace

QString normalizeFilter(const QString &text)
{
    return text.trimmed();
}

bool isFilterActive(const QString &text)
{
    return !normalizeFilter(text).isEmpty();
}

bool matches(const QString &category, const QString &item, const QStringList &choices,
             const QString &filter)
{
    const QString needle = normalizeFilter(filter);
    if (needle.isEmpty()) {
        return true;
    }
    if (item.contains(needle, Qt::CaseInsensitive) ||
        category.contains(needle, Qt::CaseInsensitive)) {
        return true;
    }
    return std::any_of(choices.cbegin(), choices.cend(), [&needle](const QString &choice) {
        return choice.contains(needle, Qt::CaseInsensitive);
    });
}

QStringList editorChoices(const QVariant &value, const QStringList &options)
{
    if (!options.isEmpty()) {
        return options;
    }
    if (hasOwnEditor(value)) {
        return {};
    }
    const QString text = value.toString();
    for (const auto &[first, second] : kBooleanPairs) {
        const QString on = QString::fromLatin1(first);
        const QString off = QString::fromLatin1(second);
        if (text.compare(on, Qt::CaseInsensitive) == 0 ||
            text.compare(off, Qt::CaseInsensitive) == 0) {
            return {on, off};
        }
    }
    return {};
}

QList<CategoryItems> filterSnapshot(const QList<CategoryItems> &snapshot, const QString &filter)
{
    const bool active = isFilterActive(filter);
    QList<CategoryItems> result;
    for (const CategoryItems &group : snapshot) {
        CategoryItems kept;
        kept.category = group.category;
        for (const QString &item : group.items) {
            const QStringList choices = group.choices.value(item);
            if (matches(group.category, item, choices, filter)) {
                kept.items.append(item);
                if (!choices.isEmpty()) {
                    kept.choices.insert(item, choices);
                }
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

int scrollValueToReveal(int value, int viewportHeight, int top, int bottom)
{
    if (bottom - top > viewportHeight || top < value) {
        return top;
    }
    if (bottom > value + viewportHeight) {
        return bottom - viewportHeight;
    }
    return value;
}

QString itemKey(const QString &category, const QString &item)
{
    return category + QLatin1Char('/') + item;
}

}  // namespace configfiltercore
