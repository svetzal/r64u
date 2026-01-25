/**
 * @file favoritesmanager.cpp
 * @brief Implementation of the FavoritesManager service.
 */

#include "favoritesmanager.h"
#include <QSettings>

FavoritesManager::FavoritesManager(QObject *parent)
    : QObject(parent)
{
    loadSettings();
}

bool FavoritesManager::isFavorite(const QString &path) const
{
    return favorites_.contains(path);
}

void FavoritesManager::addFavorite(const QString &path)
{
    if (path.isEmpty() || favorites_.contains(path)) {
        return;
    }

    favorites_.append(path);
    saveSettings();
    emit favoriteAdded(path);
    emit favoritesChanged();
}

void FavoritesManager::removeFavorite(const QString &path)
{
    if (!favorites_.contains(path)) {
        return;
    }

    favorites_.removeAll(path);
    saveSettings();
    emit favoriteRemoved(path);
    emit favoritesChanged();
}

bool FavoritesManager::toggleFavorite(const QString &path)
{
    if (isFavorite(path)) {
        removeFavorite(path);
        return false;
    } else {
        addFavorite(path);
        return true;
    }
}

void FavoritesManager::moveFavorite(int from, int to)
{
    if (from < 0 || from >= favorites_.count() ||
        to < 0 || to >= favorites_.count() ||
        from == to) {
        return;
    }

    favorites_.move(from, to);
    saveSettings();
    emit favoritesChanged();
}

void FavoritesManager::clearAll()
{
    if (favorites_.isEmpty()) {
        return;
    }

    favorites_.clear();
    saveSettings();
    emit favoritesChanged();
}

void FavoritesManager::loadSettings()
{
    QSettings settings;
    favorites_ = settings.value("bookmarks/paths").toStringList();
}

void FavoritesManager::saveSettings()
{
    QSettings settings;
    settings.setValue("bookmarks/paths", favorites_);
}
