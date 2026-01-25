#ifndef EXPLOREPANEL_H
#define EXPLOREPANEL_H

#include <QWidget>
#include <QTreeView>
#include <QSplitter>
#include <QToolBar>
#include <QMenu>

class DeviceConnection;
class RemoteFileModel;
class ConfigFileLoader;
class FilePreviewService;
class FavoritesManager;
class PlaylistManager;
class FileDetailsPanel;
class PathNavigationWidget;
class DriveStatusWidget;
class PlaylistWidget;

class ExplorePanel : public QWidget
{
    Q_OBJECT

public:
    explicit ExplorePanel(DeviceConnection *connection,
                          RemoteFileModel *model,
                          ConfigFileLoader *configLoader,
                          FilePreviewService *previewService,
                          FavoritesManager *favoritesManager,
                          PlaylistManager *playlistManager,
                          QWidget *parent = nullptr);

    // Public API for MainWindow coordination
    void setCurrentDirectory(const QString &path);
    QString currentDirectory() const { return currentDirectory_; }
    void refresh();
    void refreshIfStale();
    void updateDriveInfo();

    // Settings
    void loadSettings();
    void saveSettings();

    // Selection info for MainWindow
    QString selectedPath() const;
    bool isSelectedDirectory() const;

signals:
    void statusMessage(const QString &message, int timeout = 0);
    void selectionChanged();

private slots:
    void onConnectionStateChanged();
    void onSelectionChanged();
    void onDoubleClicked(const QModelIndex &index);
    void onContextMenu(const QPoint &pos);
    void onParentFolder();

    // File actions
    void onPlay();
    void onRun();
    void onMount();
    void onMountToDriveA();
    void onMountToDriveB();
    void onLoadConfig();
    void onDownload();
    void onRefresh();

    // File preview service slots
    void onFileContentRequested(const QString &path);
    void onPreviewReady(const QString &remotePath, const QByteArray &data);
    void onPreviewFailed(const QString &remotePath, const QString &error);

    // Favorites slots
    void onToggleFavorite();
    void onFavoriteSelected(QAction *action);
    void onFavoritesChanged();

    // Playlist slots
    void onAddToPlaylist();

    // Config file loading slots
    void onConfigLoadFinished(const QString &path);
    void onConfigLoadFailed(const QString &path, const QString &error);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void setupUi();
    void setupContextMenu();
    void setupConnections();
    void runDiskImage(const QString &path);

    // Dependencies (not owned)
    DeviceConnection *deviceConnection_ = nullptr;
    RemoteFileModel *remoteFileModel_ = nullptr;
    ConfigFileLoader *configFileLoader_ = nullptr;
    FilePreviewService *previewService_ = nullptr;
    FavoritesManager *favoritesManager_ = nullptr;
    PlaylistManager *playlistManager_ = nullptr;

    // State
    QString currentDirectory_;

    // UI widgets
    QSplitter *splitter_ = nullptr;
    QSplitter *rightSplitter_ = nullptr;  // For details + playlist
    QTreeView *treeView_ = nullptr;
    FileDetailsPanel *fileDetailsPanel_ = nullptr;
    PlaylistWidget *playlistWidget_ = nullptr;
    QToolBar *toolBar_ = nullptr;
    PathNavigationWidget *navWidget_ = nullptr;
    DriveStatusWidget *drive8Status_ = nullptr;
    DriveStatusWidget *drive9Status_ = nullptr;

    // Actions
    QAction *playAction_ = nullptr;
    QAction *runAction_ = nullptr;
    QAction *mountAction_ = nullptr;
    QAction *refreshAction_ = nullptr;

    // Context menu
    QMenu *contextMenu_ = nullptr;
    QAction *contextPlayAction_ = nullptr;
    QAction *contextRunAction_ = nullptr;
    QAction *contextLoadConfigAction_ = nullptr;
    QAction *contextMountAAction_ = nullptr;
    QAction *contextMountBAction_ = nullptr;
    QAction *contextDownloadAction_ = nullptr;
    QAction *contextToggleFavoriteAction_ = nullptr;
    QAction *contextAddToPlaylistAction_ = nullptr;

    // Favorites UI
    QAction *toggleFavoriteAction_ = nullptr;
    QMenu *favoritesMenu_ = nullptr;
};

#endif // EXPLOREPANEL_H
