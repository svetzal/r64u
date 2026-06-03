#include "explorecontextmenucontroller.h"

#include "explorepanelcore.h"

#include "core/filetypecore.h"

#include <QAction>
#include <QMenu>
#include <QPoint>

ExploreContextMenuController::ExploreContextMenuController(QObject *parent) : QObject(parent)
{
    contextMenu_ = new QMenu();

    contextPlayAction_ = contextMenu_->addAction(tr("Play"));
    connect(contextPlayAction_, &QAction::triggered, this,
            &ExploreContextMenuController::playRequested);

    contextAddToPlaylistAction_ = contextMenu_->addAction(tr("Add to Playlist"));
    connect(contextAddToPlaylistAction_, &QAction::triggered, this,
            &ExploreContextMenuController::addToPlaylistRequested);

    contextRunAction_ = contextMenu_->addAction(tr("Run"));
    connect(contextRunAction_, &QAction::triggered, this,
            &ExploreContextMenuController::runRequested);

    contextLoadConfigAction_ = contextMenu_->addAction(tr("Load Config"));
    connect(contextLoadConfigAction_, &QAction::triggered, this,
            &ExploreContextMenuController::loadConfigRequested);

    contextMenu_->addSeparator();

    contextMountAAction_ = contextMenu_->addAction(tr("Mount to Drive A"));
    connect(contextMountAAction_, &QAction::triggered, this,
            &ExploreContextMenuController::mountARequested);

    contextMountBAction_ = contextMenu_->addAction(tr("Mount to Drive B"));
    connect(contextMountBAction_, &QAction::triggered, this,
            &ExploreContextMenuController::mountBRequested);

    contextMenu_->addSeparator();

    contextDownloadAction_ = contextMenu_->addAction(tr("Download"));
    connect(contextDownloadAction_, &QAction::triggered, this,
            &ExploreContextMenuController::downloadRequested);

    contextMenu_->addSeparator();

    contextToggleFavoriteAction_ = contextMenu_->addAction(tr("Toggle Favorite"));
    connect(contextToggleFavoriteAction_, &QAction::triggered, this,
            &ExploreContextMenuController::toggleFavoriteRequested);

    contextMenu_->addSeparator();

    QAction *refreshAction = contextMenu_->addAction(tr("Refresh"));
    connect(refreshAction, &QAction::triggered, this,
            &ExploreContextMenuController::refreshRequested);
}

void ExploreContextMenuController::prepareMenu(const explorepanel::ActionEnablement &enablement,
                                               bool canAddToPlaylist, bool isFavorite)
{
    contextPlayAction_->setEnabled(enablement.canPlay);
    contextAddToPlaylistAction_->setEnabled(canAddToPlaylist);
    contextRunAction_->setEnabled(enablement.canRun);
    contextLoadConfigAction_->setEnabled(enablement.canLoadConfig);
    contextMountAAction_->setEnabled(enablement.canMount);
    contextMountBAction_->setEnabled(enablement.canMount);
    contextDownloadAction_->setEnabled(enablement.canDownload);
    contextToggleFavoriteAction_->setText(isFavorite ? tr("Remove from Favorites")
                                                     : tr("Add to Favorites"));
}

void ExploreContextMenuController::showForSelection(
    const QPoint &globalPos, const explorepanel::ActionEnablement &enablement,
    bool canAddToPlaylist, bool isFavorite)
{
    prepareMenu(enablement, canAddToPlaylist, isFavorite);
    contextMenu_->exec(globalPos);
}
