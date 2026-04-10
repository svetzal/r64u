#include "mainwindow.h"

#include "models/remotefilemodel.h"
#include "models/transferqueue.h"
#include "services/configfileloader.h"
#include "services/credentialstore.h"
#include "services/deviceconnection.h"
#include "services/errorhandler.h"
#include "services/favoritesmanager.h"
#include "services/filepreviewservice.h"
#include "services/gamebase64service.h"
#include "services/httpfiledownloader.h"
#include "services/hvscmetadataservice.h"
#include "services/irestclient.h"
#include "services/playlistmanager.h"
#include "services/servicefactory.h"
#include "services/songlengthsdatabase.h"
#include "services/statusmessageservice.h"
#include "services/streamingmanager.h"
#include "services/systemcommandcontroller.h"
#include "services/transferservice.h"
#include "ui/configpanel.h"
#include "ui/connectionstatuswidget.h"
#include "ui/connectionuicontroller.h"
#include "ui/explorepanel.h"
#include "ui/menubarbuilder.h"
#include "ui/panelcoordinator.h"
#include "ui/preferencesdialog.h"
#include "ui/transferpanel.h"
#include "ui/viewpanel.h"

#include <QCloseEvent>
#include <QFileInfo>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    auto *services = new ServiceFactory(this, this);

    deviceConnection_ = services->deviceConnection();
    remoteFileModel_ = services->remoteFileModel();
    transferQueue_ = services->transferQueue();
    configFileLoader_ = services->configFileLoader();
    filePreviewService_ = services->filePreviewService();
    transferService_ = services->transferService();
    errorHandler_ = services->errorHandler();
    statusMessageService_ = services->statusMessageService();
    favoritesManager_ = services->favoritesManager();
    playlistManager_ = services->playlistManager();
    songlengthsDownloader_ = services->songlengthsDownloader();
    songlengthsDatabase_ = services->songlengthsDatabase();
    stilDownloader_ = services->stilDownloader();
    buglistDownloader_ = services->buglistDownloader();
    hvscMetadataService_ = services->hvscMetadataService();
    gameBase64Downloader_ = services->gameBase64Downloader();
    gameBase64Service_ = services->gameBase64Service();

    systemCommandController_ =
        new SystemCommandController(deviceConnection_->restClient(), statusMessageService_, this);

    setupUi();
    setupMenuBar();
    setupSystemToolBar();
    setupStatusBar();
    setupPanels();
    setupConnections();

    switchToMode(Mode::ExploreRun);
    updateWindowTitle();
    connectionUiController_->updateAll();
    loadSettings();

    resize(1024, 768);
    setMinimumSize(800, 600);
}

MainWindow::~MainWindow()
{
    saveSettings();
}

void MainWindow::setupUi()
{
    // Create container with top margin for spacing below toolbar
    auto *centralContainer = new QWidget(this);
    auto *layout = new QVBoxLayout(centralContainer);
    layout->setContentsMargins(8, 16, 8, 8);
    layout->setSpacing(0);

    // Create tabbed mode widget
    // Note: Signal connection is deferred to PanelCoordinator to avoid
    // triggering onModeChanged before all widgets are initialized
    modeTabWidget_ = new QTabWidget();
    layout->addWidget(modeTabWidget_);

    setCentralWidget(centralContainer);
}

void MainWindow::setupMenuBar()
{
    refreshAction_ = MenuBarBuilder::build(this, systemCommandController_, modeTabWidget_);
}

void MainWindow::setupSystemToolBar()
{
    // System toolbar (top row)
    systemToolBar_ = addToolBar(tr("System"));
    systemToolBar_->setMovable(false);
    systemToolBar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    // Connect button
    connectAction_ = systemToolBar_->addAction(tr("Connect"));
    connectAction_->setToolTip(tr("Connect to C64U device"));
    connect(connectAction_, &QAction::triggered, this, [this]() {
        DeviceConnection::ConnectionState state = deviceConnection_->state();
        if (state == DeviceConnection::ConnectionState::Disconnected) {
            onConnect();
        } else {
            onDisconnect();
        }
    });

    systemToolBar_->addSeparator();

    // Machine actions
    resetAction_ = systemToolBar_->addAction(tr("Reset"));
    resetAction_->setToolTip(tr("Reset the C64"));
    connect(resetAction_, &QAction::triggered, systemCommandController_,
            &SystemCommandController::onReset);

    rebootAction_ = systemToolBar_->addAction(tr("Reboot"));
    rebootAction_->setToolTip(tr("Reboot the Ultimate device"));
    connect(rebootAction_, &QAction::triggered, systemCommandController_,
            &SystemCommandController::onReboot);

    pauseAction_ = systemToolBar_->addAction(tr("Pause"));
    pauseAction_->setToolTip(tr("Pause C64 execution"));
    connect(pauseAction_, &QAction::triggered, systemCommandController_,
            &SystemCommandController::onPause);

    resumeAction_ = systemToolBar_->addAction(tr("Resume"));
    resumeAction_->setToolTip(tr("Resume C64 execution"));
    connect(resumeAction_, &QAction::triggered, systemCommandController_,
            &SystemCommandController::onResume);

    menuAction_ = systemToolBar_->addAction(tr("Menu"));
    menuAction_->setToolTip(tr("Press Ultimate menu button"));
    connect(menuAction_, &QAction::triggered, systemCommandController_,
            &SystemCommandController::onMenuButton);

    powerOffAction_ = systemToolBar_->addAction(tr("Power Off"));
    powerOffAction_->setToolTip(tr("Power off the Ultimate device"));
    connect(powerOffAction_, &QAction::triggered, this, &MainWindow::onPowerOff);

    systemToolBar_->addSeparator();

    auto *prefsAction = systemToolBar_->addAction(tr("Preferences"));
    prefsAction->setToolTip(tr("Open preferences dialog"));
    connect(prefsAction, &QAction::triggered, this, &MainWindow::onPreferences);

    // Spacer to push connection status to the right
    auto *spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    systemToolBar_->addWidget(spacer);

    // Connection status on the right
    connectionStatus_ = new ConnectionStatusWidget();
    systemToolBar_->addWidget(connectionStatus_);

    // Create ConnectionUIController now that all actions and status widget exist
    connectionUiController_ =
        new ConnectionUIController(deviceConnection_, connectionStatus_, this);
    connectionUiController_->setManagedActions(
        {resetAction_, rebootAction_, pauseAction_, resumeAction_, menuAction_, powerOffAction_},
        connectAction_, refreshAction_);

    // Update window title and actions when connection UI changes
    connect(connectionUiController_, &ConnectionUIController::windowTitleUpdateNeeded, this,
            &MainWindow::updateWindowTitle);
}

void MainWindow::setupStatusBar()
{
    // Connect StatusMessageService to statusBar
    connect(statusMessageService_, &StatusMessageService::displayMessage, statusBar(),
            &QStatusBar::showMessage);

    statusMessageService_->showInfo(tr("Ready"));
}

void MainWindow::setupPanels()
{
    // Create mode panels with their dependencies
    explorePanel_ = new ExplorePanel(deviceConnection_, remoteFileModel_, configFileLoader_,
                                     filePreviewService_, favoritesManager_, playlistManager_);
    explorePanel_->setSonglengthsDatabase(songlengthsDatabase_);
    explorePanel_->setHVSCMetadataService(hvscMetadataService_);
    explorePanel_->setGameBase64Service(gameBase64Service_);
    transferPanel_ = new TransferPanel(deviceConnection_, remoteFileModel_, transferService_);
    viewPanel_ = new ViewPanel(deviceConnection_);
    configPanel_ = new ConfigPanel(deviceConnection_);

    // Wire up the streaming manager for auto stream start/stop
    playlistManager_->setStreamingManager(viewPanel_->streamingManager());
    explorePanel_->setStreamingManager(viewPanel_->streamingManager());

    // Add panels to tab widget
    modeTabWidget_->addTab(explorePanel_, tr("Explore/Run"));
    modeTabWidget_->addTab(transferPanel_, tr("Transfer"));
    modeTabWidget_->addTab(viewPanel_, tr("View"));
    modeTabWidget_->addTab(configPanel_, tr("Config"));

    // Connect ExplorePanel drive eject signals to SystemCommandController
    connect(explorePanel_, &ExplorePanel::ejectDriveARequested, systemCommandController_,
            &SystemCommandController::onEjectDriveA);
    connect(explorePanel_, &ExplorePanel::ejectDriveBRequested, systemCommandController_,
            &SystemCommandController::onEjectDriveB);

    // Create PanelCoordinator to handle panel signal wiring and mode coordination
    panelCoordinator_ =
        new PanelCoordinator(explorePanel_, transferPanel_, viewPanel_, configPanel_,
                             deviceConnection_, remoteFileModel_, transferService_,
                             statusMessageService_, errorHandler_, modeTabWidget_, this);

    // PanelCoordinator signals for window title and actions
    connect(panelCoordinator_, &PanelCoordinator::windowTitleUpdateNeeded, this,
            &MainWindow::updateWindowTitle);
    connect(panelCoordinator_, &PanelCoordinator::actionsUpdateNeeded, this,
            [this]() { connectionUiController_->updateAll(); });
}

void MainWindow::setupConnections()
{
    // Device connection error routing
    connect(deviceConnection_, &DeviceConnection::connectionError, errorHandler_,
            &ErrorHandler::handleConnectionError);
    connect(deviceConnection_->restClient(), &IRestClient::operationFailed, errorHandler_,
            &ErrorHandler::handleOperationFailed);
    connect(remoteFileModel_, &RemoteFileModel::errorOccurred, errorHandler_,
            &ErrorHandler::handleDataError);

    // Connection lifecycle signals (navigation / model management)
    connect(deviceConnection_, &DeviceConnection::stateChanged, this,
            &MainWindow::onConnectionStateChanged);
    connect(deviceConnection_, &DeviceConnection::driveInfoUpdated, this,
            &MainWindow::onDriveInfoUpdated);

    // Config file loader loading started notification
    connect(configFileLoader_, &ConfigFileLoader::loadStarted, this, [this](const QString &path) {
        statusMessageService_->showInfo(
            tr("Loading configuration: %1...").arg(QFileInfo(path).fileName()));
    });
}

void MainWindow::switchToMode(Mode mode)
{
    currentMode_ = mode;

    int pageIndex = 0;
    switch (mode) {
    case Mode::ExploreRun:
        pageIndex = 0;
        break;
    case Mode::Transfer:
        pageIndex = 1;
        break;
    case Mode::View:
        pageIndex = 2;
        break;
    case Mode::Config:
        pageIndex = 3;
        break;
    }
    modeTabWidget_->setCurrentIndex(pageIndex);

    updateWindowTitle();
    if (connectionUiController_) {
        connectionUiController_->updateAll();
    }
}

void MainWindow::updateWindowTitle()
{
    QString title = "r64u";

    if (deviceConnection_->isConnected()) {
        DeviceInfo info = deviceConnection_->deviceInfo();
        if (!info.hostname.isEmpty()) {
            title += QString(" - %1").arg(info.hostname);
        }
        if (!info.firmwareVersion.isEmpty()) {
            title += QString(" (%1)").arg(info.firmwareVersion);
        }
    }

    QString modeName;
    switch (currentMode_) {
    case Mode::ExploreRun:
        modeName = tr("Explore/Run");
        break;
    case Mode::Transfer:
        modeName = tr("Transfer");
        break;
    case Mode::View:
        modeName = tr("View");
        break;
    case Mode::Config:
        modeName = tr("Config");
        break;
    }
    title += QString(" - %1").arg(modeName);

    setWindowTitle(title);
}

void MainWindow::loadSettings()
{
    QSettings settings;
    QString host = settings.value("device/host").toString();
    bool autoConnect = settings.value("device/autoConnect", false).toBool();

    if (!host.isEmpty()) {
        // Load password from secure storage
        QString password = CredentialStore::retrievePassword("r64u", host);

        deviceConnection_->setHost(host);
        deviceConnection_->setPassword(password);

        if (autoConnect) {
            QTimer::singleShot(500, this, &MainWindow::onConnect);
        }
    }

    // Restore window geometry
    restoreGeometry(settings.value("window/geometry").toByteArray());
    restoreState(settings.value("window/state").toByteArray());

    // Load panel settings
    explorePanel_->loadSettings();
    transferPanel_->loadSettings();
    viewPanel_->loadSettings();
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue("window/geometry", saveGeometry());
    settings.setValue("window/state", saveState());

    // Save panel settings
    explorePanel_->saveSettings();
    transferPanel_->saveSettings();
    viewPanel_->saveSettings();
}

// Slots

void MainWindow::onPreferences()
{
    if (!preferencesDialog_) {
        preferencesDialog_ = new PreferencesDialog(this);
        preferencesDialog_->setSonglengthsDatabase(songlengthsDatabase_);
        preferencesDialog_->setHVSCMetadataService(hvscMetadataService_);
        preferencesDialog_->setGameBase64Service(gameBase64Service_);
    }

    if (preferencesDialog_->exec() == QDialog::Accepted) {
        QSettings settings;
        QString host = settings.value("device/host").toString();
        QString password = settings.value("device/password").toString();

        deviceConnection_->setHost(host);
        deviceConnection_->setPassword(password);
    }
}

void MainWindow::onConnect()
{
    QSettings settings;
    QString host = settings.value("device/host").toString();

    if (host.isEmpty()) {
        QMessageBox::warning(
            this, tr("Connection Error"),
            tr("No host configured. Please set the device address in Preferences."));
        onPreferences();
        return;
    }

    deviceConnection_->setHost(host);
    deviceConnection_->setPassword(settings.value("device/password").toString());
    deviceConnection_->connectToDevice();
}

void MainWindow::onDisconnect()
{
    deviceConnection_->disconnectFromDevice();
}

void MainWindow::onPowerOff()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Power Off Device"));
    msgBox.setText(tr("Are you sure you want to power off the Ultimate device?\n\n"
                      "You will lose connection and need physical access to turn it back on."));
    msgBox.setIcon(QMessageBox::Warning);
    QPushButton *powerOffButton = msgBox.addButton(tr("Power Off"), QMessageBox::DestructiveRole);
    msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
    msgBox.exec();

    if (msgBox.clickedButton() != powerOffButton) {
        return;
    }

    systemCommandController_->powerOff();
    deviceConnection_->disconnectFromDevice();
}

void MainWindow::onRefresh()
{
    if (!deviceConnection_->isConnected()) {
        return;
    }

    // Delegate to the current panel
    switch (currentMode_) {
    case Mode::ExploreRun:
        explorePanel_->refresh();
        break;
    case Mode::Transfer:
        remoteFileModel_->refresh();
        break;
    case Mode::View:
        break;
    case Mode::Config:
        configPanel_->refreshIfEmpty();
        break;
    }

    deviceConnection_->refreshDriveInfo();
}

void MainWindow::onConnectionStateChanged()
{
    DeviceConnection::ConnectionState state = deviceConnection_->state();

    switch (state) {
    case DeviceConnection::ConnectionState::Connecting:
        statusMessageService_->showInfo(tr("Connecting..."));
        break;
    case DeviceConnection::ConnectionState::Connected:
        statusMessageService_->showInfo(tr("Connected"), 3000);
        // Navigate to saved directory for the currently active panel only
        if (currentMode_ == Mode::ExploreRun) {
            QString dir = explorePanel_->currentDirectory();
            explorePanel_->setCurrentDirectory(dir.isEmpty() ? "/" : dir);
        } else if (currentMode_ == Mode::Transfer) {
            QString dir = transferPanel_->currentRemoteDir();
            transferPanel_->setCurrentRemoteDir(dir.isEmpty() ? "/" : dir);
        }
        break;
    case DeviceConnection::ConnectionState::Reconnecting:
        statusMessageService_->showWarning(tr("Reconnecting..."));
        break;
    case DeviceConnection::ConnectionState::Disconnected:
        statusMessageService_->showInfo(tr("Disconnected"), 3000);
        remoteFileModel_->clear();
        viewPanel_->stopStreamingIfActive();
        break;
    }
}

void MainWindow::onDriveInfoUpdated()
{
    explorePanel_->updateDriveInfo();
}
