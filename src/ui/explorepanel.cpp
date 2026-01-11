#include "explorepanel.h"
#include "filedetailspanel.h"
#include "pathnavigationwidget.h"
#include "drivestatuswidget.h"
#include "services/deviceconnection.h"
#include "services/configfileloader.h"
#include "services/filepreviewservice.h"
#include "models/remotefilemodel.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileInfo>
#include <QTimer>
#include <QSettings>

ExplorePanel::ExplorePanel(DeviceConnection *connection,
                           RemoteFileModel *model,
                           ConfigFileLoader *configLoader,
                           FilePreviewService *previewService,
                           QWidget *parent)
    : QWidget(parent)
    , deviceConnection_(connection)
    , remoteFileModel_(model)
    , configFileLoader_(configLoader)
    , previewService_(previewService)
    , currentDirectory_("/")
{
    setupUi();
    setupContextMenu();
    setupConnections();
}

void ExplorePanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Create horizontal splitter for tree view and details panel
    splitter_ = new QSplitter(Qt::Horizontal);

    // Left side: remote file browser with toolbar
    auto *remoteWidget = new QWidget();
    auto *remoteLayout = new QVBoxLayout(remoteWidget);
    remoteLayout->setContentsMargins(4, 4, 4, 4);

    auto *remoteLabel = new QLabel(tr("C64U Files"));
    remoteLabel->setStyleSheet("font-weight: bold;");
    remoteLayout->addWidget(remoteLabel);

    // Path navigation widget (spans full width)
    navWidget_ = new PathNavigationWidget(tr("Location:"));
    connect(navWidget_, &PathNavigationWidget::upClicked, this, &ExplorePanel::onParentFolder);
    remoteLayout->addWidget(navWidget_);

    // Panel toolbar
    toolBar_ = new QToolBar();
    toolBar_->setIconSize(QSize(16, 16));
    toolBar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    // File actions
    playAction_ = toolBar_->addAction(tr("Play"));
    playAction_->setToolTip(tr("Play selected SID/MOD file"));
    connect(playAction_, &QAction::triggered, this, &ExplorePanel::onPlay);

    runAction_ = toolBar_->addAction(tr("Run"));
    runAction_->setToolTip(tr("Run selected PRG/CRT file"));
    connect(runAction_, &QAction::triggered, this, &ExplorePanel::onRun);

    mountAction_ = toolBar_->addAction(tr("Mount"));
    mountAction_->setToolTip(tr("Mount selected disk image"));
    connect(mountAction_, &QAction::triggered, this, &ExplorePanel::onMount);

    toolBar_->addSeparator();

    refreshAction_ = toolBar_->addAction(tr("Refresh"));
    refreshAction_->setToolTip(tr("Refresh file listing"));
    connect(refreshAction_, &QAction::triggered, this, &ExplorePanel::onRefresh);

    remoteLayout->addWidget(toolBar_);

    // File tree
    treeView_ = new QTreeView();
    treeView_->setModel(remoteFileModel_);
    treeView_->setHeaderHidden(false);
    treeView_->setAlternatingRowColors(true);
    treeView_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    treeView_->setContextMenuPolicy(Qt::CustomContextMenu);
    treeView_->setSortingEnabled(true);
    treeView_->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    connect(treeView_->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ExplorePanel::onSelectionChanged);
    connect(treeView_, &QTreeView::doubleClicked,
            this, &ExplorePanel::onDoubleClicked);
    connect(treeView_, &QTreeView::customContextMenuRequested,
            this, &ExplorePanel::onContextMenu);

    remoteLayout->addWidget(treeView_);

    // Drive status widgets at the bottom
    drive8Status_ = new DriveStatusWidget(tr("Drive 8:"));
    remoteLayout->addWidget(drive8Status_);

    drive9Status_ = new DriveStatusWidget(tr("Drive 9:"));
    remoteLayout->addWidget(drive9Status_);

    splitter_->addWidget(remoteWidget);

    // Right side: file details panel
    fileDetailsPanel_ = new FileDetailsPanel();
    connect(fileDetailsPanel_, &FileDetailsPanel::contentRequested,
            this, &ExplorePanel::onFileContentRequested);
    splitter_->addWidget(fileDetailsPanel_);

    // Set initial splitter sizes (40% tree, 60% details)
    splitter_->setSizes({400, 600});

    layout->addWidget(splitter_);
}

void ExplorePanel::setupContextMenu()
{
    contextMenu_ = new QMenu(this);
    contextPlayAction_ = contextMenu_->addAction(tr("Play"), this, &ExplorePanel::onPlay);
    contextRunAction_ = contextMenu_->addAction(tr("Run"), this, &ExplorePanel::onRun);
    contextLoadConfigAction_ = contextMenu_->addAction(tr("Load Config"), this, &ExplorePanel::onLoadConfig);
    contextMenu_->addSeparator();
    contextMountAAction_ = contextMenu_->addAction(tr("Mount to Drive A"), this, &ExplorePanel::onMountToDriveA);
    contextMountBAction_ = contextMenu_->addAction(tr("Mount to Drive B"), this, &ExplorePanel::onMountToDriveB);
    contextMenu_->addSeparator();
    contextDownloadAction_ = contextMenu_->addAction(tr("Download"), this, &ExplorePanel::onDownload);
    contextMenu_->addSeparator();
    contextMenu_->addAction(tr("Refresh"), this, &ExplorePanel::onRefresh);
}

void ExplorePanel::setupConnections()
{
    // Subscribe to device connection state changes
    connect(deviceConnection_, &DeviceConnection::stateChanged,
            this, &ExplorePanel::onConnectionStateChanged);

    // Connect to file preview service for file content
    connect(previewService_, &FilePreviewService::previewReady,
            this, &ExplorePanel::onPreviewReady);
    connect(previewService_, &FilePreviewService::previewFailed,
            this, &ExplorePanel::onPreviewFailed);

    // Connect to config file loader
    connect(configFileLoader_, &ConfigFileLoader::loadFinished,
            this, &ExplorePanel::onConfigLoadFinished);
    connect(configFileLoader_, &ConfigFileLoader::loadFailed,
            this, &ExplorePanel::onConfigLoadFailed);

    // Connect drive status eject buttons
    connect(drive8Status_, &DriveStatusWidget::ejectClicked, this, [this]() {
        deviceConnection_->restClient()->unmountImage("a");
        emit statusMessage(tr("Ejecting Drive A"), 3000);
    });
    connect(drive9Status_, &DriveStatusWidget::ejectClicked, this, [this]() {
        deviceConnection_->restClient()->unmountImage("b");
        emit statusMessage(tr("Ejecting Drive B"), 3000);
    });
}

void ExplorePanel::setCurrentDirectory(const QString &path)
{
    currentDirectory_ = path;

    // Update the remote file model to show this folder as root
    remoteFileModel_->setRootPath(path);

    navWidget_->setPath(path);
    emit statusMessage(tr("Navigated to: %1").arg(path), 2000);

    // Enable/disable up button based on whether we can go up
    bool canGoUp = (path != "/" && !path.isEmpty());
    navWidget_->setUpEnabled(canGoUp);
}

void ExplorePanel::refresh()
{
    if (!deviceConnection_->canPerformOperations()) return;

    QModelIndex index = treeView_->currentIndex();
    if (index.isValid() && remoteFileModel_->isDirectory(index)) {
        remoteFileModel_->refresh(index);
    } else {
        remoteFileModel_->refresh();
    }

    deviceConnection_->refreshDriveInfo();
}

void ExplorePanel::updateDriveInfo()
{
    if (deviceConnection_->canPerformOperations()) {
        QList<DriveInfo> drives = deviceConnection_->driveInfo();
        for (const DriveInfo &drive : drives) {
            bool hasDisk = !drive.imageFile.isEmpty();

            if (drive.name.toLower() == "a") {
                drive8Status_->setImageName(drive.imageFile);
                drive8Status_->setMounted(hasDisk);
            } else if (drive.name.toLower() == "b") {
                drive9Status_->setImageName(drive.imageFile);
                drive9Status_->setMounted(hasDisk);
            }
        }
    } else {
        drive8Status_->setImageName(QString());
        drive8Status_->setMounted(false);
        drive9Status_->setImageName(QString());
        drive9Status_->setMounted(false);
    }
}

void ExplorePanel::loadSettings()
{
    QSettings settings;
    QString savedDir = settings.value("directories/exploreRemote", "/").toString();
    currentDirectory_ = savedDir;
}

void ExplorePanel::saveSettings()
{
    QSettings settings;
    settings.setValue("directories/exploreRemote", currentDirectory_);
}

void ExplorePanel::onConnectionStateChanged()
{
    bool canOperate = deviceConnection_->canPerformOperations();

    playAction_->setEnabled(false);
    runAction_->setEnabled(false);
    mountAction_->setEnabled(false);
    refreshAction_->setEnabled(canOperate);

    bool canGoUp = (currentDirectory_ != "/" && !currentDirectory_.isEmpty());
    navWidget_->setUpEnabled(canGoUp && canOperate);

    if (!canOperate) {
        fileDetailsPanel_->clear();
    }
}

QString ExplorePanel::selectedPath() const
{
    QModelIndex index = treeView_->currentIndex();
    if (index.isValid()) {
        return remoteFileModel_->filePath(index);
    }
    return QString();
}

bool ExplorePanel::isSelectedDirectory() const
{
    QModelIndex index = treeView_->currentIndex();
    if (index.isValid()) {
        return remoteFileModel_->isDirectory(index);
    }
    return false;
}

void ExplorePanel::onSelectionChanged()
{
    emit selectionChanged();

    // Update toolbar actions
    QString selected = selectedPath();
    bool hasSelection = !selected.isEmpty();
    bool canOperate = deviceConnection_->canPerformOperations();

    RemoteFileModel::FileType fileType = RemoteFileModel::FileType::Unknown;
    if (hasSelection) {
        QModelIndex index = treeView_->currentIndex();
        fileType = remoteFileModel_->fileType(index);
    }

    bool canPlay = hasSelection && (fileType == RemoteFileModel::FileType::SidMusic ||
                                     fileType == RemoteFileModel::FileType::ModMusic);
    bool canRun = hasSelection && (fileType == RemoteFileModel::FileType::Program ||
                                    fileType == RemoteFileModel::FileType::Cartridge ||
                                    fileType == RemoteFileModel::FileType::DiskImage);
    bool canMount = hasSelection && fileType == RemoteFileModel::FileType::DiskImage;

    playAction_->setEnabled(canOperate && canPlay);
    runAction_->setEnabled(canOperate && canRun);
    mountAction_->setEnabled(canOperate && canMount);

    // Update file details panel
    QModelIndex index = treeView_->currentIndex();
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
    QString typeStr = RemoteFileModel::fileTypeString(fileType);

    fileDetailsPanel_->showFileDetails(path, size, typeStr);
}

void ExplorePanel::onDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }

    if (remoteFileModel_->isDirectory(index)) {
        // Navigate into the directory
        QString path = remoteFileModel_->filePath(index);
        setCurrentDirectory(path);
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

void ExplorePanel::onContextMenu(const QPoint &pos)
{
    QModelIndex index = treeView_->indexAt(pos);
    if (index.isValid()) {
        // Get file type and enable/disable context menu actions accordingly
        RemoteFileModel::FileType fileType = remoteFileModel_->fileType(index);
        bool canOperate = deviceConnection_->canPerformOperations();

        bool canPlay = fileType == RemoteFileModel::FileType::SidMusic ||
                       fileType == RemoteFileModel::FileType::ModMusic;
        bool canRun = fileType == RemoteFileModel::FileType::Program ||
                      fileType == RemoteFileModel::FileType::Cartridge ||
                      fileType == RemoteFileModel::FileType::DiskImage;
        bool canMount = fileType == RemoteFileModel::FileType::DiskImage;
        bool canLoadConfig = fileType == RemoteFileModel::FileType::Config;

        contextPlayAction_->setEnabled(canOperate && canPlay);
        contextRunAction_->setEnabled(canOperate && canRun);
        contextLoadConfigAction_->setEnabled(canOperate && canLoadConfig);
        contextMountAAction_->setEnabled(canOperate && canMount);
        contextMountBAction_->setEnabled(canOperate && canMount);
        contextDownloadAction_->setEnabled(canOperate);

        contextMenu_->exec(treeView_->viewport()->mapToGlobal(pos));
    }
}

void ExplorePanel::onParentFolder()
{
    if (currentDirectory_.isEmpty() || currentDirectory_ == "/") {
        return;  // Already at root
    }

    // Get parent path
    QString parentPath = currentDirectory_;
    int lastSlash = parentPath.lastIndexOf('/');
    if (lastSlash > 0) {
        parentPath = parentPath.left(lastSlash);
    } else {
        parentPath = "/";
    }

    setCurrentDirectory(parentPath);
}

void ExplorePanel::onPlay()
{
    QString path = selectedPath();
    if (path.isEmpty()) return;

    RemoteFileModel::FileType type = remoteFileModel_->fileType(treeView_->currentIndex());

    if (type == RemoteFileModel::FileType::SidMusic) {
        deviceConnection_->restClient()->playSid(path);
        emit statusMessage(tr("Playing SID: %1").arg(path), 3000);
    } else if (type == RemoteFileModel::FileType::ModMusic) {
        deviceConnection_->restClient()->playMod(path);
        emit statusMessage(tr("Playing MOD: %1").arg(path), 3000);
    }
}

void ExplorePanel::onRun()
{
    QString path = selectedPath();
    if (path.isEmpty()) return;

    RemoteFileModel::FileType type = remoteFileModel_->fileType(treeView_->currentIndex());

    if (type == RemoteFileModel::FileType::Program) {
        deviceConnection_->restClient()->runPrg(path);
        emit statusMessage(tr("Running PRG: %1").arg(path), 3000);
    } else if (type == RemoteFileModel::FileType::Cartridge) {
        deviceConnection_->restClient()->runCrt(path);
        emit statusMessage(tr("Running CRT: %1").arg(path), 3000);
    } else if (type == RemoteFileModel::FileType::DiskImage) {
        runDiskImage(path);
    }
}

void ExplorePanel::runDiskImage(const QString &path)
{
    // Multi-step async process to run a disk image:
    // 1. Mount the disk to Drive A
    // 2. Reset the machine
    // 3. Wait for C64 to boot (3 seconds)
    // 4. Type LOAD"*",8,1 (exactly 10 chars, fits in buffer)
    // 5. Wait for buffer to be consumed, then send RETURN
    // 6. Wait for load (5 seconds)
    // 7. Type RUN + RETURN

    emit statusMessage(tr("Mounting and running: %1").arg(path));

    // Step 1: Mount the disk
    deviceConnection_->restClient()->mountImage("a", path);

    // Step 2: Reset after a brief delay to ensure mount completes
    QTimer::singleShot(500, this, [this]() {
        deviceConnection_->restClient()->resetMachine();

        // Step 3: Wait for C64 to boot
        QTimer::singleShot(3000, this, [this]() {
            emit statusMessage(tr("Loading..."));

            // Step 4: Type LOAD"*",8,1 (10 chars exactly, no newline)
            deviceConnection_->restClient()->typeText("LOAD\"*\",8,1");

            // Step 5: Wait 500ms for buffer to be consumed, then send RETURN
            QTimer::singleShot(500, this, [this]() {
                deviceConnection_->restClient()->typeText("\n");

                // Step 6: Wait for load to complete (5 seconds)
                QTimer::singleShot(5000, this, [this]() {
                    // Step 7: Type RUN + RETURN
                    deviceConnection_->restClient()->typeText("RUN\n");
                    emit statusMessage(tr("Running disk image"), 3000);
                });
            });
        });
    });
}

void ExplorePanel::onMount()
{
    onMountToDriveA();
}

void ExplorePanel::onMountToDriveA()
{
    QString path = selectedPath();
    if (path.isEmpty()) return;

    deviceConnection_->restClient()->mountImage("a", path);
    emit statusMessage(tr("Mounting to Drive A: %1").arg(path), 3000);
}

void ExplorePanel::onMountToDriveB()
{
    QString path = selectedPath();
    if (path.isEmpty()) return;

    deviceConnection_->restClient()->mountImage("b", path);
    emit statusMessage(tr("Mounting to Drive B: %1").arg(path), 3000);
}

void ExplorePanel::onLoadConfig()
{
    QString path = selectedPath();
    if (path.isEmpty()) return;

    RemoteFileModel::FileType type = remoteFileModel_->fileType(treeView_->currentIndex());
    if (type != RemoteFileModel::FileType::Config) {
        emit statusMessage(tr("Selected file is not a configuration file"), 3000);
        return;
    }

    if (!deviceConnection_->canPerformOperations()) {
        emit statusMessage(tr("Not connected"), 3000);
        return;
    }

    configFileLoader_->loadConfigFile(path);
}

void ExplorePanel::onDownload()
{
    // This will be handled by MainWindow which has access to TransferQueue
    // Emit a signal or the panel can expose the selected path for MainWindow to use
    emit statusMessage(tr("Download requested for: %1").arg(selectedPath()), 3000);
}

void ExplorePanel::onRefresh()
{
    refresh();
}

void ExplorePanel::onFileContentRequested(const QString &path)
{
    if (!deviceConnection_->canPerformOperations()) {
        fileDetailsPanel_->showError(tr("Not connected"));
        return;
    }

    // Request file content via preview service
    previewService_->requestPreview(path);
}

void ExplorePanel::onPreviewReady(const QString &remotePath, const QByteArray &data)
{
    // Check if this is a disk image file
    if (fileDetailsPanel_->isDiskImageFile(remotePath)) {
        fileDetailsPanel_->showDiskDirectory(data, remotePath);
    } else if (fileDetailsPanel_->isSidFile(remotePath)) {
        fileDetailsPanel_->showSidDetails(data, remotePath);
    } else {
        // Display the content in the file details panel as text
        QString content = QString::fromUtf8(data);
        fileDetailsPanel_->showTextContent(content);
    }
}

void ExplorePanel::onPreviewFailed(const QString &remotePath, const QString &error)
{
    Q_UNUSED(remotePath)
    fileDetailsPanel_->showError(error);
}

void ExplorePanel::onConfigLoadFinished(const QString &path)
{
    emit statusMessage(tr("Configuration loaded: %1").arg(QFileInfo(path).fileName()), 5000);
}

void ExplorePanel::onConfigLoadFailed(const QString &path, const QString &error)
{
    emit statusMessage(tr("Failed to load %1: %2").arg(QFileInfo(path).fileName()).arg(error), 5000);
    QMessageBox::warning(this, tr("Configuration Error"),
        tr("Failed to load configuration file:\n%1\n\nError: %2").arg(path).arg(error));
}
