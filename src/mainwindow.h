#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QTreeView>
#include <QSplitter>
#include <QToolBar>
#include <QComboBox>
#include <QLabel>
#include <QFileSystemModel>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>

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

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    enum class Mode {
        ExploreRun,
        Transfer,
        View
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

private:
    void setupUi();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupExploreRunMode();
    void setupTransferMode();
    void setupViewMode();
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
    QPushButton *exploreRemoteUpButton_ = nullptr;
    QLabel *exploreRemoteCurrentDirLabel_ = nullptr;
    QString currentExploreRemoteDir_;

    // Transfer mode widgets
    QWidget *transferWidget_ = nullptr;
    QSplitter *transferSplitter_ = nullptr;
    QTreeView *localTreeView_ = nullptr;
    QTreeView *remoteTransferTreeView_ = nullptr;
    QFileSystemModel *localFileModel_ = nullptr;
    LocalFileProxyModel *localFileProxyModel_ = nullptr;
    QPushButton *localUpButton_ = nullptr;
    QPushButton *remoteUpButton_ = nullptr;
    QToolBar *remotePanelToolBar_ = nullptr;
    QToolBar *localPanelToolBar_ = nullptr;

    // View mode widgets
    QWidget *viewWidget_ = nullptr;
    VideoDisplayWidget *videoDisplayWidget_ = nullptr;
    QToolBar *viewPanelToolBar_ = nullptr;
    QAction *startStreamAction_ = nullptr;
    QAction *stopStreamAction_ = nullptr;
    QLabel *streamStatusLabel_ = nullptr;

    // View mode services
    StreamControlClient *streamControl_ = nullptr;
    VideoStreamReceiver *videoReceiver_ = nullptr;
    AudioStreamReceiver *audioReceiver_ = nullptr;
    AudioPlaybackService *audioPlayback_ = nullptr;
    bool isStreaming_ = false;

    // Transfer progress UI
    QWidget *transferProgressWidget_ = nullptr;
    QProgressBar *transferProgressBar_ = nullptr;
    QLabel *transferStatusLabel_ = nullptr;
    QTimer *transferProgressDelayTimer_ = nullptr;
    int transferTotalCount_ = 0;
    int transferCompletedCount_ = 0;
    bool transferProgressPending_ = false;

    // Toolbar
    QToolBar *mainToolBar_ = nullptr;
    QComboBox *modeCombo_ = nullptr;
    QAction *connectAction_ = nullptr;

    // Mode-specific actions
    QAction *playAction_ = nullptr;
    QAction *runAction_ = nullptr;
    QAction *mountAction_ = nullptr;
    QAction *resetAction_ = nullptr;
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
    QMenu *transferContextMenu_ = nullptr;
    QMenu *localContextMenu_ = nullptr;

    // Transfer mode current directories
    QString currentLocalDir_;
    QString currentRemoteTransferDir_;
    QLabel *localCurrentDirLabel_ = nullptr;
    QLabel *remoteCurrentDirLabel_ = nullptr;

    // Status bar
    QLabel *driveALabel_ = nullptr;
    QProgressBar *transferProgress_ = nullptr;
    QLabel *driveBLabel_ = nullptr;
    QLabel *connectionLabel_ = nullptr;

    // Dialogs
    PreferencesDialog *preferencesDialog_ = nullptr;
};

#endif // MAINWINDOW_H
