/**
 * @file favoritescore.cpp
 * @brief Implementation of pure favorites core functions.
 */

#include "favoritescore.h"

namespace favorites {

QStringList sorted(const QStringList &paths)
{
    QStringList result = paths;
    result.sort(Qt::CaseInsensitive);
    return result;
}

AddResult addFavorite(const QStringList &paths, const QString &path)
{
    if (path.isEmpty() || paths.contains(path)) {
        return {paths, false};
    }

    QStringList result = paths;
    result.append(path);
    return {result, true};
}

RemoveResult removeFavorite(const QStringList &paths, const QString &path)
{
    if (!paths.contains(path)) {
        return {paths, false};
    }

    QStringList result = paths;
    result.removeAll(path);
    return {result, true};
}

QStringList moveFavorite(const QStringList &paths, int from, int to)
{
    if (from < 0 || from >= paths.count() || to < 0 || to >= paths.count() || from == to) {
        return paths;
    }

    QStringList result = paths;
    result.move(from, to);
    return result;
}

}  // namespace favorites
