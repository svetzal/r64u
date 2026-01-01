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

class PreferencesDialog;
class DeviceConnection;
class RemoteFileModel;
class TransferQueue;
class TransferQueueWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    enum class Mode {
        ExploreRun,
        Transfer
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

private:
    void setupUi();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupExploreRunMode();
    void setupTransferMode();
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

    // Central widget
    QStackedWidget *stackedWidget_ = nullptr;

    // Explore/Run mode widgets
    QWidget *exploreRunWidget_ = nullptr;
    QTreeView *remoteTreeView_ = nullptr;

    // Transfer mode widgets
    QWidget *transferWidget_ = nullptr;
    QSplitter *transferSplitter_ = nullptr;
    QTreeView *localTreeView_ = nullptr;
    QTreeView *remoteTransferTreeView_ = nullptr;
    QFileSystemModel *localFileModel_ = nullptr;
    TransferQueueWidget *transferQueueWidget_ = nullptr;

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
    QAction *deleteAction_ = nullptr;
    QAction *refreshAction_ = nullptr;

    // Context menus
    QMenu *remoteContextMenu_ = nullptr;
    QMenu *transferContextMenu_ = nullptr;

    // Status bar
    QLabel *driveALabel_ = nullptr;
    QProgressBar *transferProgress_ = nullptr;
    QLabel *driveBLabel_ = nullptr;
    QLabel *connectionLabel_ = nullptr;

    // Dialogs
    PreferencesDialog *preferencesDialog_ = nullptr;
};

#endif // MAINWINDOW_H
