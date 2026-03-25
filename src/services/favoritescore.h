/**
 * @file favoritescore.h
 * @brief Pure core functions for favorites management logic.
 *
 * All functions in this namespace are pure: they take immutable input and return
 * new output with no side effects. This enables comprehensive unit testing and
 * clean separation from I/O concerns (QSettings persistence).
 */

#ifndef FAVORITESCORE_H
#define FAVORITESCORE_H

#include <QStringList>

namespace favorites {

/**
 * @brief Result of adding a favorite path.
 */
struct AddResult
{
    QStringList paths;   ///< Updated favorites list (unsorted, in insertion order)
    bool added = false;  ///< True if the path was actually added (was not already present)
};

/**
 * @brief Result of removing a favorite path.
 */
struct RemoveResult
{
    QStringList paths;     ///< Updated favorites list
    bool removed = false;  ///< True if the path was actually removed (was present)
};

/**
 * @brief Returns the list sorted case-insensitively.
 * @param paths List to sort.
 * @return New sorted list.
 */
[[nodiscard]] QStringList sorted(const QStringList &paths);

/**
 * @brief Adds a path to the favorites list if not already present.
 * @param paths Current favorites list.
 * @param path Path to add.
 * @return AddResult with updated list and whether the path was newly added.
 *         Empty paths are ignored (returns AddResult with added=false).
 */
[[nodiscard]] AddResult addFavorite(const QStringList &paths, const QString &path);

/**
 * @brief Removes a path from the favorites list.
 * @param paths Current favorites list.
 * @param path Path to remove.
 * @return RemoveResult with updated list and whether the path was present.
 */
[[nodiscard]] RemoveResult removeFavorite(const QStringList &paths, const QString &path);

/**
 * @brief Moves a favorite from one position to another.
 * @param paths Current favorites list.
 * @param from Source index.
 * @param to Destination index.
 * @return New list after the move, or unchanged list if indices are invalid.
 */
[[nodiscard]] QStringList moveFavorite(const QStringList &paths, int from, int to);

}  // namespace favorites

#endif  // FAVORITESCORE_H
