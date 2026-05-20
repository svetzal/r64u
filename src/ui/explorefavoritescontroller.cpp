#include "explorefavoritescontroller.h"

#include "services/favoritesservice.h"
#include "services/favoritesuicore.h"
#include "utils/logging.h"

#include <QAction>
#include <QFileInfo>
#include <QMenu>

ExploreFavoritesController::ExploreFavoritesController(FavoritesService *favoritesService,
                                                       QObject *parent)
    : QObject(parent), favoritesService_(favoritesService)
{
    if (favoritesService_) {
        connect(favoritesService_, &FavoritesService::favoritesChanged, this,
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
    if (!toggleFavoriteAction_ || !favoritesService_) {
        qCWarning(LogUi) << "updateForPath: toggleFavoriteAction_ or favoritesService_ is null";
        return;
    }
    bool fav = favoritesService_->isFavorite(path);
    toggleFavoriteAction_->setChecked(fav);
    toggleFavoriteAction_->setText(fav ? QString::fromUtf8("⭐") : QString::fromUtf8("☆"));
}

bool ExploreFavoritesController::isFavorite(const QString &path) const
{
    return favoritesService_ && favoritesService_->isFavorite(path);
}

void ExploreFavoritesController::onToggleFavorite(const QString &path)
{
    if (!favoritesService_) {
        emit statusMessage(tr("Favorites not available"));
        return;
    }
    if (path.isEmpty()) {
        emit statusMessage(tr("No file selected"));
        return;
    }

    bool isNowFavorite = favoritesService_->toggleFavorite(path);
    if (isNowFavorite) {
        emit statusMessage(tr("Added to favorites: %1").arg(path));
    } else {
        emit statusMessage(tr("Removed from favorites: %1").arg(path));
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
        emit statusMessage(tr("No favorite selected"));
        return;
    }

    QString path = action->data().toString();
    if (path.isEmpty()) {
        emit statusMessage(tr("Favorite has no associated path"));
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
        emit statusMessage(tr("Navigated to favorite: %1").arg(path));
    }
}

void ExploreFavoritesController::onFavoritesChanged()
{
    if (!favoritesMenu_ || !favoritesService_) {
        qCWarning(LogUi) << "onFavoritesChanged: favoritesMenu_ or favoritesService_ is null";
        return;
    }

    favoritesMenu_->clear();

    QStringList favorites = favoritesService_->favorites();
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
