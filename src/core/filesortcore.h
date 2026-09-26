#ifndef FILESORTCORE_H
#define FILESORTCORE_H

#include <QString>
#include <Qt>

/**
 * @namespace filesort
 * @brief The one ordering rule for a file listing, shared by the local and remote browsers.
 *
 * Directories always come before files, whatever the column or direction.
 * Within each group the entries are ordered by the chosen key; ties (two files
 * of the same size, say) fall back to the name so the order is stable.
 */
namespace filesort {

/// Which column's value orders the listing.
enum class Key { Name, Size, Type };

/// A key and a direction, as a view header reports them.
struct Spec
{
    Key key = Key::Name;
    Qt::SortOrder order = Qt::AscendingOrder;
};

/// The attributes of one listed entry that the ordering looks at.
struct Entry
{
    QString name;
    bool isDirectory = false;
    qint64 size = 0;   ///< Raw bytes: "916 bytes" must sort below "26.9 KB"
    QString typeName;  ///< The Type column's text
};

/// Maps a listing's column (Name, Size, Type) to its key; any other column orders by name.
[[nodiscard]] inline Key keyForColumn(int column)
{
    switch (column) {
    case 1:
        return Key::Size;
    case 2:
        return Key::Type;
    default:
        return Key::Name;
    }
}

/// Compares two names the way the listing shows them: case-insensitively.
[[nodiscard]] inline int compareNames(const Entry &a, const Entry &b)
{
    return a.name.compare(b.name, Qt::CaseInsensitive);
}

/// Three-way comparison on the key alone, before direction or the name tie-break apply.
[[nodiscard]] inline int compareByKey(const Entry &a, const Entry &b, Key key)
{
    switch (key) {
    case Key::Size:
        return a.size < b.size ? -1 : (a.size > b.size ? 1 : 0);
    case Key::Type:
        return a.typeName.compare(b.typeName, Qt::CaseInsensitive);
    case Key::Name:
        break;
    }
    return compareNames(a, b);
}

/**
 * @brief True when @p a is listed before @p b under @p spec.
 *
 * Directories precede files in both directions. Within a group the key decides,
 * reversed for a descending order; equal keys keep ascending name order.
 */
[[nodiscard]] inline bool lessThan(const Entry &a, const Entry &b, const Spec &spec)
{
    if (a.isDirectory != b.isDirectory) {
        return a.isDirectory;
    }
    const int byKey = compareByKey(a, b, spec.key);
    if (byKey != 0) {
        return spec.order == Qt::AscendingOrder ? byKey < 0 : byKey > 0;
    }
    return compareNames(a, b) < 0;
}

}  // namespace filesort

#endif  // FILESORTCORE_H
