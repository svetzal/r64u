#ifndef EXPLORECONTEXTMENU_H
#define EXPLORECONTEXTMENU_H

#include "explorepanelcore.h"

#include "services/filetypecore.h"

#include <QObject>

class QMenu;
class QAction;
class QPoint;

class ExploreContextMenu : public QObject
{
    Q_OBJECT

public:
    explicit ExploreContextMenu(QObject *parent = nullptr);

    /// Show the context menu using a pre-computed ActionEnablement struct.
    void showForSelection(const QPoint &globalPos, const explorepanel::ActionEnablement &enablement,
                          bool canAddToPlaylist, bool isFavorite);

signals:
    void playRequested();
    void runRequested();
    void mountARequested();
    void mountBRequested();
    void downloadRequested();
    void loadConfigRequested();
    void toggleFavoriteRequested();
    void addToPlaylistRequested();
    void refreshRequested();

private:
    QMenu *contextMenu_ = nullptr;
    QAction *contextPlayAction_ = nullptr;
    QAction *contextRunAction_ = nullptr;
    QAction *contextLoadConfigAction_ = nullptr;
    QAction *contextMountAAction_ = nullptr;
    QAction *contextMountBAction_ = nullptr;
    QAction *contextDownloadAction_ = nullptr;
    QAction *contextToggleFavoriteAction_ = nullptr;
    QAction *contextAddToPlaylistAction_ = nullptr;
};

#endif  // EXPLORECONTEXTMENU_H
