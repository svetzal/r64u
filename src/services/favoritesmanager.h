/**
 * @file favoritesmanager.h
 * @brief Service for managing user's favorite/bookmarked file paths.
 */

#ifndef FAVORITESMANAGER_H
#define FAVORITESMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>

/**
 * @brief Manages a list of favorite/bookmarked remote file paths.
 *
 * Favorites are persisted using QSettings and loaded at startup.
 * This allows users to quickly access frequently-used files and directories.
 */
class FavoritesManager : public QObject
{
    Q_OBJECT

public:
    explicit FavoritesManager(QObject *parent = nullptr);
    ~FavoritesManager() override = default;

    /**
     * @brief Returns all favorite paths.
     * @return List of favorite paths in user-defined order.
     */
    [[nodiscard]] QStringList favorites() const { return favorites_; }

    /**
     * @brief Checks if a path is in the favorites list.
     * @param path The path to check.
     * @return True if the path is a favorite.
     */
    [[nodiscard]] bool isFavorite(const QString &path) const;

    /**
     * @brief Returns the number of favorites.
     * @return The count of favorite paths.
     */
    [[nodiscard]] int count() const { return favorites_.count(); }

public slots:
    /**
     * @brief Adds a path to favorites.
     * @param path The path to add.
     *
     * Does nothing if the path is already a favorite.
     */
    void addFavorite(const QString &path);

    /**
     * @brief Removes a path from favorites.
     * @param path The path to remove.
     */
    void removeFavorite(const QString &path);

    /**
     * @brief Toggles the favorite status of a path.
     * @param path The path to toggle.
     * @return True if the path is now a favorite, false if removed.
     */
    bool toggleFavorite(const QString &path);

    /**
     * @brief Moves a favorite to a new position.
     * @param from The current index.
     * @param to The target index.
     */
    void moveFavorite(int from, int to);

    /**
     * @brief Clears all favorites.
     */
    void clearAll();

    /**
     * @brief Loads favorites from persistent storage.
     */
    void loadSettings();

    /**
     * @brief Saves favorites to persistent storage.
     */
    void saveSettings();

signals:
    /**
     * @brief Emitted when a favorite is added.
     * @param path The added path.
     */
    void favoriteAdded(const QString &path);

    /**
     * @brief Emitted when a favorite is removed.
     * @param path The removed path.
     */
    void favoriteRemoved(const QString &path);

    /**
     * @brief Emitted when the favorites list changes.
     */
    void favoritesChanged();

private:
    QStringList favorites_;
};

#endif // FAVORITESMANAGER_H
