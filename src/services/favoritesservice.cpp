/**
 * @file favoritesservice.cpp
 * @brief Implementation of the FavoritesService service.
 */

#include "favoritesservice.h"

#include "favoritescore.h"

#include <QSettings>

FavoritesService::FavoritesService(QObject *parent) : QObject(parent)
{
    loadSettings();
}

QStringList FavoritesService::favorites() const
{
    return favorites::sorted(favorites_);
}

bool FavoritesService::isFavorite(const QString &path) const
{
    return favorites_.contains(path);
}

void FavoritesService::addFavorite(const QString &path)
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

void FavoritesService::removeFavorite(const QString &path)
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

bool FavoritesService::toggleFavorite(const QString &path)
{
    if (isFavorite(path)) {
        removeFavorite(path);
        return false;
    } else {
        addFavorite(path);
        return true;
    }
}

void FavoritesService::moveFavorite(int from, int to)
{
    QStringList moved = favorites::moveFavorite(favorites_, from, to);
    if (moved == favorites_) {
        return;
    }

    favorites_ = moved;
    saveSettings();
    emit favoritesChanged();
}

void FavoritesService::clearAll()
{
    if (favorites_.isEmpty()) {
        return;
    }

    favorites_.clear();
    saveSettings();
    emit favoritesChanged();
}

void FavoritesService::loadSettings()
{
    QSettings settings;
    favorites_ = settings.value("bookmarks/paths").toStringList();
}

void FavoritesService::saveSettings()
{
    QSettings settings;
    settings.setValue("bookmarks/paths", favorites_);
}
