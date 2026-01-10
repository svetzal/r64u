#include "mainwindow.h"
#include "ui/preferencesdialog.h"
#include "ui/filedetailspanel.h"
#include "ui/videodisplaywidget.h"
#include "services/deviceconnection.h"
#include "services/configfileloader.h"
#include "services/streamcontrolclient.h"
#include "services/videostreamreceiver.h"
#include "services/audiostreamreceiver.h"
#include "services/audioplaybackservice.h"
#include "services/keyboardinputservice.h"
#include "models/remotefilemodel.h"
#include "models/localfileproxymodel.h"
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
#include <QKeyEvent>
#include <QInputDialog>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QDir>
#include <QUrl>
#include <QNetworkInterface>
#include "utils/logging.h"

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
    setupViewMode();
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

    auto *viewModeAction = viewMenu->addAction(tr("&View Mode"));
    viewModeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_3));
    connect(viewModeAction, &QAction::triggered, this, [this]() {
        modeCombo_->setCurrentIndex(2);
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
    modeCombo_->addItem(tr("View"));
    connect(modeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onModeChanged);
    mainToolBar_->addWidget(modeCombo_);

    mainToolBar_->addSeparator();

    // Connect button
    connectAction_ = mainToolBar_->addAction(tr("Connect"));
    connectAction_->setToolTip(tr("Connect to C64U device"));
    connect(connectAction_, &QAction::triggered, this, [this]() {
        // Check actual state, not just isConnected()
        // In Connecting/Reconnecting states, clicking should cancel
        DeviceConnection::ConnectionState state = deviceConnection_->state();
        if (state == DeviceConnection::ConnectionState::Disconnected) {
            onConnect();
        } else {
            // Connected, Connecting, or Reconnecting - disconnect/cancel
            onDisconnect();
        }
    });

    mainToolBar_->addSeparator();

    // Machine actions (not file-specific)
    resetAction_ = mainToolBar_->addAction(tr("Reset"));
    resetAction_->setToolTip(tr("Reset the C64"));
    connect(resetAction_, &QAction::triggered, this, &MainWindow::onReset);

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

    // Left side: remote file browser with toolbar
    auto *remoteWidget = new QWidget();
    auto *remoteLayout = new QVBoxLayout(remoteWidget);
    remoteLayout->setContentsMargins(4, 4, 4, 4);

    auto *remoteLabel = new QLabel(tr("C64U Files"));
    remoteLabel->setStyleSheet("font-weight: bold;");
    remoteLayout->addWidget(remoteLabel);

    // Remote panel toolbar
    exploreRemotePanelToolBar_ = new QToolBar();
    exploreRemotePanelToolBar_->setIconSize(QSize(16, 16));
    exploreRemotePanelToolBar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    exploreRemoteUpButton_ = new QPushButton(tr("↑ Up"));
    exploreRemoteUpButton_->setToolTip(tr("Go to parent folder"));
    connect(exploreRemoteUpButton_, &QPushButton::clicked, this, &MainWindow::onExploreRemoteParentFolder);
    exploreRemotePanelToolBar_->addWidget(exploreRemoteUpButton_);

    exploreRemotePanelToolBar_->addSeparator();

    // File actions
    playAction_ = exploreRemotePanelToolBar_->addAction(tr("Play"));
    playAction_->setToolTip(tr("Play selected SID/MOD file"));
    connect(playAction_, &QAction::triggered, this, &MainWindow::onPlay);

    runAction_ = exploreRemotePanelToolBar_->addAction(tr("Run"));
    runAction_->setToolTip(tr("Run selected PRG/CRT file"));
    connect(runAction_, &QAction::triggered, this, &MainWindow::onRun);

    mountAction_ = exploreRemotePanelToolBar_->addAction(tr("Mount"));
    mountAction_->setToolTip(tr("Mount selected disk image"));
    connect(mountAction_, &QAction::triggered, this, &MainWindow::onMount);

    exploreRemotePanelToolBar_->addSeparator();

    auto *exploreRefreshAction = exploreRemotePanelToolBar_->addAction(tr("Refresh"));
    exploreRefreshAction->setToolTip(tr("Refresh file listing"));
    connect(exploreRefreshAction, &QAction::triggered, this, &MainWindow::onRefresh);

    remoteLayout->addWidget(exploreRemotePanelToolBar_);

    // Current directory indicator
    exploreRemoteCurrentDirLabel_ = new QLabel(tr("Location: /"));
    exploreRemoteCurrentDirLabel_->setStyleSheet("color: #0066cc; padding: 2px; background-color: #f0f8ff; border-radius: 3px;");
    exploreRemoteCurrentDirLabel_->setWordWrap(true);
    remoteLayout->addWidget(exploreRemoteCurrentDirLabel_);

    // File tree
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

    remoteLayout->addWidget(remoteTreeView_);
    exploreRunSplitter_->addWidget(remoteWidget);

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

    // Remote panel toolbar
    remotePanelToolBar_ = new QToolBar();
    remotePanelToolBar_->setIconSize(QSize(16, 16));
    remotePanelToolBar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    remoteUpButton_ = new QPushButton(tr("↑ Up"));
    remoteUpButton_->setToolTip(tr("Go to parent folder"));
    connect(remoteUpButton_, &QPushButton::clicked, this, &MainWindow::onRemoteParentFolder);
    remotePanelToolBar_->addWidget(remoteUpButton_);

    remotePanelToolBar_->addSeparator();

    downloadAction_ = remotePanelToolBar_->addAction(tr("Download"));
    downloadAction_->setToolTip(tr("Download selected files from C64U"));
    connect(downloadAction_, &QAction::triggered, this, &MainWindow::onDownload);

    newFolderAction_ = remotePanelToolBar_->addAction(tr("New Folder"));
    newFolderAction_->setToolTip(tr("Create new folder on C64U"));
    connect(newFolderAction_, &QAction::triggered, this, &MainWindow::onNewFolder);

    remoteRenameAction_ = remotePanelToolBar_->addAction(tr("Rename"));
    remoteRenameAction_->setToolTip(tr("Rename selected file or folder on C64U"));
    connect(remoteRenameAction_, &QAction::triggered, this, &MainWindow::onRemoteRename);

    remoteDeleteAction_ = remotePanelToolBar_->addAction(tr("Delete"));
    remoteDeleteAction_->setToolTip(tr("Delete selected file or folder on C64U"));
    connect(remoteDeleteAction_, &QAction::triggered, this, &MainWindow::onDelete);

    remotePanelToolBar_->addSeparator();

    auto *transferRefreshAction = remotePanelToolBar_->addAction(tr("Refresh"));
    transferRefreshAction->setToolTip(tr("Refresh file listing"));
    connect(transferRefreshAction, &QAction::triggered, this, &MainWindow::onRefresh);

    remoteLayout->addWidget(remotePanelToolBar_);

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
    connect(remoteTransferTreeView_->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::updateActions);

    remoteLayout->addWidget(remoteTransferTreeView_);
    transferSplitter_->addWidget(remoteWidget);

    // Local file browser - RIGHT SIDE
    auto *localWidget = new QWidget();
    auto *localLayout = new QVBoxLayout(localWidget);
    localLayout->setContentsMargins(4, 4, 4, 4);

    auto *localLabel = new QLabel(tr("Local Files"));
    localLabel->setStyleSheet("font-weight: bold;");
    localLayout->addWidget(localLabel);

    // Local panel toolbar
    localPanelToolBar_ = new QToolBar();
    localPanelToolBar_->setIconSize(QSize(16, 16));
    localPanelToolBar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    localUpButton_ = new QPushButton(tr("↑ Up"));
    localUpButton_->setToolTip(tr("Go to parent folder"));
    connect(localUpButton_, &QPushButton::clicked, this, &MainWindow::onLocalParentFolder);
    localPanelToolBar_->addWidget(localUpButton_);

    localPanelToolBar_->addSeparator();

    uploadAction_ = localPanelToolBar_->addAction(tr("Upload"));
    uploadAction_->setToolTip(tr("Upload selected files to C64U"));
    connect(uploadAction_, &QAction::triggered, this, &MainWindow::onUpload);

    localNewFolderAction_ = localPanelToolBar_->addAction(tr("New Folder"));
    localNewFolderAction_->setToolTip(tr("Create new folder in local directory"));
    connect(localNewFolderAction_, &QAction::triggered, this, &MainWindow::onNewLocalFolder);

    localRenameAction_ = localPanelToolBar_->addAction(tr("Rename"));
    localRenameAction_->setToolTip(tr("Rename selected local file or folder"));
    connect(localRenameAction_, &QAction::triggered, this, &MainWindow::onLocalRename);

    localDeleteAction_ = localPanelToolBar_->addAction(tr("Delete"));
    localDeleteAction_->setToolTip(tr("Move selected local file to trash"));
    connect(localDeleteAction_, &QAction::triggered, this, &MainWindow::onLocalDelete);

    localLayout->addWidget(localPanelToolBar_);

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

    // Use proxy model to show file sizes in bytes instead of KB/MB
    localFileProxyModel_ = new LocalFileProxyModel(this);
    localFileProxyModel_->setSourceModel(localFileModel_);
    localTreeView_->setModel(localFileProxyModel_);
    localTreeView_->setRootIndex(localFileProxyModel_->mapFromSource(localFileModel_->index(homePath)));
    localTreeView_->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    // Initialize current directories
    currentLocalDir_ = homePath;
    currentRemoteTransferDir_ = "/";

    connect(localTreeView_, &QTreeView::doubleClicked,
            this, &MainWindow::onLocalDoubleClicked);
    connect(localTreeView_, &QTreeView::customContextMenuRequested,
            this, &MainWindow::onLocalContextMenu);
    connect(localTreeView_->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::updateActions);

    localLayout->addWidget(localTreeView_);
    transferSplitter_->addWidget(localWidget);

    transferSplitter_->setSizes({400, 400});
    layout->addWidget(transferSplitter_, 1);

    // Transfer progress widget (replaces TransferQueueWidget)
    transferProgressWidget_ = new QWidget();
    transferProgressWidget_->setMaximumHeight(60);
    auto *progressLayout = new QHBoxLayout(transferProgressWidget_);
    progressLayout->setContentsMargins(8, 4, 8, 4);

    transferStatusLabel_ = new QLabel(tr("Ready"));
    transferStatusLabel_->setMinimumWidth(200);
    progressLayout->addWidget(transferStatusLabel_);

    transferProgressBar_ = new QProgressBar();
    transferProgressBar_->setMinimum(0);
    transferProgressBar_->setMaximum(100);
    transferProgressBar_->setValue(0);
    progressLayout->addWidget(transferProgressBar_, 1);

    // Initially hide the progress widget
    transferProgressWidget_->setVisible(false);
    layout->addWidget(transferProgressWidget_);

    // Setup 2-second delay timer for progress display
    transferProgressDelayTimer_ = new QTimer(this);
    transferProgressDelayTimer_->setSingleShot(true);
    connect(transferProgressDelayTimer_, &QTimer::timeout,
            this, &MainWindow::onShowTransferProgress);

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
        QModelIndex proxyIndex = localTreeView_->currentIndex();
        if (proxyIndex.isValid()) {
            QModelIndex sourceIndex = localFileProxyModel_->mapToSource(proxyIndex);
            QString path = localFileModel_->filePath(sourceIndex);
            if (localFileModel_->isDir(sourceIndex)) {
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
    localContextMenu_->addAction(tr("Rename"), this, &MainWindow::onLocalRename);
    localContextMenu_->addAction(tr("Delete"), this, &MainWindow::onLocalDelete);
}

void MainWindow::setupViewMode()
{
    viewWidget_ = new QWidget();
    auto *layout = new QVBoxLayout(viewWidget_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Create toolbar for View panel
    viewPanelToolBar_ = new QToolBar();
    viewPanelToolBar_->setMovable(false);
    viewPanelToolBar_->setIconSize(QSize(16, 16));

    startStreamAction_ = viewPanelToolBar_->addAction(tr("Start Stream"));
    startStreamAction_->setToolTip(tr("Start video and audio streaming"));
    connect(startStreamAction_, &QAction::triggered, this, &MainWindow::onStartStreaming);

    stopStreamAction_ = viewPanelToolBar_->addAction(tr("Stop Stream"));
    stopStreamAction_->setToolTip(tr("Stop streaming"));
    stopStreamAction_->setEnabled(false);
    connect(stopStreamAction_, &QAction::triggered, this, &MainWindow::onStopStreaming);

    viewPanelToolBar_->addSeparator();

    streamStatusLabel_ = new QLabel(tr("Not streaming"));
    viewPanelToolBar_->addWidget(streamStatusLabel_);

    layout->addWidget(viewPanelToolBar_);

    // Create video display widget
    videoDisplayWidget_ = new VideoDisplayWidget();
    videoDisplayWidget_->setMinimumSize(384, 272);
    layout->addWidget(videoDisplayWidget_, 1);

    stackedWidget_->addWidget(viewWidget_);

    // Create streaming services
    streamControl_ = new StreamControlClient(this);
    videoReceiver_ = new VideoStreamReceiver(this);
    audioReceiver_ = new AudioStreamReceiver(this);
    audioPlayback_ = new AudioPlaybackService(this);

    // Connect video receiver to display
    connect(videoReceiver_, &VideoStreamReceiver::frameReady,
            videoDisplayWidget_, &VideoDisplayWidget::displayFrame);
    connect(videoReceiver_, &VideoStreamReceiver::formatDetected,
            this, [this](VideoStreamReceiver::VideoFormat format) {
        onVideoFormatDetected(static_cast<int>(format));
    });

    // Connect audio receiver to playback
    connect(audioReceiver_, &AudioStreamReceiver::samplesReady,
            audioPlayback_, &AudioPlaybackService::writeSamples);

    // Connect stream control signals
    connect(streamControl_, &StreamControlClient::commandSucceeded,
            this, &MainWindow::onStreamCommandSucceeded);
    connect(streamControl_, &StreamControlClient::commandFailed,
            this, &MainWindow::onStreamCommandFailed);

    // Create keyboard input service
    keyboardInput_ = new KeyboardInputService(deviceConnection_->restClient(), this);

    // Connect video display keyboard events to keyboard service
    connect(videoDisplayWidget_, &VideoDisplayWidget::keyPressed,
            this, [this](QKeyEvent *event) {
        keyboardInput_->handleKeyPress(event);
    });
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
    connect(ftp, &C64UFtpClient::fileRenamed,
            this, &MainWindow::onFileRenamed);
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
    connect(transferQueue_, &TransferQueue::transferStarted,
            this, &MainWindow::onTransferStarted);
    connect(transferQueue_, &TransferQueue::transferCompleted,
            this, [this](const QString &fileName) {
        statusBar()->showMessage(tr("Transfer complete: %1").arg(fileName), 3000);
        transferCompletedCount_++;
        onTransferQueueChanged();
    });
    connect(transferQueue_, &TransferQueue::transferFailed,
            this, [this](const QString &fileName, const QString &error) {
        statusBar()->showMessage(tr("Transfer failed: %1 - %2").arg(fileName, error), 5000);
        transferCompletedCount_++;
        onTransferQueueChanged();
    });
    connect(transferQueue_, &TransferQueue::allTransfersCompleted,
            this, &MainWindow::onAllTransfersCompleted);
    connect(transferQueue_, &TransferQueue::queueChanged,
            this, &MainWindow::onTransferQueueChanged);
}

void MainWindow::switchToMode(Mode mode)
{
    currentMode_ = mode;

    // Mode-specific actions are on panel toolbars which are part of the mode widgets,
    // so they automatically become visible/invisible when switching modes via stacked widget

    // Switch stacked widget based on mode
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
    }
    stackedWidget_->setCurrentIndex(pageIndex);

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
    }
    title += QString(" - %1").arg(modeName);

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

    // Check remote selection
    QString selectedRemote = selectedRemotePath();
    bool hasRemoteSelection = !selectedRemote.isEmpty();

    // Check local selection (for Transfer mode)
    QString selectedLocal = selectedLocalPath();
    bool hasLocalSelection = !selectedLocal.isEmpty();

    // Determine file type for enabling actions (use mode-aware view)
    QTreeView *remoteView = (currentMode_ == Mode::ExploreRun) ? remoteTreeView_ : remoteTransferTreeView_;
    RemoteFileModel::FileType fileType = RemoteFileModel::FileType::Unknown;
    if (hasRemoteSelection) {
        QModelIndex index = remoteView->currentIndex();
        fileType = remoteFileModel_->fileType(index);
    }

    bool canPlay = hasRemoteSelection && (fileType == RemoteFileModel::FileType::SidMusic ||
                                           fileType == RemoteFileModel::FileType::ModMusic);
    bool canRun = hasRemoteSelection && (fileType == RemoteFileModel::FileType::Program ||
                                          fileType == RemoteFileModel::FileType::Cartridge);
    bool canMount = hasRemoteSelection && fileType == RemoteFileModel::FileType::DiskImage;

    // Update action states based on actual connection state
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
    playAction_->setEnabled(connected && canPlay);
    runAction_->setEnabled(connected && canRun);
    mountAction_->setEnabled(connected && canMount);
    resetAction_->setEnabled(connected);
    uploadAction_->setEnabled(connected && hasLocalSelection);
    downloadAction_->setEnabled(connected && hasRemoteSelection);
    newFolderAction_->setEnabled(connected);
    remoteDeleteAction_->setEnabled(connected && hasRemoteSelection);
    remoteRenameAction_->setEnabled(connected && hasRemoteSelection);
    localNewFolderAction_->setEnabled(true);  // Always enabled in Transfer mode
    localDeleteAction_->setEnabled(hasLocalSelection);
    localRenameAction_->setEnabled(hasLocalSelection);
    refreshAction_->setEnabled(connected);

    // View mode streaming actions
    if (startStreamAction_) {
        startStreamAction_->setEnabled(connected && !isStreaming_);
    }
    if (stopStreamAction_) {
        stopStreamAction_->setEnabled(isStreaming_);
    }
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

    // Restore directories
    QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QString savedLocalDir = settings.value("directories/local", homePath).toString();
    QString savedRemoteDir = settings.value("directories/remote", "/").toString();

    // Set local directory (updates model, label, tree view root, and up button state)
    if (QDir(savedLocalDir).exists()) {
        setCurrentLocalDir(savedLocalDir);
    }

    // Set remote transfer directory (will be used when connected)
    currentRemoteTransferDir_ = savedRemoteDir;
    remoteCurrentDirLabel_->setText(tr("Upload to: %1").arg(savedRemoteDir));

    // Initialize remote up button state (disabled until connected, then based on path)
    bool canGoUpRemote = (savedRemoteDir != "/" && !savedRemoteDir.isEmpty());
    remoteUpButton_->setEnabled(canGoUpRemote && deviceConnection_->isConnected());

    // Set explore remote directory (will be used when connected)
    QString savedExploreRemoteDir = settings.value("directories/exploreRemote", "/").toString();
    currentExploreRemoteDir_ = savedExploreRemoteDir;
    exploreRemoteCurrentDirLabel_->setText(tr("Location: %1").arg(savedExploreRemoteDir));

    // Initialize explore remote up button state (disabled until connected)
    bool canGoUpExploreRemote = (savedExploreRemoteDir != "/" && !savedExploreRemoteDir.isEmpty());
    exploreRemoteUpButton_->setEnabled(canGoUpExploreRemote && deviceConnection_->isConnected());
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue("window/geometry", saveGeometry());
    settings.setValue("window/state", saveState());

    // Save directories
    settings.setValue("directories/local", currentLocalDir_);
    settings.setValue("directories/remote", currentRemoteTransferDir_);
    settings.setValue("directories/exploreRemote", currentExploreRemoteDir_);
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
    QModelIndex proxyIndex = localTreeView_->currentIndex();
    if (proxyIndex.isValid()) {
        QModelIndex sourceIndex = localFileProxyModel_->mapToSource(proxyIndex);
        return localFileModel_->filePath(sourceIndex);
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
        // Navigate to saved remote directories (or "/" if none saved)
        setCurrentRemoteTransferDir(currentRemoteTransferDir_.isEmpty() ? "/" : currentRemoteTransferDir_);
        setCurrentExploreRemoteDir(currentExploreRemoteDir_.isEmpty() ? "/" : currentExploreRemoteDir_);
        break;
    case DeviceConnection::ConnectionState::Reconnecting:
        statusBar()->showMessage(tr("Reconnecting..."));
        break;
    case DeviceConnection::ConnectionState::Disconnected:
        statusBar()->showMessage(tr("Disconnected"), 3000);
        // Clear remote file model to prevent browsing stale cached listings
        remoteFileModel_->clear();
        // Stop streaming if active
        if (isStreaming_) {
            onStopStreaming();
        }
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
    if (!index.isValid()) {
        return;
    }

    if (remoteFileModel_->isDirectory(index)) {
        // Navigate into the directory
        QString path = remoteFileModel_->filePath(index);
        setCurrentExploreRemoteDir(path);
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

void MainWindow::onFileRenamed(const QString &oldPath, const QString &newPath)
{
    QString oldName = QFileInfo(oldPath).fileName();
    QString newName = QFileInfo(newPath).fileName();
    statusBar()->showMessage(tr("Renamed: %1 → %2").arg(oldName).arg(newName), 3000);

    // Refresh the remote file listing
    remoteFileModel_->refresh();
}

void MainWindow::onRemoteRename()
{
    if (!deviceConnection_->isConnected()) {
        return;
    }

    QString remotePath = selectedRemotePath();
    if (remotePath.isEmpty()) {
        return;
    }

    QFileInfo fileInfo(remotePath);
    QString oldName = fileInfo.fileName();
    QString itemType = isSelectedDirectory() ? tr("folder") : tr("file");

    bool ok;
    QString newName = QInputDialog::getText(this, tr("Rename Remote %1").arg(itemType),
        tr("New name:"), QLineEdit::Normal, oldName, &ok);

    if (!ok || newName.isEmpty()) {
        return;
    }

    // Check if the name changed
    if (newName == oldName) {
        return;
    }

    // Validate the new name - check for invalid characters
    if (newName.contains('/') || newName.contains('\\')) {
        QMessageBox::warning(this, tr("Invalid Name"),
            tr("The name cannot contain '/' or '\\' characters."));
        return;
    }

    // Construct the new path
    QString parentPath = fileInfo.path();
    if (!parentPath.endsWith('/')) {
        parentPath += '/';
    }
    QString newPath = parentPath + newName;

    // Perform the rename via FTP
    deviceConnection_->ftpClient()->rename(remotePath, newPath);
    statusBar()->showMessage(tr("Renaming %1...").arg(oldName));
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
    // Check if this is a disk image file
    if (fileDetailsPanel_->isDiskImageFile(remotePath)) {
        fileDetailsPanel_->showDiskDirectory(data, remotePath);
    } else {
        // Display the content in the file details panel as text
        QString content = QString::fromUtf8(data);
        fileDetailsPanel_->showTextContent(content);
    }
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

void MainWindow::onLocalDoubleClicked(const QModelIndex &proxyIndex)
{
    if (!proxyIndex.isValid()) return;

    QModelIndex sourceIndex = localFileProxyModel_->mapToSource(proxyIndex);
    QString path = localFileModel_->filePath(sourceIndex);

    if (localFileModel_->isDir(sourceIndex)) {
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

void MainWindow::onLocalDelete()
{
    QString localPath = selectedLocalPath();
    if (localPath.isEmpty()) {
        return;
    }

    QFileInfo fileInfo(localPath);
    QString itemName = fileInfo.fileName();
    QString itemType = fileInfo.isDir() ? tr("folder") : tr("file");

    int result = QMessageBox::question(this, tr("Move to Trash"),
        tr("Are you sure you want to move the %1 '%2' to the trash?").arg(itemType).arg(itemName),
        QMessageBox::Yes | QMessageBox::No);

    if (result != QMessageBox::Yes) {
        return;
    }

    QString pathInTrash;
    bool success = QFile::moveToTrash(localPath, &pathInTrash);

    if (success) {
        statusBar()->showMessage(tr("Moved to trash: %1").arg(itemName), 3000);
        // QFileSystemModel automatically detects changes via QFileSystemWatcher
        // so no explicit refresh is needed
    } else {
        QString errorMessage;
        if (!fileInfo.exists()) {
            errorMessage = tr("The %1 no longer exists.").arg(itemType);
        } else if (!fileInfo.isWritable()) {
            errorMessage = tr("Permission denied. You don't have permission to delete this %1.").arg(itemType);
        } else {
            errorMessage = tr("Failed to move the %1 to trash. The system may not support trash functionality.").arg(itemType);
        }
        QMessageBox::warning(this, tr("Delete Failed"), errorMessage);
        statusBar()->showMessage(tr("Failed to delete: %1").arg(itemName), 3000);
    }
}

void MainWindow::onLocalRename()
{
    QString localPath = selectedLocalPath();
    if (localPath.isEmpty()) {
        return;
    }

    QFileInfo fileInfo(localPath);
    QString oldName = fileInfo.fileName();
    QString itemType = fileInfo.isDir() ? tr("folder") : tr("file");

    bool ok;
    QString newName = QInputDialog::getText(this, tr("Rename %1").arg(itemType),
        tr("New name:"), QLineEdit::Normal, oldName, &ok);

    if (!ok || newName.isEmpty()) {
        return;
    }

    // Check if the name changed
    if (newName == oldName) {
        return;
    }

    // Validate the new name - check for invalid characters
    if (newName.contains('/') || newName.contains('\\')) {
        QMessageBox::warning(this, tr("Invalid Name"),
            tr("The name cannot contain '/' or '\\' characters."));
        return;
    }

    // Construct the new path
    QString newPath = fileInfo.absolutePath() + "/" + newName;

    // Check if target already exists
    if (QFileInfo::exists(newPath)) {
        QMessageBox::warning(this, tr("Rename Failed"),
            tr("A %1 with the name '%2' already exists.").arg(itemType).arg(newName));
        return;
    }

    // Perform the rename
    QDir dir;
    bool success = dir.rename(localPath, newPath);

    if (success) {
        statusBar()->showMessage(tr("Renamed: %1 → %2").arg(oldName).arg(newName), 3000);
        // QFileSystemModel automatically detects changes via QFileSystemWatcher
        // so no explicit refresh is needed
    } else {
        QString errorMessage;
        if (!fileInfo.exists()) {
            errorMessage = tr("The %1 no longer exists.").arg(itemType);
        } else if (!fileInfo.isWritable()) {
            errorMessage = tr("Permission denied. You don't have permission to rename this %1.").arg(itemType);
        } else {
            errorMessage = tr("Failed to rename the %1. Please check that you have the necessary permissions.").arg(itemType);
        }
        QMessageBox::warning(this, tr("Rename Failed"), errorMessage);
        statusBar()->showMessage(tr("Failed to rename: %1").arg(oldName), 3000);
    }
}

void MainWindow::onLocalParentFolder()
{
    QDir dir(currentLocalDir_);
    if (dir.cdUp()) {
        setCurrentLocalDir(dir.absolutePath());
    }
}

void MainWindow::onRemoteParentFolder()
{
    if (currentRemoteTransferDir_.isEmpty() || currentRemoteTransferDir_ == "/") {
        return;  // Already at root
    }

    // Get parent path
    QString parentPath = currentRemoteTransferDir_;
    int lastSlash = parentPath.lastIndexOf('/');
    if (lastSlash > 0) {
        parentPath = parentPath.left(lastSlash);
    } else {
        parentPath = "/";
    }

    setCurrentRemoteTransferDir(parentPath);
}

void MainWindow::setCurrentLocalDir(const QString &path)
{
    currentLocalDir_ = path;

    // Update tree view to show this folder as root
    localFileModel_->setRootPath(path);
    localTreeView_->setRootIndex(localFileProxyModel_->mapFromSource(localFileModel_->index(path)));

    // Shorten path for display by replacing home with ~
    QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QString displayPath = path;
    if (displayPath.startsWith(homePath)) {
        displayPath = "~" + displayPath.mid(homePath.length());
    }

    localCurrentDirLabel_->setText(tr("Download to: %1").arg(displayPath));
    statusBar()->showMessage(tr("Download destination: %1").arg(displayPath), 2000);

    // Enable/disable up button based on whether we can go up
    QDir dir(path);
    bool canGoUp = dir.cdUp();
    localUpButton_->setEnabled(canGoUp);
}

void MainWindow::setCurrentRemoteTransferDir(const QString &path)
{
    currentRemoteTransferDir_ = path;

    // Update tree view to show this folder as root
    remoteFileModel_->setRootPath(path);

    remoteCurrentDirLabel_->setText(tr("Upload to: %1").arg(path));
    statusBar()->showMessage(tr("Upload destination: %1").arg(path), 2000);

    // Enable/disable up button based on whether we can go up
    bool canGoUp = (path != "/" && !path.isEmpty());
    remoteUpButton_->setEnabled(canGoUp);
}

// Transfer progress slots

void MainWindow::onTransferStarted(const QString &fileName)
{
    Q_UNUSED(fileName)

    // If this is the first transfer, start the delay timer
    if (!transferProgressPending_ && !transferProgressWidget_->isVisible()) {
        transferProgressPending_ = true;
        transferTotalCount_ = transferQueue_->rowCount();
        transferCompletedCount_ = 0;
        transferProgressDelayTimer_->start(2000);  // 2-second delay
    }
}

void MainWindow::onTransferQueueChanged()
{
    // Update progress count
    transferTotalCount_ = transferQueue_->rowCount();

    // Update progress bar and label if visible
    if (transferProgressWidget_->isVisible()) {
        if (transferQueue_->isScanning()) {
            // Indeterminate mode while scanning directories
            transferProgressBar_->setMaximum(0);  // Indeterminate
            transferStatusLabel_->setText(tr("Scanning directories..."));
        } else if (transferTotalCount_ > 0) {
            // Determinate mode showing file progress
            transferProgressBar_->setMaximum(100);
            int progress = (transferTotalCount_ > 0)
                ? (transferCompletedCount_ * 100) / transferTotalCount_
                : 0;
            transferProgressBar_->setValue(progress);
            transferStatusLabel_->setText(tr("Transferring %1 of %2 files...")
                .arg(transferCompletedCount_ + 1)
                .arg(transferTotalCount_));
        }
    }
}

void MainWindow::onAllTransfersCompleted()
{
    // Cancel the delay timer if it hasn't fired yet
    transferProgressDelayTimer_->stop();
    transferProgressPending_ = false;

    // Hide the progress widget
    transferProgressWidget_->setVisible(false);

    // Reset counters
    transferTotalCount_ = 0;
    transferCompletedCount_ = 0;

    // Reset progress bar
    transferProgressBar_->setMaximum(100);
    transferProgressBar_->setValue(0);
    transferStatusLabel_->setText(tr("Ready"));
}

void MainWindow::onShowTransferProgress()
{
    transferProgressPending_ = false;

    // Only show if there are still transfers in progress
    if (transferQueue_->isProcessing() || transferQueue_->isScanning()) {
        transferProgressWidget_->setVisible(true);

        // Initialize display state
        if (transferQueue_->isScanning()) {
            transferProgressBar_->setMaximum(0);  // Indeterminate
            transferStatusLabel_->setText(tr("Scanning directories..."));
        } else {
            transferProgressBar_->setMaximum(100);
            int progress = (transferTotalCount_ > 0)
                ? (transferCompletedCount_ * 100) / transferTotalCount_
                : 0;
            transferProgressBar_->setValue(progress);
            transferStatusLabel_->setText(tr("Transferring %1 of %2 files...")
                .arg(transferCompletedCount_ + 1)
                .arg(transferTotalCount_));
        }
    }
}

// Explore/Run mode navigation slots

void MainWindow::onExploreRemoteParentFolder()
{
    if (currentExploreRemoteDir_.isEmpty() || currentExploreRemoteDir_ == "/") {
        return;  // Already at root
    }

    // Get parent path
    QString parentPath = currentExploreRemoteDir_;
    int lastSlash = parentPath.lastIndexOf('/');
    if (lastSlash > 0) {
        parentPath = parentPath.left(lastSlash);
    } else {
        parentPath = "/";
    }

    setCurrentExploreRemoteDir(parentPath);
}

void MainWindow::setCurrentExploreRemoteDir(const QString &path)
{
    currentExploreRemoteDir_ = path;

    // Update the remote file model to show this folder as root
    remoteFileModel_->setRootPath(path);

    exploreRemoteCurrentDirLabel_->setText(tr("Location: %1").arg(path));
    statusBar()->showMessage(tr("Navigated to: %1").arg(path), 2000);

    // Enable/disable up button based on whether we can go up
    bool canGoUp = (path != "/" && !path.isEmpty());
    exploreRemoteUpButton_->setEnabled(canGoUp);
}

// View mode streaming slots

void MainWindow::onStartStreaming()
{
    if (!deviceConnection_->isConnected()) {
        QMessageBox::warning(this, tr("Not Connected"),
                           tr("Please connect to a C64 Ultimate device first."));
        return;
    }

    // Clear any pending commands from previous sessions
    streamControl_->clearPendingCommands();

    // Extract just the host/IP from the REST client URL
    QString deviceUrl = deviceConnection_->restClient()->host();
    LOG_VERBOSE() << "MainWindow::onStartStreaming: deviceUrl from restClient:" << deviceUrl;
    QString deviceHost = QUrl(deviceUrl).host();
    if (deviceHost.isEmpty()) {
        // Maybe it's already just an IP address without scheme
        deviceHost = deviceUrl;
    }
    LOG_VERBOSE() << "MainWindow::onStartStreaming: extracted deviceHost:" << deviceHost;
    streamControl_->setHost(deviceHost);

    // Parse the device IP address
    QHostAddress deviceAddr(deviceHost);
    if (deviceAddr.isNull() || deviceAddr.protocol() != QAbstractSocket::IPv4Protocol) {
        LOG_VERBOSE() << "MainWindow::onStartStreaming: Invalid device IP - isNull:" << deviceAddr.isNull()
                 << "protocol:" << deviceAddr.protocol();
        QMessageBox::warning(this, tr("Network Error"),
                           tr("Invalid device IP address: %1").arg(deviceHost));
        return;
    }
    LOG_VERBOSE() << "MainWindow::onStartStreaming: device IP address:" << deviceAddr.toString();

    // Find our local IP address that can reach the device
    // Look for an interface on the same subnet as the C64 device
    QString targetHost;

    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp) ||
            !(iface.flags() & QNetworkInterface::IsRunning) ||
            (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol) {
                continue;
            }

            // Check if device is in same subnet
            QHostAddress subnet = entry.netmask();
            if ((entry.ip().toIPv4Address() & subnet.toIPv4Address()) ==
                (deviceAddr.toIPv4Address() & subnet.toIPv4Address())) {
                targetHost = entry.ip().toString();
                break;
            }
        }
        if (!targetHost.isEmpty()) {
            break;
        }
    }

    if (targetHost.isEmpty()) {
        LOG_VERBOSE() << "MainWindow::onStartStreaming: Could not find local IP on same subnet as device";
        QMessageBox::warning(this, tr("Network Error"),
                           tr("Could not determine local IP address for streaming.\n\n"
                              "Device IP: %1\n"
                              "Make sure you're on the same network as the C64 device.")
                           .arg(deviceAddr.toString()));
        return;
    }

    LOG_VERBOSE() << "MainWindow::onStartStreaming: Local IP for streaming:" << targetHost;

    // Start UDP receivers
    if (!videoReceiver_->bind()) {
        QMessageBox::warning(this, tr("Stream Error"),
                           tr("Failed to bind video receiver port."));
        return;
    }

    if (!audioReceiver_->bind()) {
        videoReceiver_->close();
        QMessageBox::warning(this, tr("Stream Error"),
                           tr("Failed to bind audio receiver port."));
        return;
    }

    // Start audio playback
    if (!audioPlayback_->start()) {
        videoReceiver_->close();
        audioReceiver_->close();
        QMessageBox::warning(this, tr("Stream Error"),
                           tr("Failed to start audio playback."));
        return;
    }

    // Send stream start commands to the device
    LOG_VERBOSE() << "MainWindow::onStartStreaming: Sending stream commands to device"
             << deviceHost << "- target:" << targetHost
             << "video port:" << VideoStreamReceiver::DefaultPort
             << "audio port:" << AudioStreamReceiver::DefaultPort;
    streamControl_->startAllStreams(targetHost,
                                    VideoStreamReceiver::DefaultPort,
                                    AudioStreamReceiver::DefaultPort);

    isStreaming_ = true;
    startStreamAction_->setEnabled(false);
    stopStreamAction_->setEnabled(true);
    streamStatusLabel_->setText(tr("Starting stream to %1...").arg(targetHost));
}

void MainWindow::onStopStreaming()
{
    if (!isStreaming_) {
        return;
    }

    // Send stop commands
    streamControl_->stopAllStreams();

    // Stop receivers and playback
    audioPlayback_->stop();
    videoReceiver_->close();
    audioReceiver_->close();

    // Clear display
    videoDisplayWidget_->clear();

    isStreaming_ = false;
    startStreamAction_->setEnabled(deviceConnection_->isConnected());
    stopStreamAction_->setEnabled(false);
    streamStatusLabel_->setText(tr("Not streaming"));
}

void MainWindow::onVideoFrameReady()
{
    // Frame rendering is handled by VideoDisplayWidget directly
    // This slot can be used for frame counting or stats if needed
}

void MainWindow::onVideoFormatDetected(int format)
{
    auto videoFormat = static_cast<VideoStreamReceiver::VideoFormat>(format);
    QString formatName;
    switch (videoFormat) {
    case VideoStreamReceiver::VideoFormat::PAL:
        formatName = "PAL";
        audioReceiver_->setAudioFormat(AudioStreamReceiver::AudioFormat::PAL);
        break;
    case VideoStreamReceiver::VideoFormat::NTSC:
        formatName = "NTSC";
        audioReceiver_->setAudioFormat(AudioStreamReceiver::AudioFormat::NTSC);
        break;
    default:
        formatName = "Unknown";
        break;
    }

    streamStatusLabel_->setText(tr("Streaming (%1)").arg(formatName));
}

void MainWindow::onStreamCommandSucceeded(const QString &command)
{
    statusBar()->showMessage(tr("Stream: %1").arg(command), 2000);
}

void MainWindow::onStreamCommandFailed(const QString &command, const QString &error)
{
    statusBar()->showMessage(tr("Stream failed: %1 - %2").arg(command, error), 5000);

    // If we're trying to start and it failed, clean up
    if (isStreaming_ && command.contains("start")) {
        onStopStreaming();
    }
}
