#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProgressBar>
#include <QTabWidget>
#include <QToolBar>

class PreferencesDialog;
class DeviceConnection;
class RemoteFileModel;
class TransferQueue;
class ConfigFileLoader;
class FilePreviewService;
class TransferService;
class ErrorHandler;
class StatusMessageService;
class FavoritesManager;
class PlaylistManager;
class HttpFileDownloader;
class SonglengthsDatabase;
class HVSCMetadataService;
class GameBase64Service;
class ConnectionStatusWidget;
class ExplorePanel;
class TransferPanel;
class ViewPanel;
class ConfigPanel;
class SystemCommandController;
class PanelCoordinator;
class ConnectionUIController;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    enum class Mode { ExploreRun, Transfer, View, Config };

    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onPreferences();
    void onConnect();
    void onDisconnect();

    // System control slots
    void onPowerOff();

    // Connection lifecycle slots (navigation/model management)
    void onConnectionStateChanged();
    void onDriveInfoUpdated();

    // Refresh slot (shared by panels)
    void onRefresh();

private:
    void setupUi();
    void setupMenuBar();
    void setupSystemToolBar();
    void setupStatusBar();
    void setupPanels();
    void setupConnections();
    void switchToMode(Mode mode);
    void updateWindowTitle();
    void loadSettings();
    void saveSettings();

    Mode currentMode_ = Mode::ExploreRun;

    // Services (owned by MainWindow, shared with panels)
    DeviceConnection *deviceConnection_ = nullptr;
    RemoteFileModel *remoteFileModel_ = nullptr;
    TransferQueue *transferQueue_ = nullptr;
    ConfigFileLoader *configFileLoader_ = nullptr;
    FilePreviewService *filePreviewService_ = nullptr;
    TransferService *transferService_ = nullptr;
    ErrorHandler *errorHandler_ = nullptr;
    StatusMessageService *statusMessageService_ = nullptr;
    FavoritesManager *favoritesManager_ = nullptr;
    PlaylistManager *playlistManager_ = nullptr;
    HttpFileDownloader *songlengthsDownloader_ = nullptr;
    SonglengthsDatabase *songlengthsDatabase_ = nullptr;
    HttpFileDownloader *stilDownloader_ = nullptr;
    HttpFileDownloader *buglistDownloader_ = nullptr;
    HVSCMetadataService *hvscMetadataService_ = nullptr;
    HttpFileDownloader *gameBase64Downloader_ = nullptr;
    GameBase64Service *gameBase64Service_ = nullptr;

    SystemCommandController *systemCommandController_ = nullptr;
    PanelCoordinator *panelCoordinator_ = nullptr;
    ConnectionUIController *connectionUiController_ = nullptr;

    // Central widget
    QTabWidget *modeTabWidget_ = nullptr;

    // Mode panels (owned by tab widget)
    ExplorePanel *explorePanel_ = nullptr;
    TransferPanel *transferPanel_ = nullptr;
    ViewPanel *viewPanel_ = nullptr;
    ConfigPanel *configPanel_ = nullptr;

    // System toolbar
    QToolBar *systemToolBar_ = nullptr;
    QAction *connectAction_ = nullptr;
    QAction *resetAction_ = nullptr;
    QAction *rebootAction_ = nullptr;
    QAction *pauseAction_ = nullptr;
    QAction *resumeAction_ = nullptr;
    QAction *menuAction_ = nullptr;
    QAction *powerOffAction_ = nullptr;
    QAction *refreshAction_ = nullptr;

    // Status bar
    ConnectionStatusWidget *connectionStatus_ = nullptr;

    // Dialogs
    PreferencesDialog *preferencesDialog_ = nullptr;
};

#endif  // MAINWINDOW_H
