#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QTreeView>
#include <QSplitter>
#include <QToolBar>
#include <QTabBar>
#include <QLabel>
#include <QFileSystemModel>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QButtonGroup>
#include <QRadioButton>
#include <QListWidget>
#include <QToolButton>

#include "services/c64urestclient.h"

class PreferencesDialog;
class DeviceConnection;
class RemoteFileModel;
class LocalFileProxyModel;
class TransferQueue;
class FileDetailsPanel;
class ConfigFileLoader;
class VideoDisplayWidget;
class VideoStreamReceiver;
class AudioStreamReceiver;
class AudioPlaybackService;
class StreamControlClient;
class KeyboardInputService;
class ConfigurationModel;
class ConfigItemsPanel;
class PathNavigationWidget;
class ConnectionStatusWidget;
class DriveStatusWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    enum class Mode {
        ExploreRun,
        Transfer,
        View,
        Config
    };

    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onModeChanged(int index);
    void onPreferences();
    void onConnect();
    void onDisconnect();
    void onPlay();
    void onRun();
    void onMount();
    void onMountToDriveA();
    void onMountToDriveB();
    void onEjectDriveA();
    void onEjectDriveB();
    void onReset();
    void onReboot();
    void onPause();
    void onResume();
    void onMenuButton();
    void onPowerOff();
    void onUpload();
    void onDownload();
    void onNewFolder();
    void onDelete();
    void onRefresh();

    // Transfer progress slots
    void onUploadProgress(const QString &file, qint64 sent, qint64 total);
    void onUploadFinished(const QString &localPath, const QString &remotePath);
    void onDownloadProgress(const QString &file, qint64 received, qint64 total);
    void onDownloadFinished(const QString &remotePath, const QString &localPath);
    void onDirectoryCreated(const QString &path);
    void onFileRemoved(const QString &path);
    void onFileRenamed(const QString &oldPath, const QString &newPath);
    void onRemoteRename();

    // Connection slots
    void onConnectionStateChanged();
    void onDeviceInfoUpdated();
    void onDriveInfoUpdated();
    void onConnectionError(const QString &message);

    // Tree view slots
    void onRemoteSelectionChanged();
    void onRemoteDoubleClicked(const QModelIndex &index);
    void onRemoteContextMenu(const QPoint &pos);

    // Operation result slots
    void onOperationSucceeded(const QString &operation);
    void onOperationFailed(const QString &operation, const QString &error);

    // File details panel slots
    void onFileContentRequested(const QString &path);
    void onFileContentReceived(const QString &remotePath, const QByteArray &data);

    // Config file loading slots
    void onLoadConfig();
    void onConfigLoadFinished(const QString &path);
    void onConfigLoadFailed(const QString &path, const QString &error);

    // Transfer mode slots
    void onLocalDoubleClicked(const QModelIndex &index);
    void onRemoteTransferDoubleClicked(const QModelIndex &index);
    void onLocalContextMenu(const QPoint &pos);
    void onNewLocalFolder();
    void onLocalDelete();
    void onLocalRename();
    void onLocalParentFolder();
    void onRemoteParentFolder();
    void setCurrentLocalDir(const QString &path);
    void setCurrentRemoteTransferDir(const QString &path);

    // Explore/Run mode navigation slots
    void onExploreRemoteParentFolder();
    void setCurrentExploreRemoteDir(const QString &path);

    // Transfer progress slots
    void onTransferStarted(const QString &fileName);
    void onTransferQueueChanged();
    void onAllTransfersCompleted();
    void onShowTransferProgress();

    // View mode streaming slots
    void onStartStreaming();
    void onStopStreaming();
    void onVideoFrameReady();
    void onVideoFormatDetected(int format);
    void onStreamCommandSucceeded(const QString &command);
    void onStreamCommandFailed(const QString &command, const QString &error);
    void onScalingModeChanged(int id);

    // Config mode slots
    void onConfigSaveToFlash();
    void onConfigLoadFromFlash();
    void onConfigResetToDefaults();
    void onConfigRefresh();
    void onConfigCategoriesReceived(const QStringList &categories);
    void onConfigCategoryItemsReceived(const QString &category,
                                       const QHash<QString, ConfigItemMetadata> &items);
    void onConfigSavedToFlash();
    void onConfigLoadedFromFlash();
    void onConfigResetComplete();
    void onConfigDirtyStateChanged(bool isDirty);
    void onConfigCategorySelected(QListWidgetItem *current, QListWidgetItem *previous);
    void onConfigItemEdited(const QString &category, const QString &item,
                            const QVariant &value);
    void onConfigItemSetResult(const QString &category, const QString &item);

private:
    void setupUi();
    void setupMenuBar();
    void setupSystemToolBar();
    void setupStatusBar();
    void setupExploreRunMode();
    void setupTransferMode();
    void setupViewMode();
    void setupConfigMode();
    void setupConnections();
    void switchToMode(Mode mode);
    void updateWindowTitle();
    void updateStatusBar();
    void updateActions();
    void loadSettings();
    void saveSettings();

    QString selectedRemotePath() const;
    QString selectedLocalPath() const;
    QString currentRemoteDirectory() const;
    bool isSelectedDirectory() const;
    void runDiskImage(const QString &path);

    Mode currentMode_ = Mode::ExploreRun;

    // Services
    DeviceConnection *deviceConnection_ = nullptr;
    RemoteFileModel *remoteFileModel_ = nullptr;
    TransferQueue *transferQueue_ = nullptr;
    ConfigFileLoader *configFileLoader_ = nullptr;

    // Central widget
    QStackedWidget *stackedWidget_ = nullptr;

    // Explore/Run mode widgets
    QWidget *exploreRunWidget_ = nullptr;
    QSplitter *exploreRunSplitter_ = nullptr;
    QTreeView *remoteTreeView_ = nullptr;
    FileDetailsPanel *fileDetailsPanel_ = nullptr;
    QToolBar *exploreRemotePanelToolBar_ = nullptr;
    PathNavigationWidget *exploreRemoteNavWidget_ = nullptr;
    QString currentExploreRemoteDir_;

    // Transfer mode widgets
    QWidget *transferWidget_ = nullptr;
    QSplitter *transferSplitter_ = nullptr;
    QTreeView *localTreeView_ = nullptr;
    QTreeView *remoteTransferTreeView_ = nullptr;
    QFileSystemModel *localFileModel_ = nullptr;
    LocalFileProxyModel *localFileProxyModel_ = nullptr;
    PathNavigationWidget *localNavWidget_ = nullptr;
    PathNavigationWidget *remoteNavWidget_ = nullptr;
    QToolBar *remotePanelToolBar_ = nullptr;
    QToolBar *localPanelToolBar_ = nullptr;

    // View mode widgets
    QWidget *viewWidget_ = nullptr;
    VideoDisplayWidget *videoDisplayWidget_ = nullptr;
    QToolBar *viewPanelToolBar_ = nullptr;
    QAction *startStreamAction_ = nullptr;
    QAction *stopStreamAction_ = nullptr;
    QLabel *streamStatusLabel_ = nullptr;
    QButtonGroup *scalingModeGroup_ = nullptr;
    QRadioButton *sharpRadio_ = nullptr;
    QRadioButton *smoothRadio_ = nullptr;
    QRadioButton *integerRadio_ = nullptr;

    // View mode services
    StreamControlClient *streamControl_ = nullptr;
    VideoStreamReceiver *videoReceiver_ = nullptr;
    AudioStreamReceiver *audioReceiver_ = nullptr;
    AudioPlaybackService *audioPlayback_ = nullptr;
    KeyboardInputService *keyboardInput_ = nullptr;
    bool isStreaming_ = false;

    // Config mode widgets
    QWidget *configWidget_ = nullptr;
    QToolBar *configToolBar_ = nullptr;
    QAction *configSaveToFlashAction_ = nullptr;
    QAction *configLoadFromFlashAction_ = nullptr;
    QAction *configResetToDefaultsAction_ = nullptr;
    QAction *configRefreshAction_ = nullptr;
    QLabel *configUnsavedIndicator_ = nullptr;
    QSplitter *configSplitter_ = nullptr;
    QListWidget *configCategoryList_ = nullptr;
    ConfigItemsPanel *configItemsPanel_ = nullptr;

    // Config mode model
    ConfigurationModel *configModel_ = nullptr;

    // Transfer progress UI
    QWidget *transferProgressWidget_ = nullptr;
    QProgressBar *transferProgressBar_ = nullptr;
    QLabel *transferStatusLabel_ = nullptr;
    QTimer *transferProgressDelayTimer_ = nullptr;
    int transferTotalCount_ = 0;
    int transferCompletedCount_ = 0;
    bool transferProgressPending_ = false;

    // Toolbar
    QToolBar *systemToolBar_ = nullptr;
    QTabBar *modeTabBar_ = nullptr;
    QAction *connectAction_ = nullptr;

    // Mode-specific actions
    QAction *playAction_ = nullptr;
    QAction *runAction_ = nullptr;
    QAction *mountAction_ = nullptr;
    QAction *resetAction_ = nullptr;
    QAction *rebootAction_ = nullptr;
    QAction *pauseAction_ = nullptr;
    QAction *resumeAction_ = nullptr;
    QAction *menuAction_ = nullptr;
    QAction *powerOffAction_ = nullptr;
    QAction *uploadAction_ = nullptr;
    QAction *downloadAction_ = nullptr;
    QAction *newFolderAction_ = nullptr;
    QAction *remoteDeleteAction_ = nullptr;
    QAction *remoteRenameAction_ = nullptr;
    QAction *localDeleteAction_ = nullptr;
    QAction *localRenameAction_ = nullptr;
    QAction *localNewFolderAction_ = nullptr;
    QAction *refreshAction_ = nullptr;

    // Context menus
    QMenu *remoteContextMenu_ = nullptr;
    QAction *contextPlayAction_ = nullptr;
    QAction *contextRunAction_ = nullptr;
    QAction *contextLoadConfigAction_ = nullptr;
    QAction *contextMountAAction_ = nullptr;
    QAction *contextMountBAction_ = nullptr;
    QMenu *transferContextMenu_ = nullptr;
    QAction *transferSetDestAction_ = nullptr;
    QMenu *localContextMenu_ = nullptr;
    QAction *localSetDestAction_ = nullptr;

    // Transfer mode current directories
    QString currentLocalDir_;
    QString currentRemoteTransferDir_;

    // Status bar
    DriveStatusWidget *drive8Status_ = nullptr;
    DriveStatusWidget *drive9Status_ = nullptr;
    QProgressBar *transferProgress_ = nullptr;
    ConnectionStatusWidget *connectionStatus_ = nullptr;

    // Dialogs
    PreferencesDialog *preferencesDialog_ = nullptr;
};

#endif // MAINWINDOW_H
