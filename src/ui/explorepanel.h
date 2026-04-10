#ifndef EXPLOREPANEL_H
#define EXPLOREPANEL_H

#include "services/filetypecore.h"

#include <QSplitter>
#include <QToolBar>
#include <QTreeView>
#include <QWidget>

class DeviceConnection;
class RemoteFileModel;
class ConfigFileLoader;
class FilePreviewService;
class FavoritesManager;
class PlaylistManager;
class SonglengthsDatabase;
class HVSCMetadataService;
class GameBase64Service;
class StreamingManager;
class FileDetailsPanel;
class PathNavigationWidget;
class DriveStatusWidget;
class PlaylistWidget;
class FileActionController;
class ExploreFavoritesController;
class ExploreContextMenu;
class ExploreNavigationController;
class PreviewCoordinator;
class QMenu;

class ExplorePanel : public QWidget
{
    Q_OBJECT

public:
    explicit ExplorePanel(DeviceConnection *connection, RemoteFileModel *model,
                          ConfigFileLoader *configLoader, FilePreviewService *previewService,
                          FavoritesManager *favoritesManager, PlaylistManager *playlistManager,
                          QWidget *parent = nullptr);

    // Public API for MainWindow coordination
    void setCurrentDirectory(const QString &path);
    QString currentDirectory() const;
    void refresh();
    void refreshIfStale();
    void updateDriveInfo();

    // Settings
    void loadSettings();
    void saveSettings();

    // Database injection
    void setSonglengthsDatabase(SonglengthsDatabase *database);
    void setHVSCMetadataService(HVSCMetadataService *service);
    void setGameBase64Service(GameBase64Service *service);

    // Streaming manager injection (for auto-start on play/run)
    void setStreamingManager(StreamingManager *manager);

    // Selection info for MainWindow
    QString selectedPath() const;
    bool isSelectedDirectory() const;

signals:
    void statusMessage(const QString &message, int timeout = 0);
    void selectionChanged();
    void ejectDriveARequested();
    void ejectDriveBRequested();

private slots:
    void onConnectionStateChanged();
    void onSelectionChanged();
    void onDoubleClicked(const QModelIndex &index);
    void onContextMenu(const QPoint &pos);
    void onParentFolder();
    void onRefresh();

    // File action slots (delegate to actionController_)
    void onPlay();
    void onRun();
    void onMount();
    void onMountToDriveA();
    void onMountToDriveB();
    void onLoadConfig();
    void onDownload();

    // Playlist slot (delegates to actionController_)
    void onAddToPlaylist();

protected:
    void showEvent(QShowEvent *event) override;

private:
    void setupUi();
    void setupConnections();

    // Dependencies (not owned)
    DeviceConnection *deviceConnection_ = nullptr;
    RemoteFileModel *remoteFileModel_ = nullptr;
    PlaylistManager *playlistManager_ = nullptr;

    // Owned controllers (Qt parent ownership)
    FileActionController *actionController_ = nullptr;
    ExploreFavoritesController *favoritesController_ = nullptr;
    ExploreContextMenu *contextMenu_ = nullptr;
    ExploreNavigationController *navController_ = nullptr;
    PreviewCoordinator *previewCoordinator_ = nullptr;

    // UI widgets
    QSplitter *splitter_ = nullptr;
    QSplitter *rightSplitter_ = nullptr;
    QTreeView *treeView_ = nullptr;
    FileDetailsPanel *fileDetailsPanel_ = nullptr;
    PlaylistWidget *playlistWidget_ = nullptr;
    QToolBar *toolBar_ = nullptr;
    PathNavigationWidget *navWidget_ = nullptr;
    DriveStatusWidget *drive8Status_ = nullptr;
    DriveStatusWidget *drive9Status_ = nullptr;

    // Toolbar actions
    QAction *playAction_ = nullptr;
    QAction *runAction_ = nullptr;
    QAction *mountAction_ = nullptr;
    QAction *refreshAction_ = nullptr;
    QAction *toggleFavoriteAction_ = nullptr;
    QMenu *favoritesMenu_ = nullptr;
};

#endif  // EXPLOREPANEL_H
