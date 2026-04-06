/**
 * @file favoritesuicore.h
 * @brief Pure core functions for favorites UI display logic.
 *
 * All functions are pure: they take immutable input and return results
 * with no side effects.
 */
#ifndef FAVORITESUICORE_H
#define FAVORITESUICORE_H

#include <QString>
#include <QStringList>

namespace favoritesui {

/**
 * @brief A single entry for display in the favorites menu.
 */
struct FavoriteEntry
{
    QString displayName;  ///< Short name for menu item label
    QString path;         ///< Full path (for navigation and tooltip)
};

/**
 * @brief Derives the display name for a path.
 *
 * Returns the last path component (after the final '/').
 * Returns "/" for the root path or any path with no components.
 *
 * @param path A remote file path.
 * @return Short display name.
 */
[[nodiscard]] QString displayNameForPath(const QString &path);

/**
 * @brief Builds the favorites menu entries from a list of paths.
 *
 * @param favorites List of favorite paths.
 * @return List of FavoriteEntry values ready for menu population.
 *         Returns a single disabled placeholder entry if favorites is empty.
 */
[[nodiscard]] QList<FavoriteEntry> buildMenuEntries(const QStringList &favorites);

/**
 * @brief Checks whether a path is in the favorites list.
 * @param path The path to check.
 * @param favorites Current favorites list.
 * @return True if path is in favorites.
 */
[[nodiscard]] bool isFavorite(const QString &path, const QStringList &favorites);

}  // namespace favoritesui

#endif  // FAVORITESUICORE_H
