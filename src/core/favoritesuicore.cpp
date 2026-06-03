/**
 * @file favoritesuicore.cpp
 * @brief Implementation of pure favorites UI display logic functions.
 */

#include "favoritesuicore.h"

namespace favoritesui {

QString displayNameForPath(const QString &path)
{
    int lastSlash = path.lastIndexOf('/');
    if (lastSlash < 0) {
        // No slash found — return the path itself, or "/" if it is empty
        return path.isEmpty() ? QStringLiteral("/") : path;
    }

    QString name = path.mid(lastSlash + 1);
    if (name.isEmpty()) {
        return QStringLiteral("/");
    }
    return name;
}

QList<FavoriteEntry> buildMenuEntries(const QStringList &favorites)
{
    if (favorites.isEmpty()) {
        return {{QStringLiteral("(No favorites)"), QString()}};
    }

    QList<FavoriteEntry> entries;
    entries.reserve(favorites.size());
    for (const QString &path : favorites) {
        entries.append(FavoriteEntry{displayNameForPath(path), path});
    }
    return entries;
}

bool isFavorite(const QString &path, const QStringList &favorites)
{
    return favorites.contains(path);
}

}  // namespace favoritesui
