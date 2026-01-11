#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QToolBar>
#include <QProgressBar>

class PreferencesDialog;
class DeviceConnection;
class RemoteFileModel;
class TransferQueue;
class ConfigFileLoader;
class FilePreviewService;
class TransferService;
class ErrorHandler;
class ConnectionStatusWidget;
class ExplorePanel;
class TransferPanel;
class ViewPanel;
class ConfigPanel;

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

    // System control slots
    void onReset();
    void onReboot();
    void onPause();
    void onResume();
    void onMenuButton();
    void onPowerOff();
    void onEjectDriveA();
    void onEjectDriveB();

    // Connection slots
    void onConnectionStateChanged();
    void onDeviceInfoUpdated();
    void onDriveInfoUpdated();

    // Operation result slots
    void onOperationSucceeded(const QString &operation);

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
    void updateStatusBar();
    void updateActions();
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

#endif // MAINWINDOW_H
