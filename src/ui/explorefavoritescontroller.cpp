#include "explorefavoritescontroller.h"

#include "services/favoritesmanager.h"
#include "services/favoritesuicore.h"

#include <QAction>
#include <QFileInfo>
#include <QMenu>

ExploreFavoritesController::ExploreFavoritesController(FavoritesManager *favoritesManager,
                                                       QObject *parent)
    : QObject(parent), favoritesManager_(favoritesManager)
{
    if (favoritesManager_) {
        connect(favoritesManager_, &FavoritesManager::favoritesChanged, this,
                &ExploreFavoritesController::onFavoritesChanged);
    }
}

void ExploreFavoritesController::setToggleAction(QAction *action)
{
    toggleFavoriteAction_ = action;
}

void ExploreFavoritesController::setFavoritesMenu(QMenu *menu)
{
    favoritesMenu_ = menu;
    onFavoritesChanged();
}

void ExploreFavoritesController::updateForPath(const QString &path)
{
    if (!toggleFavoriteAction_ || !favoritesManager_) {
        return;
    }
    bool fav = favoritesManager_->isFavorite(path);
    toggleFavoriteAction_->setChecked(fav);
    toggleFavoriteAction_->setText(fav ? QString::fromUtf8("⭐") : QString::fromUtf8("☆"));
}

bool ExploreFavoritesController::isFavorite(const QString &path) const
{
    return favoritesManager_ && favoritesManager_->isFavorite(path);
}

void ExploreFavoritesController::onToggleFavorite(const QString &path)
{
    if (!favoritesManager_ || path.isEmpty()) {
        return;
    }

    bool isNowFavorite = favoritesManager_->toggleFavorite(path);
    if (isNowFavorite) {
        emit statusMessage(tr("Added to favorites: %1").arg(path), 3000);
    } else {
        emit statusMessage(tr("Removed from favorites: %1").arg(path), 3000);
    }

    if (toggleFavoriteAction_) {
        toggleFavoriteAction_->setChecked(isNowFavorite);
        toggleFavoriteAction_->setText(isNowFavorite ? QString::fromUtf8("⭐")
                                                     : QString::fromUtf8("☆"));
    }
}

void ExploreFavoritesController::onFavoriteSelected(QAction *action)
{
    if (!action) {
        return;
    }

    QString path = action->data().toString();
    if (path.isEmpty()) {
        return;
    }

    QFileInfo fileInfo(path);
    if (fileInfo.suffix().isEmpty()) {
        emit navigateToPath(path);
    } else {
        QString dir = path.left(path.lastIndexOf('/'));
        if (dir.isEmpty()) {
            dir = "/";
        }
        emit navigateToPath(dir);
        emit statusMessage(tr("Navigated to favorite: %1").arg(path), 3000);
    }
}

void ExploreFavoritesController::onFavoritesChanged()
{
    if (!favoritesMenu_ || !favoritesManager_) {
        return;
    }

    favoritesMenu_->clear();

    QStringList favorites = favoritesManager_->favorites();
    auto entries = favoritesui::buildMenuEntries(favorites);
    for (const auto &entry : entries) {
        QAction *action = favoritesMenu_->addAction(entry.displayName);
        if (entry.path.isEmpty()) {
            action->setEnabled(false);
        } else {
            action->setData(entry.path);
            action->setToolTip(entry.path);
        }
    }
}
