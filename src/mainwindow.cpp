#include "mainwindow.h"
#include "ui/preferencesdialog.h"
#include "services/deviceconnection.h"
#include "models/remotefilemodel.h"

#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QStandardPaths>
#include <QMessageBox>
#include <QSettings>
#include <QCloseEvent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , deviceConnection_(new DeviceConnection(this))
    , remoteFileModel_(new RemoteFileModel(this))
{
    // Connect the FTP client to the model
    remoteFileModel_->setFtpClient(deviceConnection_->ftpClient());

    setupUi();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupExploreRunMode();
    setupTransferMode();
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
    stackedWidget_ = new QStackedWidget(this);
    setCentralWidget(stackedWidget_);
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
        modeCombo_->setCurrentIndex(0);
    });

    auto *transferAction = viewMenu->addAction(tr("&Transfer Mode"));
    transferAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_2));
    connect(transferAction, &QAction::triggered, this, [this]() {
        modeCombo_->setCurrentIndex(1);
    });

    viewMenu->addSeparator();

    refreshAction_ = viewMenu->addAction(tr("&Refresh"));
    refreshAction_->setShortcut(QKeySequence::Refresh);
    connect(refreshAction_, &QAction::triggered, this, &MainWindow::onRefresh);

    // Machine menu
    auto *machineMenu = menuBar()->addMenu(tr("&Machine"));

    auto *resetMachineAction = machineMenu->addAction(tr("&Reset"));
    connect(resetMachineAction, &QAction::triggered, this, &MainWindow::onReset);

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

void MainWindow::setupToolBar()
{
    mainToolBar_ = addToolBar(tr("Main"));
    mainToolBar_->setMovable(false);
    mainToolBar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    // Mode selector
    modeCombo_ = new QComboBox();
    modeCombo_->addItem(tr("Explore/Run"));
    modeCombo_->addItem(tr("Transfer"));
    connect(modeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onModeChanged);
    mainToolBar_->addWidget(modeCombo_);

    mainToolBar_->addSeparator();

    // Connect button
    connectAction_ = mainToolBar_->addAction(tr("Connect"));
    connectAction_->setToolTip(tr("Connect to C64U device"));
    connect(connectAction_, &QAction::triggered, this, [this]() {
        if (deviceConnection_->isConnected()) {
            onDisconnect();
        } else {
            onConnect();
        }
    });

    mainToolBar_->addSeparator();

    // Explore/Run mode actions
    playAction_ = mainToolBar_->addAction(tr("Play"));
    playAction_->setToolTip(tr("Play selected SID/MOD file"));
    connect(playAction_, &QAction::triggered, this, &MainWindow::onPlay);

    runAction_ = mainToolBar_->addAction(tr("Run"));
    runAction_->setToolTip(tr("Run selected PRG/CRT file"));
    connect(runAction_, &QAction::triggered, this, &MainWindow::onRun);

    mountAction_ = mainToolBar_->addAction(tr("Mount"));
    mountAction_->setToolTip(tr("Mount selected disk image"));
    connect(mountAction_, &QAction::triggered, this, &MainWindow::onMount);

    resetAction_ = mainToolBar_->addAction(tr("Reset"));
    resetAction_->setToolTip(tr("Reset the C64"));
    connect(resetAction_, &QAction::triggered, this, &MainWindow::onReset);

    // Transfer mode actions
    uploadAction_ = mainToolBar_->addAction(tr("Upload"));
    uploadAction_->setToolTip(tr("Upload selected files to C64U"));
    connect(uploadAction_, &QAction::triggered, this, &MainWindow::onUpload);

    downloadAction_ = mainToolBar_->addAction(tr("Download"));
    downloadAction_->setToolTip(tr("Download selected files from C64U"));
    connect(downloadAction_, &QAction::triggered, this, &MainWindow::onDownload);

    newFolderAction_ = mainToolBar_->addAction(tr("New Folder"));
    newFolderAction_->setToolTip(tr("Create new folder on C64U"));
    connect(newFolderAction_, &QAction::triggered, this, &MainWindow::onNewFolder);

    mainToolBar_->addSeparator();

    auto *prefsAction = mainToolBar_->addAction(tr("Preferences"));
    prefsAction->setToolTip(tr("Open preferences dialog"));
    connect(prefsAction, &QAction::triggered, this, &MainWindow::onPreferences);
}

void MainWindow::setupStatusBar()
{
    driveALabel_ = new QLabel(tr("Drive A: [none]"));
    driveBLabel_ = new QLabel(tr("Drive B: [none]"));
    connectionLabel_ = new QLabel(tr("Disconnected"));

    statusBar()->addWidget(driveALabel_);
    statusBar()->addWidget(new QLabel(" | "));
    statusBar()->addWidget(driveBLabel_);
    statusBar()->addPermanentWidget(connectionLabel_);
}

void MainWindow::setupExploreRunMode()
{
    exploreRunWidget_ = new QWidget();
    auto *layout = new QVBoxLayout(exploreRunWidget_);
    layout->setContentsMargins(0, 0, 0, 0);

    remoteTreeView_ = new QTreeView();
    remoteTreeView_->setModel(remoteFileModel_);
    remoteTreeView_->setHeaderHidden(false);
    remoteTreeView_->setAlternatingRowColors(true);
    remoteTreeView_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    remoteTreeView_->setContextMenuPolicy(Qt::CustomContextMenu);
    remoteTreeView_->setSortingEnabled(true);
    remoteTreeView_->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    connect(remoteTreeView_->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::onRemoteSelectionChanged);
    connect(remoteTreeView_, &QTreeView::doubleClicked,
            this, &MainWindow::onRemoteDoubleClicked);
    connect(remoteTreeView_, &QTreeView::customContextMenuRequested,
            this, &MainWindow::onRemoteContextMenu);

    layout->addWidget(remoteTreeView_);

    stackedWidget_->addWidget(exploreRunWidget_);

    // Setup context menu
    remoteContextMenu_ = new QMenu(this);
    remoteContextMenu_->addAction(tr("Play"), this, &MainWindow::onPlay);
    remoteContextMenu_->addAction(tr("Run"), this, &MainWindow::onRun);
    remoteContextMenu_->addSeparator();
    remoteContextMenu_->addAction(tr("Mount to Drive A"), this, &MainWindow::onMountToDriveA);
    remoteContextMenu_->addAction(tr("Mount to Drive B"), this, &MainWindow::onMountToDriveB);
    remoteContextMenu_->addSeparator();
    remoteContextMenu_->addAction(tr("Download"), this, &MainWindow::onDownload);
    remoteContextMenu_->addSeparator();
    remoteContextMenu_->addAction(tr("Refresh"), this, &MainWindow::onRefresh);
}

void MainWindow::setupTransferMode()
{
    transferWidget_ = new QWidget();
    auto *layout = new QVBoxLayout(transferWidget_);
    layout->setContentsMargins(0, 0, 0, 0);

    transferSplitter_ = new QSplitter(Qt::Horizontal);

    // Local file browser
    auto *localWidget = new QWidget();
    auto *localLayout = new QVBoxLayout(localWidget);
    localLayout->setContentsMargins(4, 4, 4, 4);

    auto *localLabel = new QLabel(tr("Local Files"));
    localLabel->setStyleSheet("font-weight: bold;");
    localLayout->addWidget(localLabel);

    localTreeView_ = new QTreeView();
    localTreeView_->setAlternatingRowColors(true);
    localTreeView_->setSelectionMode(QAbstractItemView::ExtendedSelection);

    localFileModel_ = new QFileSystemModel(this);
    localFileModel_->setRootPath(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    localTreeView_->setModel(localFileModel_);
    localTreeView_->setRootIndex(localFileModel_->index(
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation)));
    localTreeView_->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    localLayout->addWidget(localTreeView_);
    transferSplitter_->addWidget(localWidget);

    // Remote file browser (shares model with explore mode)
    auto *remoteWidget = new QWidget();
    auto *remoteLayout = new QVBoxLayout(remoteWidget);
    remoteLayout->setContentsMargins(4, 4, 4, 4);

    auto *remoteLabel = new QLabel(tr("C64U Files"));
    remoteLabel->setStyleSheet("font-weight: bold;");
    remoteLayout->addWidget(remoteLabel);

    remoteTransferTreeView_ = new QTreeView();
    remoteTransferTreeView_->setModel(remoteFileModel_);
    remoteTransferTreeView_->setAlternatingRowColors(true);
    remoteTransferTreeView_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    remoteTransferTreeView_->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    remoteLayout->addWidget(remoteTransferTreeView_);
    transferSplitter_->addWidget(remoteWidget);

    transferSplitter_->setSizes({400, 400});
    layout->addWidget(transferSplitter_);

    stackedWidget_->addWidget(transferWidget_);
}

void MainWindow::setupConnections()
{
    // Device connection signals
    connect(deviceConnection_, &DeviceConnection::stateChanged,
            this, &MainWindow::onConnectionStateChanged);
    connect(deviceConnection_, &DeviceConnection::deviceInfoUpdated,
            this, &MainWindow::onDeviceInfoUpdated);
    connect(deviceConnection_, &DeviceConnection::driveInfoUpdated,
            this, &MainWindow::onDriveInfoUpdated);
    connect(deviceConnection_, &DeviceConnection::connectionError,
            this, &MainWindow::onConnectionError);

    // REST client signals for operation results
    connect(deviceConnection_->restClient(), &C64URestClient::operationSucceeded,
            this, &MainWindow::onOperationSucceeded);
    connect(deviceConnection_->restClient(), &C64URestClient::operationFailed,
            this, &MainWindow::onOperationFailed);

    // Model signals
    connect(remoteFileModel_, &RemoteFileModel::loadingStarted,
            this, [this](const QString &path) {
        statusBar()->showMessage(tr("Loading %1...").arg(path));
    });
    connect(remoteFileModel_, &RemoteFileModel::loadingFinished,
            this, [this](const QString &) {
        statusBar()->clearMessage();
    });
    connect(remoteFileModel_, &RemoteFileModel::errorOccurred,
            this, [this](const QString &message) {
        statusBar()->showMessage(tr("Error: %1").arg(message), 5000);
    });
}

void MainWindow::switchToMode(Mode mode)
{
    currentMode_ = mode;

    bool exploreMode = (mode == Mode::ExploreRun);

    // Show/hide mode-specific actions
    playAction_->setVisible(exploreMode);
    runAction_->setVisible(exploreMode);
    mountAction_->setVisible(exploreMode);
    resetAction_->setVisible(exploreMode);

    uploadAction_->setVisible(!exploreMode);
    downloadAction_->setVisible(!exploreMode);
    newFolderAction_->setVisible(!exploreMode);

    // Switch stacked widget
    stackedWidget_->setCurrentIndex(exploreMode ? 0 : 1);

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

    title += QString(" - %1").arg(
        currentMode_ == Mode::ExploreRun ? tr("Explore/Run") : tr("Transfer"));

    setWindowTitle(title);
}

void MainWindow::updateStatusBar()
{
    if (deviceConnection_->isConnected()) {
        DeviceInfo info = deviceConnection_->deviceInfo();
        connectionLabel_->setText(tr("Connected: %1 (%2)")
            .arg(info.hostname.isEmpty() ? deviceConnection_->host() : info.hostname)
            .arg(info.firmwareVersion));

        // Update drive info
        QList<DriveInfo> drives = deviceConnection_->driveInfo();
        for (const DriveInfo &drive : drives) {
            QString label;
            if (drive.imageFile.isEmpty()) {
                label = tr("Drive %1: [none]").arg(drive.name.toUpper());
            } else {
                label = tr("Drive %1: %2").arg(drive.name.toUpper()).arg(drive.imageFile);
            }

            if (drive.name.toLower() == "a") {
                driveALabel_->setText(label);
            } else if (drive.name.toLower() == "b") {
                driveBLabel_->setText(label);
            }
        }
    } else {
        connectionLabel_->setText(tr("Disconnected"));
        driveALabel_->setText(tr("Drive A: [none]"));
        driveBLabel_->setText(tr("Drive B: [none]"));
    }
}

void MainWindow::updateActions()
{
    bool connected = deviceConnection_->isConnected();
    QString selectedPath = selectedRemotePath();
    bool hasSelection = !selectedPath.isEmpty();
    bool isDir = isSelectedDirectory();

    // Determine file type for enabling actions
    RemoteFileModel::FileType fileType = RemoteFileModel::FileType::Unknown;
    if (hasSelection) {
        QModelIndex index = remoteTreeView_->currentIndex();
        fileType = remoteFileModel_->fileType(index);
    }

    bool canPlay = hasSelection && (fileType == RemoteFileModel::FileType::SidMusic ||
                                     fileType == RemoteFileModel::FileType::ModMusic);
    bool canRun = hasSelection && (fileType == RemoteFileModel::FileType::Program ||
                                    fileType == RemoteFileModel::FileType::Cartridge);
    bool canMount = hasSelection && fileType == RemoteFileModel::FileType::DiskImage;

    // Update action states
    connectAction_->setText(connected ? tr("Disconnect") : tr("Connect"));
    playAction_->setEnabled(connected && canPlay);
    runAction_->setEnabled(connected && canRun);
    mountAction_->setEnabled(connected && canMount);
    resetAction_->setEnabled(connected);
    uploadAction_->setEnabled(connected);
    downloadAction_->setEnabled(connected && hasSelection && !isDir);
    newFolderAction_->setEnabled(connected);
    refreshAction_->setEnabled(connected);
}

void MainWindow::loadSettings()
{
    QSettings settings;
    QString host = settings.value("device/host").toString();
    QString password = settings.value("device/password").toString();
    bool autoConnect = settings.value("device/autoConnect", false).toBool();

    if (!host.isEmpty()) {
        deviceConnection_->setHost(host);
        deviceConnection_->setPassword(password);

        if (autoConnect) {
            onConnect();
        }
    }

    // Restore window geometry
    restoreGeometry(settings.value("window/geometry").toByteArray());
    restoreState(settings.value("window/state").toByteArray());
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue("window/geometry", saveGeometry());
    settings.setValue("window/state", saveState());
}

QString MainWindow::selectedRemotePath() const
{
    QTreeView *view = (currentMode_ == Mode::ExploreRun) ? remoteTreeView_ : remoteTransferTreeView_;
    QModelIndex index = view->currentIndex();
    if (index.isValid()) {
        return remoteFileModel_->filePath(index);
    }
    return QString();
}

bool MainWindow::isSelectedDirectory() const
{
    QTreeView *view = (currentMode_ == Mode::ExploreRun) ? remoteTreeView_ : remoteTransferTreeView_;
    QModelIndex index = view->currentIndex();
    if (index.isValid()) {
        return remoteFileModel_->isDirectory(index);
    }
    return false;
}

// Slots

void MainWindow::onModeChanged(int index)
{
    switchToMode(index == 0 ? Mode::ExploreRun : Mode::Transfer);
}

void MainWindow::onPreferences()
{
    if (!preferencesDialog_) {
        preferencesDialog_ = new PreferencesDialog(this);
    }

    if (preferencesDialog_->exec() == QDialog::Accepted) {
        // Reload settings
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

void MainWindow::onPlay()
{
    QString path = selectedRemotePath();
    if (path.isEmpty()) return;

    RemoteFileModel::FileType type = remoteFileModel_->fileType(remoteTreeView_->currentIndex());

    if (type == RemoteFileModel::FileType::SidMusic) {
        deviceConnection_->restClient()->playSid(path);
        statusBar()->showMessage(tr("Playing SID: %1").arg(path), 3000);
    } else if (type == RemoteFileModel::FileType::ModMusic) {
        deviceConnection_->restClient()->playMod(path);
        statusBar()->showMessage(tr("Playing MOD: %1").arg(path), 3000);
    }
}

void MainWindow::onRun()
{
    QString path = selectedRemotePath();
    if (path.isEmpty()) return;

    RemoteFileModel::FileType type = remoteFileModel_->fileType(remoteTreeView_->currentIndex());

    if (type == RemoteFileModel::FileType::Program) {
        deviceConnection_->restClient()->runPrg(path);
        statusBar()->showMessage(tr("Running PRG: %1").arg(path), 3000);
    } else if (type == RemoteFileModel::FileType::Cartridge) {
        deviceConnection_->restClient()->runCrt(path);
        statusBar()->showMessage(tr("Running CRT: %1").arg(path), 3000);
    }
}

void MainWindow::onMount()
{
    onMountToDriveA();
}

void MainWindow::onMountToDriveA()
{
    QString path = selectedRemotePath();
    if (path.isEmpty()) return;

    deviceConnection_->restClient()->mountImage("a", path);
    statusBar()->showMessage(tr("Mounting to Drive A: %1").arg(path), 3000);
}

void MainWindow::onMountToDriveB()
{
    QString path = selectedRemotePath();
    if (path.isEmpty()) return;

    deviceConnection_->restClient()->mountImage("b", path);
    statusBar()->showMessage(tr("Mounting to Drive B: %1").arg(path), 3000);
}

void MainWindow::onReset()
{
    deviceConnection_->restClient()->resetMachine();
    statusBar()->showMessage(tr("Reset sent"), 3000);
}

void MainWindow::onUpload()
{
    // TODO: Implement in Transfer mode issue
    statusBar()->showMessage(tr("Upload: Not yet implemented"), 3000);
}

void MainWindow::onDownload()
{
    // TODO: Implement in Transfer mode issue
    statusBar()->showMessage(tr("Download: Not yet implemented"), 3000);
}

void MainWindow::onNewFolder()
{
    // TODO: Implement in Transfer mode issue
    statusBar()->showMessage(tr("New Folder: Not yet implemented"), 3000);
}

void MainWindow::onRefresh()
{
    if (!deviceConnection_->isConnected()) return;

    QModelIndex index = remoteTreeView_->currentIndex();
    if (index.isValid() && remoteFileModel_->isDirectory(index)) {
        remoteFileModel_->refresh(index);
    } else {
        remoteFileModel_->refresh();
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
        statusBar()->showMessage(tr("Connecting..."));
        break;
    case DeviceConnection::ConnectionState::Connected:
        statusBar()->showMessage(tr("Connected"), 3000);
        // Refresh file listing
        remoteFileModel_->setRootPath("/");
        break;
    case DeviceConnection::ConnectionState::Reconnecting:
        statusBar()->showMessage(tr("Reconnecting..."));
        break;
    case DeviceConnection::ConnectionState::Disconnected:
        statusBar()->showMessage(tr("Disconnected"), 3000);
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
}

void MainWindow::onConnectionError(const QString &message)
{
    statusBar()->showMessage(tr("Connection error: %1").arg(message), 5000);
    QMessageBox::warning(this, tr("Connection Error"), message);
}

void MainWindow::onRemoteSelectionChanged()
{
    updateActions();
}

void MainWindow::onRemoteDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;

    if (remoteFileModel_->isDirectory(index)) {
        // Expand/collapse directory
        if (remoteTreeView_->isExpanded(index)) {
            remoteTreeView_->collapse(index);
        } else {
            remoteTreeView_->expand(index);
        }
    } else {
        // Execute default action based on file type
        RemoteFileModel::FileType type = remoteFileModel_->fileType(index);

        switch (type) {
        case RemoteFileModel::FileType::SidMusic:
        case RemoteFileModel::FileType::ModMusic:
            onPlay();
            break;
        case RemoteFileModel::FileType::Program:
        case RemoteFileModel::FileType::Cartridge:
            onRun();
            break;
        case RemoteFileModel::FileType::DiskImage:
            onMount();
            break;
        default:
            break;
        }
    }
}

void MainWindow::onRemoteContextMenu(const QPoint &pos)
{
    QModelIndex index = remoteTreeView_->indexAt(pos);
    if (index.isValid()) {
        remoteContextMenu_->exec(remoteTreeView_->viewport()->mapToGlobal(pos));
    }
}

void MainWindow::onOperationSucceeded(const QString &operation)
{
    statusBar()->showMessage(tr("%1 succeeded").arg(operation), 3000);

    // Refresh drive info after mount/unmount
    if (operation == "mount" || operation == "unmount") {
        deviceConnection_->refreshDriveInfo();
    }
}

void MainWindow::onOperationFailed(const QString &operation, const QString &error)
{
    statusBar()->showMessage(tr("%1 failed: %2").arg(operation).arg(error), 5000);
}
