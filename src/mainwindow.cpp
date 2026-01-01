#include "mainwindow.h"
#include "ui/preferencesdialog.h"

#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QStandardPaths>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupExploreRunMode();
    setupTransferMode();

    switchToMode(Mode::ExploreRun);
    updateWindowTitle();

    resize(1024, 768);
    setMinimumSize(800, 600);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    stackedWidget_ = new QStackedWidget(this);
    setCentralWidget(stackedWidget_);
}

void MainWindow::setupMenuBar()
{
    auto *fileMenu = menuBar()->addMenu(tr("&File"));

    auto *prefsAction = fileMenu->addAction(tr("&Preferences..."));
    prefsAction->setShortcut(QKeySequence::Preferences);
    connect(prefsAction, &QAction::triggered, this, &MainWindow::onPreferences);

    fileMenu->addSeparator();

    auto *quitAction = fileMenu->addAction(tr("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QMainWindow::close);

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

    auto *refreshAction = viewMenu->addAction(tr("&Refresh"));
    refreshAction->setShortcut(QKeySequence::Refresh);
    connect(refreshAction, &QAction::triggered, this, &MainWindow::onRefresh);

    auto *machineMenu = menuBar()->addMenu(tr("&Machine"));

    auto *resetMachineAction = machineMenu->addAction(tr("&Reset"));
    connect(resetMachineAction, &QAction::triggered, this, &MainWindow::onReset);

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

    modeCombo_ = new QComboBox();
    modeCombo_->addItem(tr("Explore/Run"));
    modeCombo_->addItem(tr("Transfer"));
    connect(modeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onModeChanged);
    mainToolBar_->addWidget(modeCombo_);

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
    remoteTreeView_->setHeaderHidden(false);
    remoteTreeView_->setAlternatingRowColors(true);
    remoteTreeView_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    remoteTreeView_->setContextMenuPolicy(Qt::CustomContextMenu);

    // Placeholder text until connected
    remoteTreeView_->setEnabled(false);

    layout->addWidget(remoteTreeView_);

    stackedWidget_->addWidget(exploreRunWidget_);
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

    // Remote file browser
    auto *remoteWidget = new QWidget();
    auto *remoteLayout = new QVBoxLayout(remoteWidget);
    remoteLayout->setContentsMargins(4, 4, 4, 4);

    auto *remoteLabel = new QLabel(tr("C64U Files"));
    remoteLabel->setStyleSheet("font-weight: bold;");
    remoteLayout->addWidget(remoteLabel);

    remoteTransferTreeView_ = new QTreeView();
    remoteTransferTreeView_->setAlternatingRowColors(true);
    remoteTransferTreeView_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    remoteTransferTreeView_->setEnabled(false);

    remoteLayout->addWidget(remoteTransferTreeView_);
    transferSplitter_->addWidget(remoteWidget);

    transferSplitter_->setSizes({400, 400});
    layout->addWidget(transferSplitter_);

    stackedWidget_->addWidget(transferWidget_);
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
}

void MainWindow::updateWindowTitle()
{
    QString title = "r64u";

    if (!connectedHost_.isEmpty()) {
        title += QString(" - %1").arg(connectedHost_);
        if (!firmwareVersion_.isEmpty()) {
            title += QString(" (%1)").arg(firmwareVersion_);
        }
    }

    title += QString(" - %1").arg(
        currentMode_ == Mode::ExploreRun ? tr("Explore/Run") : tr("Transfer"));

    setWindowTitle(title);
}

void MainWindow::onModeChanged(int index)
{
    switchToMode(index == 0 ? Mode::ExploreRun : Mode::Transfer);
}

void MainWindow::onPreferences()
{
    if (!preferencesDialog_) {
        preferencesDialog_ = new PreferencesDialog(this);
    }
    preferencesDialog_->exec();
}

void MainWindow::onPlay()
{
    // TODO: Implement SID/MOD playback
    statusBar()->showMessage(tr("Play: Not yet implemented"), 3000);
}

void MainWindow::onRun()
{
    // TODO: Implement PRG/CRT launching
    statusBar()->showMessage(tr("Run: Not yet implemented"), 3000);
}

void MainWindow::onMount()
{
    // TODO: Implement disk mounting
    statusBar()->showMessage(tr("Mount: Not yet implemented"), 3000);
}

void MainWindow::onReset()
{
    // TODO: Implement machine reset
    statusBar()->showMessage(tr("Reset: Not yet implemented"), 3000);
}

void MainWindow::onUpload()
{
    // TODO: Implement file upload
    statusBar()->showMessage(tr("Upload: Not yet implemented"), 3000);
}

void MainWindow::onDownload()
{
    // TODO: Implement file download
    statusBar()->showMessage(tr("Download: Not yet implemented"), 3000);
}

void MainWindow::onNewFolder()
{
    // TODO: Implement folder creation
    statusBar()->showMessage(tr("New Folder: Not yet implemented"), 3000);
}

void MainWindow::onRefresh()
{
    // TODO: Implement refresh
    statusBar()->showMessage(tr("Refresh: Not yet implemented"), 3000);
}
