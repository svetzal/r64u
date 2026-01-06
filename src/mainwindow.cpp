#include "mainwindow.h"
#include "ui/preferencesdialog.h"
#include "ui/transferqueuewidget.h"
#include "ui/filedetailspanel.h"
#include "services/deviceconnection.h"
#include "services/configfileloader.h"
#include "models/remotefilemodel.h"
#include "models/transferqueue.h"

#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QStandardPaths>
#include <QMessageBox>
#include <QSettings>
#include <QCloseEvent>
#include <QInputDialog>
#include <QFileInfo>
#include <QTimer>
#include <QDir>

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

    // Configure the config file loader
    configFileLoader_->setFtpClient(deviceConnection_->ftpClient());
    configFileLoader_->setRestClient(deviceConnection_->restClient());

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

    transferProgress_ = new QProgressBar();
    transferProgress_->setMaximumWidth(150);
    transferProgress_->setVisible(false);

    statusBar()->addWidget(driveALabel_);
    statusBar()->addWidget(new QLabel(" | "));
    statusBar()->addWidget(driveBLabel_);
    statusBar()->addPermanentWidget(transferProgress_);
    statusBar()->addPermanentWidget(connectionLabel_);
}

void MainWindow::setupExploreRunMode()
{
    exploreRunWidget_ = new QWidget();
    auto *layout = new QVBoxLayout(exploreRunWidget_);
    layout->setContentsMargins(0, 0, 0, 0);

    // Create horizontal splitter for tree view and details panel
    exploreRunSplitter_ = new QSplitter(Qt::Horizontal);

    // Left side: file tree
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

    exploreRunSplitter_->addWidget(remoteTreeView_);

    // Right side: file details panel
    fileDetailsPanel_ = new FileDetailsPanel();
    connect(fileDetailsPanel_, &FileDetailsPanel::contentRequested,
            this, &MainWindow::onFileContentRequested);
    exploreRunSplitter_->addWidget(fileDetailsPanel_);

    // Set initial splitter sizes (40% tree, 60% details)
    exploreRunSplitter_->setSizes({400, 600});

    layout->addWidget(exploreRunSplitter_);

    stackedWidget_->addWidget(exploreRunWidget_);

    // Setup context menu
    remoteContextMenu_ = new QMenu(this);
    remoteContextMenu_->addAction(tr("Play"), this, &MainWindow::onPlay);
    remoteContextMenu_->addAction(tr("Run"), this, &MainWindow::onRun);
    remoteContextMenu_->addAction(tr("Load Config"), this, &MainWindow::onLoadConfig);
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

    // Remote file browser (shares model with explore mode) - LEFT SIDE
    auto *remoteWidget = new QWidget();
    auto *remoteLayout = new QVBoxLayout(remoteWidget);
    remoteLayout->setContentsMargins(4, 4, 4, 4);

    auto *remoteLabel = new QLabel(tr("C64U Files"));
    remoteLabel->setStyleSheet("font-weight: bold;");
    remoteLayout->addWidget(remoteLabel);

    // Current remote directory indicator
    remoteCurrentDirLabel_ = new QLabel(tr("Upload to: /"));
    remoteCurrentDirLabel_->setStyleSheet("color: #0066cc; padding: 2px; background-color: #f0f8ff; border-radius: 3px;");
    remoteCurrentDirLabel_->setWordWrap(true);
    remoteLayout->addWidget(remoteCurrentDirLabel_);

    remoteTransferTreeView_ = new QTreeView();
    remoteTransferTreeView_->setModel(remoteFileModel_);
    remoteTransferTreeView_->setAlternatingRowColors(true);
    remoteTransferTreeView_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    remoteTransferTreeView_->setContextMenuPolicy(Qt::CustomContextMenu);
    remoteTransferTreeView_->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    connect(remoteTransferTreeView_, &QTreeView::customContextMenuRequested,
            this, [this](const QPoint &pos) {
        QModelIndex index = remoteTransferTreeView_->indexAt(pos);
        if (index.isValid()) {
            transferContextMenu_->exec(remoteTransferTreeView_->viewport()->mapToGlobal(pos));
        }
    });
    connect(remoteTransferTreeView_, &QTreeView::doubleClicked,
            this, &MainWindow::onRemoteTransferDoubleClicked);

    remoteLayout->addWidget(remoteTransferTreeView_);
    transferSplitter_->addWidget(remoteWidget);

    // Local file browser - RIGHT SIDE
    auto *localWidget = new QWidget();
    auto *localLayout = new QVBoxLayout(localWidget);
    localLayout->setContentsMargins(4, 4, 4, 4);

    auto *localLabel = new QLabel(tr("Local Files"));
    localLabel->setStyleSheet("font-weight: bold;");
    localLayout->addWidget(localLabel);

    // Current local directory indicator
    localCurrentDirLabel_ = new QLabel(tr("Download to: ~"));
    localCurrentDirLabel_->setStyleSheet("color: #006600; padding: 2px; background-color: #f0fff0; border-radius: 3px;");
    localCurrentDirLabel_->setWordWrap(true);
    localLayout->addWidget(localCurrentDirLabel_);

    localTreeView_ = new QTreeView();
    localTreeView_->setAlternatingRowColors(true);
    localTreeView_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    localTreeView_->setContextMenuPolicy(Qt::CustomContextMenu);

    localFileModel_ = new QFileSystemModel(this);
    QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    localFileModel_->setRootPath(homePath);
    localTreeView_->setModel(localFileModel_);
    localTreeView_->setRootIndex(localFileModel_->index(homePath));
    localTreeView_->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    // Initialize current directories
    currentLocalDir_ = homePath;
    currentRemoteTransferDir_ = "/";

    connect(localTreeView_, &QTreeView::doubleClicked,
            this, &MainWindow::onLocalDoubleClicked);
    connect(localTreeView_, &QTreeView::customContextMenuRequested,
            this, &MainWindow::onLocalContextMenu);

    localLayout->addWidget(localTreeView_);
    transferSplitter_->addWidget(localWidget);

    transferSplitter_->setSizes({400, 400});
    layout->addWidget(transferSplitter_, 1);

    // Transfer queue widget
    transferQueueWidget_ = new TransferQueueWidget();
    transferQueueWidget_->setTransferQueue(transferQueue_);
    transferQueueWidget_->setMaximumHeight(200);
    layout->addWidget(transferQueueWidget_);

    stackedWidget_->addWidget(transferWidget_);

    // Setup transfer context menu (for remote files)
    transferContextMenu_ = new QMenu(this);
    transferContextMenu_->addAction(tr("Set as Upload Destination"), this, [this]() {
        QModelIndex index = remoteTransferTreeView_->currentIndex();
        if (index.isValid()) {
            QString path = remoteFileModel_->filePath(index);
            if (remoteFileModel_->isDirectory(index)) {
                setCurrentRemoteTransferDir(path);
            } else {
                setCurrentRemoteTransferDir(QFileInfo(path).path());
            }
        }
    });
    transferContextMenu_->addSeparator();
    transferContextMenu_->addAction(tr("Download to Local Directory"), this, &MainWindow::onDownload);
    transferContextMenu_->addAction(tr("Delete"), this, &MainWindow::onDelete);
    transferContextMenu_->addSeparator();
    transferContextMenu_->addAction(tr("New Folder"), this, &MainWindow::onNewFolder);
    transferContextMenu_->addAction(tr("Refresh"), this, &MainWindow::onRefresh);

    // Setup local context menu
    localContextMenu_ = new QMenu(this);
    localContextMenu_->addAction(tr("Set as Download Destination"), this, [this]() {
        QModelIndex index = localTreeView_->currentIndex();
        if (index.isValid()) {
            QString path = localFileModel_->filePath(index);
            if (localFileModel_->isDir(index)) {
                setCurrentLocalDir(path);
            } else {
                setCurrentLocalDir(QFileInfo(path).path());
            }
        }
    });
    localContextMenu_->addSeparator();
    localContextMenu_->addAction(tr("Upload to C64U"), this, &MainWindow::onUpload);
    localContextMenu_->addSeparator();
    localContextMenu_->addAction(tr("New Folder"), this, &MainWindow::onNewLocalFolder);
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

    // FTP transfer signals (for non-queued operations)
    C64UFtpClient *ftp = deviceConnection_->ftpClient();
    connect(ftp, &C64UFtpClient::directoryCreated,
            this, &MainWindow::onDirectoryCreated);
    connect(ftp, &C64UFtpClient::fileRemoved,
            this, &MainWindow::onFileRemoved);
    connect(ftp, &C64UFtpClient::downloadToMemoryFinished,
            this, &MainWindow::onFileContentReceived);

    // Config file loader signals
    connect(configFileLoader_, &ConfigFileLoader::loadStarted,
            this, [this](const QString &path) {
        statusBar()->showMessage(tr("Loading configuration: %1...").arg(QFileInfo(path).fileName()));
    });
    connect(configFileLoader_, &ConfigFileLoader::loadFinished,
            this, &MainWindow::onConfigLoadFinished);
    connect(configFileLoader_, &ConfigFileLoader::loadFailed,
            this, &MainWindow::onConfigLoadFailed);

    // Transfer queue signals
    connect(transferQueue_, &TransferQueue::transferCompleted,
            this, [this](const QString &fileName) {
        statusBar()->showMessage(tr("Transfer complete: %1").arg(fileName), 3000);
        remoteFileModel_->refresh();
    });
    connect(transferQueue_, &TransferQueue::transferFailed,
            this, [this](const QString &fileName, const QString &error) {
        statusBar()->showMessage(tr("Transfer failed: %1 - %2").arg(fileName, error), 5000);
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
    downloadAction_->setEnabled(connected && hasSelection);  // Allow downloading directories now
    newFolderAction_->setEnabled(connected);
    refreshAction_->setEnabled(connected);
}

void MainWindow::loadSettings()
{
    QSettings settings;
    QString host = settings.value("device/host").toString();
    bool autoConnect = settings.value("device/autoConnect", false).toBool();

    if (!host.isEmpty()) {
        // Load password from secure storage (Keychain on macOS)
        QString password = CredentialStore::retrievePassword("r64u", host);

        deviceConnection_->setHost(host);
        deviceConnection_->setPassword(password);

        if (autoConnect) {
            // Defer auto-connect to after event loop starts
            // Network stack may not be ready during app initialization
            QTimer::singleShot(500, this, &MainWindow::onConnect);
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

QString MainWindow::selectedLocalPath() const
{
    QModelIndex index = localTreeView_->currentIndex();
    if (index.isValid()) {
        return localFileModel_->filePath(index);
    }
    return QString();
}

QString MainWindow::currentRemoteDirectory() const
{
    QTreeView *view = (currentMode_ == Mode::ExploreRun) ? remoteTreeView_ : remoteTransferTreeView_;
    QModelIndex index = view->currentIndex();

    if (index.isValid()) {
        QString path = remoteFileModel_->filePath(index);
        if (remoteFileModel_->isDirectory(index)) {
            return path;
        }
        // Return parent directory
        return QFileInfo(path).path();
    }

    return remoteFileModel_->rootPath();
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

void MainWindow::onEjectDriveA()
{
    deviceConnection_->restClient()->unmountImage("a");
    statusBar()->showMessage(tr("Ejecting Drive A"), 3000);
}

void MainWindow::onEjectDriveB()
{
    deviceConnection_->restClient()->unmountImage("b");
    statusBar()->showMessage(tr("Ejecting Drive B"), 3000);
}

void MainWindow::onReset()
{
    deviceConnection_->restClient()->resetMachine();
    statusBar()->showMessage(tr("Reset sent"), 3000);
}

void MainWindow::onUpload()
{
    if (!deviceConnection_->isConnected()) return;

    QString localPath = selectedLocalPath();
    if (localPath.isEmpty()) {
        statusBar()->showMessage(tr("No local file selected"), 3000);
        return;
    }

    QFileInfo fileInfo(localPath);

    // Use the current remote transfer directory as destination
    QString remoteDir = currentRemoteTransferDir_;
    if (remoteDir.isEmpty()) {
        remoteDir = "/";
    }

    if (fileInfo.isDir()) {
        // Recursive folder upload
        transferQueue_->enqueueRecursiveUpload(localPath, remoteDir);
        statusBar()->showMessage(tr("Queued folder upload: %1 → %2").arg(fileInfo.fileName()).arg(remoteDir), 3000);
    } else {
        // Single file upload
        if (!remoteDir.endsWith('/')) {
            remoteDir += '/';
        }
        QString remotePath = remoteDir + fileInfo.fileName();
        transferQueue_->enqueueUpload(localPath, remotePath);
        statusBar()->showMessage(tr("Queued upload: %1 → %2").arg(fileInfo.fileName()).arg(remoteDir), 3000);
    }
}

void MainWindow::onDownload()
{
    if (!deviceConnection_->isConnected()) return;

    QString remotePath = selectedRemotePath();
    if (remotePath.isEmpty()) {
        statusBar()->showMessage(tr("No remote file selected"), 3000);
        return;
    }

    // Use current local directory as destination in Transfer mode,
    // otherwise fall back to settings
    QString downloadDir;
    if (currentMode_ == Mode::Transfer && !currentLocalDir_.isEmpty()) {
        downloadDir = currentLocalDir_;
    } else {
        QSettings settings;
        downloadDir = settings.value("app/downloadPath",
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)).toString();
    }

    if (isSelectedDirectory()) {
        // Recursive folder download
        transferQueue_->enqueueRecursiveDownload(remotePath, downloadDir);
        QString folderName = QFileInfo(remotePath).fileName();
        statusBar()->showMessage(tr("Queued folder download: %1 → %2").arg(folderName).arg(downloadDir), 3000);
    } else {
        // Single file download
        QString fileName = QFileInfo(remotePath).fileName();
        QString localPath = downloadDir + "/" + fileName;
        transferQueue_->enqueueDownload(remotePath, localPath);
        statusBar()->showMessage(tr("Queued download: %1 → %2").arg(fileName).arg(downloadDir), 3000);
    }
}

void MainWindow::onNewFolder()
{
    if (!deviceConnection_->isConnected()) return;

    // In Transfer mode, use the current remote transfer directory
    QString remoteDir;
    if (currentMode_ == Mode::Transfer && !currentRemoteTransferDir_.isEmpty()) {
        remoteDir = currentRemoteTransferDir_;
    } else {
        remoteDir = currentRemoteDirectory();
    }

    if (remoteDir.isEmpty()) {
        remoteDir = "/";
    }

    bool ok;
    QString folderName = QInputDialog::getText(this, tr("New Remote Folder"),
        tr("Folder name:"), QLineEdit::Normal, "", &ok);

    if (!ok || folderName.isEmpty()) {
        return;
    }

    if (!remoteDir.endsWith('/')) {
        remoteDir += '/';
    }

    QString newPath = remoteDir + folderName;
    deviceConnection_->ftpClient()->makeDirectory(newPath);
    statusBar()->showMessage(tr("Creating folder %1 in %2...").arg(folderName).arg(remoteDir));
}

void MainWindow::onDelete()
{
    if (!deviceConnection_->isConnected()) return;

    QString remotePath = selectedRemotePath();
    if (remotePath.isEmpty()) {
        return;
    }

    QString fileName = QFileInfo(remotePath).fileName();

    int result = QMessageBox::question(this, tr("Delete File"),
        tr("Are you sure you want to delete '%1'?").arg(fileName),
        QMessageBox::Yes | QMessageBox::No);

    if (result != QMessageBox::Yes) {
        return;
    }

    if (isSelectedDirectory()) {
        deviceConnection_->ftpClient()->removeDirectory(remotePath);
    } else {
        deviceConnection_->ftpClient()->remove(remotePath);
    }

    statusBar()->showMessage(tr("Deleting %1...").arg(fileName));
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

    // Update file details panel
    QModelIndex index = remoteTreeView_->currentIndex();
    if (!index.isValid()) {
        fileDetailsPanel_->clear();
        return;
    }

    if (remoteFileModel_->isDirectory(index)) {
        fileDetailsPanel_->clear();
        return;
    }

    QString path = remoteFileModel_->filePath(index);
    qint64 size = remoteFileModel_->fileSize(index);
    RemoteFileModel::FileType fileType = remoteFileModel_->fileType(index);
    QString typeStr = RemoteFileModel::fileTypeString(fileType);

    fileDetailsPanel_->showFileDetails(path, size, typeStr);
}

void MainWindow::onRemoteDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;

    // For directories, let the tree view handle expand/collapse automatically
    // We only need to handle double-click actions for files
    if (!remoteFileModel_->isDirectory(index)) {
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
        case RemoteFileModel::FileType::Config:
            onLoadConfig();
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

// Transfer progress slots

void MainWindow::onUploadProgress(const QString &file, qint64 sent, qint64 total)
{
    Q_UNUSED(file)
    if (total > 0) {
        int percent = static_cast<int>((sent * 100) / total);
        transferProgress_->setValue(percent);
    }
}

void MainWindow::onUploadFinished(const QString &localPath, const QString &remotePath)
{
    Q_UNUSED(localPath)
    transferProgress_->setVisible(false);
    statusBar()->showMessage(tr("Upload complete: %1").arg(QFileInfo(remotePath).fileName()), 3000);

    // Refresh the remote file listing
    remoteFileModel_->refresh();
}

void MainWindow::onDownloadProgress(const QString &file, qint64 received, qint64 total)
{
    Q_UNUSED(file)
    if (total > 0) {
        int percent = static_cast<int>((received * 100) / total);
        transferProgress_->setValue(percent);
    }
}

void MainWindow::onDownloadFinished(const QString &remotePath, const QString &localPath)
{
    Q_UNUSED(remotePath)
    transferProgress_->setVisible(false);
    statusBar()->showMessage(tr("Download complete: %1").arg(QFileInfo(localPath).fileName()), 3000);
}

void MainWindow::onDirectoryCreated(const QString &path)
{
    statusBar()->showMessage(tr("Folder created: %1").arg(QFileInfo(path).fileName()), 3000);

    // Refresh the remote file listing
    remoteFileModel_->refresh();
}

void MainWindow::onFileRemoved(const QString &path)
{
    statusBar()->showMessage(tr("Deleted: %1").arg(QFileInfo(path).fileName()), 3000);

    // Refresh the remote file listing
    remoteFileModel_->refresh();
}

void MainWindow::onFileContentRequested(const QString &path)
{
    if (!deviceConnection_->isConnected()) {
        fileDetailsPanel_->showError(tr("Not connected"));
        return;
    }

    // Download file content to memory for preview
    deviceConnection_->ftpClient()->downloadToMemory(path);
}

void MainWindow::onFileContentReceived(const QString &remotePath, const QByteArray &data)
{
    Q_UNUSED(remotePath)
    // Display the content in the file details panel
    QString content = QString::fromUtf8(data);
    fileDetailsPanel_->showTextContent(content);
}

void MainWindow::onLoadConfig()
{
    QString path = selectedRemotePath();
    if (path.isEmpty()) return;

    RemoteFileModel::FileType type = remoteFileModel_->fileType(remoteTreeView_->currentIndex());
    if (type != RemoteFileModel::FileType::Config) {
        statusBar()->showMessage(tr("Selected file is not a configuration file"), 3000);
        return;
    }

    if (!deviceConnection_->isConnected()) {
        statusBar()->showMessage(tr("Not connected"), 3000);
        return;
    }

    configFileLoader_->loadConfigFile(path);
}

void MainWindow::onConfigLoadFinished(const QString &path)
{
    statusBar()->showMessage(tr("Configuration loaded: %1").arg(QFileInfo(path).fileName()), 5000);
}

void MainWindow::onConfigLoadFailed(const QString &path, const QString &error)
{
    statusBar()->showMessage(tr("Failed to load %1: %2").arg(QFileInfo(path).fileName()).arg(error), 5000);
    QMessageBox::warning(this, tr("Configuration Error"),
        tr("Failed to load configuration file:\n%1\n\nError: %2").arg(path).arg(error));
}

// Transfer mode slots

void MainWindow::onLocalDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;

    QString path = localFileModel_->filePath(index);

    if (localFileModel_->isDir(index)) {
        // Set as current download destination
        setCurrentLocalDir(path);
    }
    // For files, no special action - tree view handles expand/collapse
}

void MainWindow::onRemoteTransferDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;

    if (remoteFileModel_->isDirectory(index)) {
        // Set as current upload destination
        QString path = remoteFileModel_->filePath(index);
        setCurrentRemoteTransferDir(path);
    }
    // For files, no special action - tree view handles expand/collapse
}

void MainWindow::onLocalContextMenu(const QPoint &pos)
{
    QModelIndex index = localTreeView_->indexAt(pos);
    if (index.isValid()) {
        localContextMenu_->exec(localTreeView_->viewport()->mapToGlobal(pos));
    }
}

void MainWindow::onNewLocalFolder()
{
    bool ok;
    QString folderName = QInputDialog::getText(this, tr("New Local Folder"),
        tr("Folder name:"), QLineEdit::Normal, "", &ok);

    if (!ok || folderName.isEmpty()) {
        return;
    }

    QString newPath = currentLocalDir_;
    if (!newPath.endsWith('/')) {
        newPath += '/';
    }
    newPath += folderName;

    QDir dir;
    if (dir.mkdir(newPath)) {
        statusBar()->showMessage(tr("Local folder created: %1").arg(folderName), 3000);
    } else {
        QMessageBox::warning(this, tr("Error"),
            tr("Failed to create folder: %1").arg(newPath));
    }
}

void MainWindow::setCurrentLocalDir(const QString &path)
{
    currentLocalDir_ = path;

    // Shorten path for display by replacing home with ~
    QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QString displayPath = path;
    if (displayPath.startsWith(homePath)) {
        displayPath = "~" + displayPath.mid(homePath.length());
    }

    localCurrentDirLabel_->setText(tr("Download to: %1").arg(displayPath));
    statusBar()->showMessage(tr("Download destination: %1").arg(displayPath), 2000);
}

void MainWindow::setCurrentRemoteTransferDir(const QString &path)
{
    currentRemoteTransferDir_ = path;
    remoteCurrentDirLabel_->setText(tr("Upload to: %1").arg(path));
    statusBar()->showMessage(tr("Upload destination: %1").arg(path), 2000);
}
