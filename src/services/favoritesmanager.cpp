/**
 * @file favoritesmanager.cpp
 * @brief Implementation of the FavoritesManager service.
 */

#include "favoritesmanager.h"

#include "favoritescore.h"

#include <QSettings>

FavoritesManager::FavoritesManager(QObject *parent) : QObject(parent)
{
    loadSettings();
}

QStringList FavoritesManager::favorites() const
{
    return favorites::sorted(favorites_);
}

bool FavoritesManager::isFavorite(const QString &path) const
{
    return favorites_.contains(path);
}

void FavoritesManager::addFavorite(const QString &path)
{
    auto [paths, added] = favorites::addFavorite(favorites_, path);
    if (!added) {
        return;
    }

    favorites_ = paths;
    saveSettings();
    emit favoriteAdded(path);
    emit favoritesChanged();
}

void FavoritesManager::removeFavorite(const QString &path)
{
    auto [paths, removed] = favorites::removeFavorite(favorites_, path);
    if (!removed) {
        return;
    }

    favorites_ = paths;
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
    QStringList moved = favorites::moveFavorite(favorites_, from, to);
    if (moved == favorites_) {
        return;
    }

    favorites_ = moved;
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
