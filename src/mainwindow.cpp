#include "mainwindow.h"
#include "ui/preferencesdialog.h"
#include "ui/connectionstatuswidget.h"
#include "ui/explorepanel.h"
#include "ui/transferpanel.h"
#include "ui/viewpanel.h"
#include "ui/configpanel.h"
#include "services/deviceconnection.h"
#include "services/configfileloader.h"
#include "services/filepreviewservice.h"
#include "services/transferservice.h"
#include "services/errorhandler.h"
#include "services/statusmessageservice.h"
#include "services/favoritesmanager.h"
#include "models/remotefilemodel.h"
#include "models/transferqueue.h"

#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QCloseEvent>
#include <QTimer>
#include <QFileInfo>

#include "services/credentialstore.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , deviceConnection_(new DeviceConnection(this))
    , remoteFileModel_(new RemoteFileModel(this))
    , transferQueue_(new TransferQueue(this))
    , configFileLoader_(new ConfigFileLoader(this))
{
    // Connect the FTP client to the model and queue
    remoteFileModel_->setFtpClient(deviceConnection_->ftpClient());
    transferQueue_->setFtpClient(deviceConnection_->ftpClient());

    // Create the file preview service
    filePreviewService_ = new FilePreviewService(deviceConnection_->ftpClient(), this);

    // Create the transfer service
    transferService_ = new TransferService(deviceConnection_, transferQueue_, this);

    // Create the error handler
    errorHandler_ = new ErrorHandler(this, this);

    // Create the status message service for coordinated status bar messages
    statusMessageService_ = new StatusMessageService(this);

    // Create the favorites manager for bookmarking remote paths
    favoritesManager_ = new FavoritesManager(this);

    // Configure the config file loader
    configFileLoader_->setFtpClient(deviceConnection_->ftpClient());
    configFileLoader_->setRestClient(deviceConnection_->restClient());

    setupUi();
    setupMenuBar();
    setupSystemToolBar();
    setupStatusBar();
    setupPanels();
    setupConnections();

    switchToMode(Mode::ExploreRun);
    updateWindowTitle();
    updateActions();
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
    // Note: Signal connection is deferred to setupConnections() to avoid
    // triggering onModeChanged before all widgets are initialized
    modeTabWidget_ = new QTabWidget();
    layout->addWidget(modeTabWidget_);

    setCentralWidget(centralContainer);
}

void MainWindow::setupMenuBar()
{
    // File menu
    auto *fileMenu = menuBar()->addMenu(tr("&File"));

    auto *connectAction = fileMenu->addAction(tr("&Connect"));
    connect(connectAction, &QAction::triggered, this, &MainWindow::onConnect);

    auto *disconnectAction = fileMenu->addAction(tr("&Disconnect"));
    connect(disconnectAction, &QAction::triggered, this, &MainWindow::onDisconnect);

    fileMenu->addSeparator();

    auto *prefsAction = fileMenu->addAction(tr("&Preferences..."));
    prefsAction->setShortcut(QKeySequence::Preferences);
    connect(prefsAction, &QAction::triggered, this, &MainWindow::onPreferences);

    fileMenu->addSeparator();

    auto *quitAction = fileMenu->addAction(tr("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QMainWindow::close);

    // View menu
    auto *viewMenu = menuBar()->addMenu(tr("&View"));

    auto *exploreAction = viewMenu->addAction(tr("&Explore/Run Mode"));
    exploreAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_1));
    connect(exploreAction, &QAction::triggered, this, [this]() {
        modeTabWidget_->setCurrentIndex(0);
    });

    auto *transferAction = viewMenu->addAction(tr("&Transfer Mode"));
    transferAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_2));
    connect(transferAction, &QAction::triggered, this, [this]() {
        modeTabWidget_->setCurrentIndex(1);
    });

    auto *viewModeAction = viewMenu->addAction(tr("&View Mode"));
    viewModeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_3));
    connect(viewModeAction, &QAction::triggered, this, [this]() {
        modeTabWidget_->setCurrentIndex(2);
    });

    auto *configModeAction = viewMenu->addAction(tr("&Config Mode"));
    configModeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_4));
    connect(configModeAction, &QAction::triggered, this, [this]() {
        modeTabWidget_->setCurrentIndex(3);
    });

    viewMenu->addSeparator();

    refreshAction_ = viewMenu->addAction(tr("&Refresh"));
    refreshAction_->setShortcut(QKeySequence::Refresh);
    connect(refreshAction_, &QAction::triggered, this, &MainWindow::onRefresh);

    // Machine menu
    auto *machineMenu = menuBar()->addMenu(tr("&Machine"));

    auto *resetMachineAction = machineMenu->addAction(tr("&Reset"));
    connect(resetMachineAction, &QAction::triggered, this, &MainWindow::onReset);

    machineMenu->addSeparator();

    auto *ejectDriveAAction = machineMenu->addAction(tr("Eject Drive &A"));
    connect(ejectDriveAAction, &QAction::triggered, this, &MainWindow::onEjectDriveA);

    auto *ejectDriveBAction = machineMenu->addAction(tr("Eject Drive &B"));
    connect(ejectDriveBAction, &QAction::triggered, this, &MainWindow::onEjectDriveB);

    // Help menu
    auto *helpMenu = menuBar()->addMenu(tr("&Help"));

    auto *aboutAction = helpMenu->addAction(tr("&About r64u"));
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, tr("About r64u"),
            tr("<h3>r64u</h3>"
               "<p>Version 0.1.0</p>"
               "<p>Remote access tool for Commodore 64 Ultimate devices.</p>"));
    });
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
    connect(resetAction_, &QAction::triggered, this, &MainWindow::onReset);

    rebootAction_ = systemToolBar_->addAction(tr("Reboot"));
    rebootAction_->setToolTip(tr("Reboot the Ultimate device"));
    connect(rebootAction_, &QAction::triggered, this, &MainWindow::onReboot);

    pauseAction_ = systemToolBar_->addAction(tr("Pause"));
    pauseAction_->setToolTip(tr("Pause C64 execution"));
    connect(pauseAction_, &QAction::triggered, this, &MainWindow::onPause);

    resumeAction_ = systemToolBar_->addAction(tr("Resume"));
    resumeAction_->setToolTip(tr("Resume C64 execution"));
    connect(resumeAction_, &QAction::triggered, this, &MainWindow::onResume);

    menuAction_ = systemToolBar_->addAction(tr("Menu"));
    menuAction_->setToolTip(tr("Press Ultimate menu button"));
    connect(menuAction_, &QAction::triggered, this, &MainWindow::onMenuButton);

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
}

void MainWindow::setupStatusBar()
{
    // Connect StatusMessageService to statusBar
    connect(statusMessageService_, &StatusMessageService::displayMessage,
            statusBar(), &QStatusBar::showMessage);

    statusMessageService_->showInfo(tr("Ready"));
}

void MainWindow::setupPanels()
{
    // Create mode panels with their dependencies
    explorePanel_ = new ExplorePanel(deviceConnection_, remoteFileModel_, configFileLoader_, filePreviewService_, favoritesManager_);
    transferPanel_ = new TransferPanel(deviceConnection_, remoteFileModel_, transferService_);
    viewPanel_ = new ViewPanel(deviceConnection_);
    configPanel_ = new ConfigPanel(deviceConnection_);

    // Add panels to tab widget
    modeTabWidget_->addTab(explorePanel_, tr("Explore/Run"));
    modeTabWidget_->addTab(transferPanel_, tr("Transfer"));
    modeTabWidget_->addTab(viewPanel_, tr("View"));
    modeTabWidget_->addTab(configPanel_, tr("Config"));

    // Connect panel status messages through the coordinator service
    connect(explorePanel_, &ExplorePanel::statusMessage,
            this, [this](const QString &msg, int timeout) {
        statusMessageService_->showInfo(msg, timeout);
    });
    connect(transferPanel_, &TransferPanel::statusMessage,
            this, [this](const QString &msg, int timeout) {
        statusMessageService_->showInfo(msg, timeout);
    });
    connect(transferPanel_, &TransferPanel::clearStatusMessages,
            statusMessageService_, &StatusMessageService::clearMessages);
    connect(viewPanel_, &ViewPanel::statusMessage,
            this, [this](const QString &msg, int timeout) {
        statusMessageService_->showInfo(msg, timeout);
    });
    connect(configPanel_, &ConfigPanel::statusMessage,
            this, [this](const QString &msg, int timeout) {
        statusMessageService_->showInfo(msg, timeout);
    });
}

void MainWindow::setupConnections()
{
    // Mode tab widget (deferred from setupUi to avoid early signal triggers)
    connect(modeTabWidget_, &QTabWidget::currentChanged,
            this, &MainWindow::onModeChanged);

    // Device connection signals
    connect(deviceConnection_, &DeviceConnection::stateChanged,
            this, &MainWindow::onConnectionStateChanged);
    connect(deviceConnection_, &DeviceConnection::deviceInfoUpdated,
            this, &MainWindow::onDeviceInfoUpdated);
    connect(deviceConnection_, &DeviceConnection::driveInfoUpdated,
            this, &MainWindow::onDriveInfoUpdated);
    // Route error signals through ErrorHandler for consistent presentation
    connect(deviceConnection_, &DeviceConnection::connectionError,
            errorHandler_, &ErrorHandler::handleConnectionError);
    connect(deviceConnection_->restClient(), &C64URestClient::operationFailed,
            errorHandler_, &ErrorHandler::handleOperationFailed);
    connect(remoteFileModel_, &RemoteFileModel::errorOccurred,
            errorHandler_, &ErrorHandler::handleDataError);

    // ErrorHandler status messages go through coordinator with appropriate priority
    connect(errorHandler_, &ErrorHandler::statusMessage,
            this, [this](const QString &msg, int timeout) {
        // ErrorHandler always uses warning or error severity
        statusMessageService_->showWarning(msg, timeout);
    });

    // REST client success signals
    connect(deviceConnection_->restClient(), &C64URestClient::operationSucceeded,
            this, &MainWindow::onOperationSucceeded);

    // Model signals for loading state (not errors)
    connect(remoteFileModel_, &RemoteFileModel::loadingStarted,
            this, [this](const QString &path) {
        statusMessageService_->showInfo(tr("Loading %1...").arg(path));
    });
    connect(remoteFileModel_, &RemoteFileModel::loadingFinished,
            this, [this](const QString &) {
        // Loading finished - no need to show a message, just let it clear naturally
    });

    // Config file loader signals
    connect(configFileLoader_, &ConfigFileLoader::loadStarted,
            this, [this](const QString &path) {
        statusMessageService_->showInfo(tr("Loading configuration: %1...").arg(QFileInfo(path).fileName()));
    });
}

void MainWindow::switchToMode(Mode mode)
{
    currentMode_ = mode;

    // Don't sync model while transfer operations are in progress
    bool canSync = deviceConnection_->isConnected() &&
                   !transferService_->isProcessing() &&
                   !transferService_->isScanning() &&
                   !transferService_->isProcessingDelete() &&
                   !transferService_->isCreatingDirectories();

    int pageIndex = 0;
    switch (mode) {
    case Mode::ExploreRun:
        pageIndex = 0;
        // Sync model with ExplorePanel's directory if needed
        if (canSync) {
            QString panelDir = explorePanel_->currentDirectory();
            if (!panelDir.isEmpty() && panelDir != remoteFileModel_->rootPath()) {
                explorePanel_->setCurrentDirectory(panelDir);
            }
        }
        break;
    case Mode::Transfer:
        pageIndex = 1;
        // Sync model with TransferPanel's directory if needed
        if (canSync) {
            QString panelDir = transferPanel_->currentRemoteDir();
            if (!panelDir.isEmpty() && panelDir != remoteFileModel_->rootPath()) {
                transferPanel_->setCurrentRemoteDir(panelDir);
            }
        }
        break;
    case Mode::View:
        pageIndex = 2;
        break;
    case Mode::Config:
        pageIndex = 3;
        // Auto-load config when switching to Config mode if connected and empty
        configPanel_->refreshIfEmpty();
        break;
    }
    modeTabWidget_->setCurrentIndex(pageIndex);

    updateWindowTitle();
    updateActions();
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

void MainWindow::updateStatusBar()
{
    if (deviceConnection_->isConnected()) {
        DeviceInfo info = deviceConnection_->deviceInfo();
        connectionStatus_->setConnected(true);
        connectionStatus_->setHostname(info.hostname.isEmpty() ? deviceConnection_->host() : info.hostname);
        connectionStatus_->setFirmwareVersion(info.firmwareVersion);
    } else {
        connectionStatus_->setConnected(false);
    }
}

void MainWindow::updateActions()
{
    // Update connect action text based on state
    DeviceConnection::ConnectionState state = deviceConnection_->state();
    switch (state) {
    case DeviceConnection::ConnectionState::Disconnected:
        connectAction_->setText(tr("Connect"));
        break;
    case DeviceConnection::ConnectionState::Connecting:
        connectAction_->setText(tr("Cancel"));
        break;
    case DeviceConnection::ConnectionState::Connected:
        connectAction_->setText(tr("Disconnect"));
        break;
    case DeviceConnection::ConnectionState::Reconnecting:
        connectAction_->setText(tr("Cancel"));
        break;
    }

    // System control actions only require REST API
    bool restConnected = deviceConnection_->isRestConnected();
    resetAction_->setEnabled(restConnected);
    rebootAction_->setEnabled(restConnected);
    pauseAction_->setEnabled(restConnected);
    resumeAction_->setEnabled(restConnected);
    menuAction_->setEnabled(restConnected);
    powerOffAction_->setEnabled(restConnected);
    refreshAction_->setEnabled(deviceConnection_->isConnected());
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

void MainWindow::onModeChanged(int index)
{
    Mode mode = Mode::ExploreRun;
    switch (index) {
    case 0:
        mode = Mode::ExploreRun;
        break;
    case 1:
        mode = Mode::Transfer;
        break;
    case 2:
        mode = Mode::View;
        break;
    case 3:
        mode = Mode::Config;
        break;
    default:
        mode = Mode::ExploreRun;
        break;
    }
    switchToMode(mode);
}

void MainWindow::onPreferences()
{
    if (!preferencesDialog_) {
        preferencesDialog_ = new PreferencesDialog(this);
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
        QMessageBox::warning(this, tr("Connection Error"),
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

void MainWindow::onReset()
{
    deviceConnection_->restClient()->resetMachine();
    statusMessageService_->showInfo(tr("Reset sent"), 3000);
}

void MainWindow::onReboot()
{
    deviceConnection_->restClient()->rebootMachine();
    statusMessageService_->showInfo(tr("Reboot sent"), 3000);
}

void MainWindow::onPause()
{
    deviceConnection_->restClient()->pauseMachine();
    statusMessageService_->showInfo(tr("Pause sent"), 3000);
}

void MainWindow::onResume()
{
    deviceConnection_->restClient()->resumeMachine();
    statusMessageService_->showInfo(tr("Resume sent"), 3000);
}

void MainWindow::onMenuButton()
{
    deviceConnection_->restClient()->pressMenuButton();
    statusMessageService_->showInfo(tr("Menu button pressed"), 3000);
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

    deviceConnection_->restClient()->powerOffMachine();
    statusMessageService_->showInfo(tr("Power off sent"), 3000);
    deviceConnection_->disconnectFromDevice();
}

void MainWindow::onEjectDriveA()
{
    deviceConnection_->restClient()->unmountImage("a");
    statusMessageService_->showInfo(tr("Ejecting Drive A"), 3000);
}

void MainWindow::onEjectDriveB()
{
    deviceConnection_->restClient()->unmountImage("b");
    statusMessageService_->showInfo(tr("Ejecting Drive B"), 3000);
}

void MainWindow::onRefresh()
{
    if (!deviceConnection_->isConnected()) return;

    // Delegate to the current panel
    switch (currentMode_) {
    case Mode::ExploreRun:
        explorePanel_->refresh();
        break;
    case Mode::Transfer:
        // Transfer panel has its own refresh
        remoteFileModel_->refresh();
        break;
    case Mode::View:
        // View mode doesn't have refresh
        break;
    case Mode::Config:
        configPanel_->refreshIfEmpty();
        break;
    }

    deviceConnection_->refreshDriveInfo();
}

void MainWindow::onConnectionStateChanged()
{
    updateWindowTitle();
    updateStatusBar();
    updateActions();

    DeviceConnection::ConnectionState state = deviceConnection_->state();

    switch (state) {
    case DeviceConnection::ConnectionState::Connecting:
        statusMessageService_->showInfo(tr("Connecting..."));
        break;
    case DeviceConnection::ConnectionState::Connected:
        statusMessageService_->showInfo(tr("Connected"), 3000);
        // Navigate to saved directory for the currently active panel only
        // (both panels share the same model, so only sync the visible one)
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

void MainWindow::onDeviceInfoUpdated()
{
    updateWindowTitle();
    updateStatusBar();
}

void MainWindow::onDriveInfoUpdated()
{
    updateStatusBar();
    explorePanel_->updateDriveInfo();
}

void MainWindow::onOperationSucceeded(const QString &operation)
{
    statusMessageService_->showInfo(tr("%1 succeeded").arg(operation), 3000);

    if (operation == "mount" || operation == "unmount") {
        deviceConnection_->refreshDriveInfo();
    }
}
