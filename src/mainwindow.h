#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "services/metadataservicebundle.h"

#include <QMainWindow>
#include <QProgressBar>
#include <QTabWidget>
#include <QToolBar>

class PreferencesDialog;
class DeviceConnectionManager;
class RemoteFileModel;
class TransferQueue;
class ConfigFileLoaderService;
class FilePreviewService;
class TransferService;
class ErrorHandler;
class StatusMessageService;
class FavoritesService;
class PlaylistService;
class ConnectionStatusWidget;
class ExplorePanel;
class TransferPanel;
class ViewPanel;
class ConfigPanel;
class SystemCommandController;
class PanelCoordinator;
class ConnectionUIController;

class ServiceFactory;
class QShortcut;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    enum class Mode { ExploreRun, Transfer, View, Config };

    explicit MainWindow(QWidget *parent = nullptr);

    /**
     * @brief Saves settings, then destroys the panels, toolbar and coordinators
     * before the shared services they point at.
     */
    ~MainWindow() override;

protected:
    /**
     * @brief Quits quietly: suppresses error dialogs, then stops any stream and
     * waits (bounded) for the device to be told, while the event loop still runs.
     */
    void closeEvent(QCloseEvent *event) override;

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

    /// Enters full screen (View mode, all chrome hidden) or leaves it.
    void toggleFullScreen();

private:
    void setupUi();
    void setupMenuBar();
    void setupSystemToolBar();
    void setupStatusBar();
    void setupPanels(ServiceFactory *services);
    void setupConnections();
    void switchToMode(Mode mode);
    void setFullScreen(bool on);
    void updateWindowTitle();
    void loadSettings();
    void saveSettings();
    void destroyServiceUsersThenServices();

    Mode currentMode_ = Mode::ExploreRun;
    bool fullScreen_ = false;
    Qt::WindowStates stateBeforeFullScreen_ = Qt::WindowNoState;

    // Services (owned by services_, shared with panels)
    ServiceFactory *services_ = nullptr;
    DeviceConnectionManager *deviceConnection_ = nullptr;
    RemoteFileModel *remoteFileModel_ = nullptr;
    TransferQueue *transferQueue_ = nullptr;
    ConfigFileLoaderService *configFileLoader_ = nullptr;
    FilePreviewService *filePreviewService_ = nullptr;
    TransferService *transferService_ = nullptr;
    ErrorHandler *errorHandler_ = nullptr;
    StatusMessageService *statusMessageService_ = nullptr;
    FavoritesService *favoritesService_ = nullptr;
    PlaylistService *playlistService_ = nullptr;
    MetadataServiceBundle metadataBundle_;

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
    QAction *fullScreenAction_ = nullptr;
    QShortcut *exitFullScreenShortcut_ = nullptr;

    // Status bar
    ConnectionStatusWidget *connectionStatus_ = nullptr;

    // Dialogs
    PreferencesDialog *preferencesDialog_ = nullptr;
};

#endif  // MAINWINDOW_H
