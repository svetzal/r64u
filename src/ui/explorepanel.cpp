#include "explorepanel.h"

#include "drivestatuswidget.h"
#include "explorecontextmenu.h"
#include "explorefavoritescontroller.h"
#include "fileactioncontroller.h"
#include "filedetailspanel.h"
#include "pathnavigationwidget.h"
#include "playlistwidget.h"
#include "previewcoordinator.h"

#include "models/remotefilemodel.h"
#include "services/configfileloader.h"
#include "services/deviceconnection.h"
#include "services/favoritesmanager.h"
#include "services/filebrowsercore.h"
#include "services/filepreviewservice.h"
#include "services/gamebase64service.h"
#include "services/hvscmetadataservice.h"
#include "services/playlistmanager.h"
#include "services/songlengthsdatabase.h"

#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QSettings>
#include <QShowEvent>
#include <QToolButton>
#include <QVBoxLayout>

ExplorePanel::ExplorePanel(DeviceConnection *connection, RemoteFileModel *model,
                           ConfigFileLoader *configLoader, FilePreviewService *previewService,
                           FavoritesManager *favoritesManager, PlaylistManager *playlistManager,
                           QWidget *parent)
    : QWidget(parent), deviceConnection_(connection), remoteFileModel_(model),
      playlistManager_(playlistManager), currentDirectory_("/")
{
    Q_ASSERT(deviceConnection_ && "DeviceConnection is required");
    Q_ASSERT(remoteFileModel_ && "RemoteFileModel is required");
    Q_ASSERT(configLoader && "ConfigFileLoader is required");
    Q_ASSERT(previewService && "FilePreviewService is required");
    Q_ASSERT(favoritesManager && "FavoritesManager is required");
    Q_ASSERT(playlistManager_ && "PlaylistManager is required");

    actionController_ = new FileActionController(deviceConnection_, configLoader, this);
    favoritesController_ = new ExploreFavoritesController(favoritesManager, this);
    contextMenu_ = new ExploreContextMenu(this);

    setupUi();

    previewCoordinator_ =
        new PreviewCoordinator(previewService, fileDetailsPanel_, playlistManager_, this);

    setupConnections();

    // Wire config file loader signals to preview coordinator
    connect(configLoader, &ConfigFileLoader::loadFinished, previewCoordinator_,
            &PreviewCoordinator::onConfigLoadFinished);
    connect(configLoader, &ConfigFileLoader::loadFailed, previewCoordinator_,
            &PreviewCoordinator::onConfigLoadFailed);
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

    toolBar_->addSeparator();

    // Favorites toggle action
    toggleFavoriteAction_ = toolBar_->addAction(QString::fromUtf8("☆"));
    toggleFavoriteAction_->setToolTip(tr("Add/remove current path from favorites"));
    toggleFavoriteAction_->setCheckable(true);
    toggleFavoriteAction_->setEnabled(true);
    connect(toggleFavoriteAction_, &QAction::triggered, this, [this]() {
        QString path = selectedPath().isEmpty() ? currentDirectory_ : selectedPath();
        favoritesController_->onToggleFavorite(path);
    });

    // Favorites dropdown menu
    favoritesMenu_ = new QMenu(tr("Favorites"), this);
    auto *favoritesMenuAction = toolBar_->addAction(tr("Favorites"));
    favoritesMenuAction->setToolTip(tr("Quick access to favorite locations"));
    favoritesMenuAction->setMenu(favoritesMenu_);
    // Make it a proper dropdown button
    if (auto *button =
            qobject_cast<QToolButton *>(toolBar_->widgetForAction(favoritesMenuAction))) {
        button->setPopupMode(QToolButton::InstantPopup);
    }
    connect(favoritesMenu_, &QMenu::triggered, favoritesController_,
            &ExploreFavoritesController::onFavoriteSelected);

    remoteLayout->addWidget(toolBar_);

    // File tree
    treeView_ = new QTreeView();
    if (remoteFileModel_) {
        treeView_->setModel(remoteFileModel_);
    }
    treeView_->setHeaderHidden(false);
    treeView_->setAlternatingRowColors(true);
    treeView_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    treeView_->setContextMenuPolicy(Qt::CustomContextMenu);
    treeView_->setSortingEnabled(true);
    treeView_->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    // Guard against null selection model (can happen if no model is set)
    if (auto *selModel = treeView_->selectionModel()) {
        connect(selModel, &QItemSelectionModel::selectionChanged, this,
                &ExplorePanel::onSelectionChanged);
    }
    connect(treeView_, &QTreeView::doubleClicked, this, &ExplorePanel::onDoubleClicked);
    connect(treeView_, &QTreeView::customContextMenuRequested, this, &ExplorePanel::onContextMenu);

    remoteLayout->addWidget(treeView_);

    // Drive status widgets at the bottom
    drive8Status_ = new DriveStatusWidget(tr("Drive 8:"));
    remoteLayout->addWidget(drive8Status_);

    drive9Status_ = new DriveStatusWidget(tr("Drive 9:"));
    remoteLayout->addWidget(drive9Status_);

    splitter_->addWidget(remoteWidget);

    // Right side: vertical splitter for details and playlist
    rightSplitter_ = new QSplitter(Qt::Vertical);

    // File details panel
    fileDetailsPanel_ = new FileDetailsPanel();
    rightSplitter_->addWidget(fileDetailsPanel_);

    // Playlist widget
    playlistWidget_ = new PlaylistWidget(playlistManager_);
    connect(playlistWidget_, &PlaylistWidget::statusMessage, this, &ExplorePanel::statusMessage);
    rightSplitter_->addWidget(playlistWidget_);

    // Set initial right splitter sizes (70% details, 30% playlist)
    rightSplitter_->setSizes({350, 150});

    splitter_->addWidget(rightSplitter_);

    // Set initial splitter sizes (40% tree, 60% details+playlist)
    splitter_->setSizes({400, 600});

    layout->addWidget(splitter_);

    // Wire up toolbar actions to action controller after creation
    actionController_->setActions(playAction_, runAction_, mountAction_);

    // Wire up favorites controller actions after creation
    favoritesController_->setToggleAction(toggleFavoriteAction_);
    favoritesController_->setFavoritesMenu(favoritesMenu_);
}

void ExplorePanel::setupConnections()
{
    // Subscribe to device connection state changes
    if (deviceConnection_) {
        connect(deviceConnection_, &DeviceConnection::stateChanged, this,
                &ExplorePanel::onConnectionStateChanged);
    }

    // Connect drive status eject buttons
    if (drive8Status_) {
        connect(drive8Status_, &DriveStatusWidget::ejectClicked, this,
                &ExplorePanel::ejectDriveARequested);
    }
    if (drive9Status_) {
        connect(drive9Status_, &DriveStatusWidget::ejectClicked, this,
                &ExplorePanel::ejectDriveBRequested);
    }

    // Connect action controller signals
    connect(actionController_, &FileActionController::statusMessage, this,
            &ExplorePanel::statusMessage);

    // Connect favorites controller signals
    connect(favoritesController_, &ExploreFavoritesController::navigateToPath, this,
            &ExplorePanel::setCurrentDirectory);
    connect(favoritesController_, &ExploreFavoritesController::statusMessage, this,
            &ExplorePanel::statusMessage);

    // Connect context menu signals to action/favorites controllers
    connect(contextMenu_, &ExploreContextMenu::playRequested, this, &ExplorePanel::onPlay);
    connect(contextMenu_, &ExploreContextMenu::runRequested, this, &ExplorePanel::onRun);
    connect(contextMenu_, &ExploreContextMenu::mountARequested, this,
            &ExplorePanel::onMountToDriveA);
    connect(contextMenu_, &ExploreContextMenu::mountBRequested, this,
            &ExplorePanel::onMountToDriveB);
    connect(contextMenu_, &ExploreContextMenu::downloadRequested, this, &ExplorePanel::onDownload);
    connect(contextMenu_, &ExploreContextMenu::loadConfigRequested, this,
            &ExplorePanel::onLoadConfig);
    connect(contextMenu_, &ExploreContextMenu::toggleFavoriteRequested, this, [this]() {
        QString path = selectedPath().isEmpty() ? currentDirectory_ : selectedPath();
        favoritesController_->onToggleFavorite(path);
    });
    connect(contextMenu_, &ExploreContextMenu::addToPlaylistRequested, this,
            &ExplorePanel::onAddToPlaylist);
    connect(contextMenu_, &ExploreContextMenu::refreshRequested, this, &ExplorePanel::onRefresh);

    // Connect preview coordinator
    connect(fileDetailsPanel_, &FileDetailsPanel::contentRequested, previewCoordinator_,
            &PreviewCoordinator::onFileContentRequested);
    connect(previewCoordinator_, &PreviewCoordinator::statusMessage, this,
            &ExplorePanel::statusMessage);
}

void ExplorePanel::setCurrentDirectory(const QString &path)
{
    currentDirectory_ = path;

    if (remoteFileModel_) {
        remoteFileModel_->setRootPath(path);
    }

    if (navWidget_) {
        navWidget_->setPath(path);
    }
    emit statusMessage(tr("Navigated to: %1").arg(path), 2000);

    bool canGoUp = (path != "/" && !path.isEmpty());
    if (navWidget_) {
        navWidget_->setUpEnabled(canGoUp);
    }

    // Update favorites star for the new directory
    favoritesController_->updateForPath(path);
}

void ExplorePanel::refresh()
{
    if (!deviceConnection_ || !deviceConnection_->canPerformOperations()) {
        return;
    }

    if (!remoteFileModel_ || !treeView_) {
        return;
    }

    QModelIndex index = treeView_->currentIndex();
    if (index.isValid() && remoteFileModel_->isDirectory(index)) {
        remoteFileModel_->refresh(index);
    } else {
        remoteFileModel_->refresh();
    }

    deviceConnection_->refreshDriveInfo();
}

void ExplorePanel::refreshIfStale()
{
    if (!deviceConnection_ || !deviceConnection_->canPerformOperations()) {
        return;
    }

    if (remoteFileModel_) {
        remoteFileModel_->refreshIfStale();
    }
}

void ExplorePanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refreshIfStale();
}

void ExplorePanel::updateDriveInfo()
{
    if (deviceConnection_ && deviceConnection_->canPerformOperations()) {
        QList<DriveInfo> drives = deviceConnection_->driveInfo();
        for (const DriveInfo &drive : drives) {
            bool hasDisk = !drive.imageFile.isEmpty();

            if (drive.name.toLower() == "a" && drive8Status_) {
                drive8Status_->setImageName(drive.imageFile);
                drive8Status_->setMounted(hasDisk);
            } else if (drive.name.toLower() == "b" && drive9Status_) {
                drive9Status_->setImageName(drive.imageFile);
                drive9Status_->setMounted(hasDisk);
            }
        }
    } else {
        if (drive8Status_) {
            drive8Status_->setImageName(QString());
            drive8Status_->setMounted(false);
        }
        if (drive9Status_) {
            drive9Status_->setImageName(QString());
            drive9Status_->setMounted(false);
        }
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

void ExplorePanel::setSonglengthsDatabase(SonglengthsDatabase *database)
{
    if (fileDetailsPanel_ != nullptr) {
        fileDetailsPanel_->setSonglengthsDatabase(database);
    }
}

void ExplorePanel::setHVSCMetadataService(HVSCMetadataService *service)
{
    if (fileDetailsPanel_ != nullptr) {
        fileDetailsPanel_->setHVSCMetadataService(service);
    }
}

void ExplorePanel::setGameBase64Service(GameBase64Service *service)
{
    if (fileDetailsPanel_ != nullptr) {
        fileDetailsPanel_->setGameBase64Service(service);
    }
}

void ExplorePanel::setStreamingManager(StreamingManager *manager)
{
    actionController_->setStreamingManager(manager);
}

void ExplorePanel::onConnectionStateChanged()
{
    bool canOperate = deviceConnection_ && deviceConnection_->canPerformOperations();

    // Reset all file action states (no selection context on state change)
    actionController_->updateActionStates(filetype::FileType::Unknown, false);

    if (refreshAction_) {
        refreshAction_->setEnabled(canOperate);
    }

    bool canGoUp = (currentDirectory_ != "/" && !currentDirectory_.isEmpty());
    if (navWidget_) {
        navWidget_->setUpEnabled(canGoUp && canOperate);
    }

    if (!canOperate && fileDetailsPanel_) {
        fileDetailsPanel_->clear();
    }
}

QString ExplorePanel::selectedPath() const
{
    if (!treeView_ || !remoteFileModel_) {
        return {};
    }
    QModelIndex index = treeView_->currentIndex();
    if (index.isValid()) {
        return remoteFileModel_->filePath(index);
    }
    return {};
}

bool ExplorePanel::isSelectedDirectory() const
{
    if (!treeView_ || !remoteFileModel_) {
        return false;
    }
    QModelIndex index = treeView_->currentIndex();
    if (index.isValid()) {
        return remoteFileModel_->isDirectory(index);
    }
    return false;
}

void ExplorePanel::onSelectionChanged()
{
    emit selectionChanged();

    QString selected = selectedPath();
    bool hasSelection = !selected.isEmpty();
    bool canOperate = deviceConnection_ && deviceConnection_->canPerformOperations();

    filetype::FileType fileType = filetype::FileType::Unknown;
    if (hasSelection && treeView_ && remoteFileModel_) {
        QModelIndex index = treeView_->currentIndex();
        fileType = remoteFileModel_->fileType(index);
    }

    actionController_->updateActionStates(fileType, canOperate && hasSelection);

    // Update favorites toggle button
    QString pathToCheck = hasSelection ? selected : currentDirectory_;
    favoritesController_->updateForPath(pathToCheck);

    // Update file details panel
    if (!treeView_ || !remoteFileModel_ || !fileDetailsPanel_) {
        return;
    }

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
    if (!index.isValid() || !remoteFileModel_) {
        return;
    }

    filetype::FileType type = remoteFileModel_->fileType(index);
    bool isDirectory = remoteFileModel_->isDirectory(index);
    auto action = filebrowser::resolveDoubleClickAction(type, isDirectory);

    switch (action) {
    case filebrowser::DoubleClickAction::Navigate:
        setCurrentDirectory(remoteFileModel_->filePath(index));
        break;
    case filebrowser::DoubleClickAction::Play:
        onPlay();
        break;
    case filebrowser::DoubleClickAction::Run:
        onRun();
        break;
    case filebrowser::DoubleClickAction::Mount:
        onMount();
        break;
    case filebrowser::DoubleClickAction::LoadConfig:
        onLoadConfig();
        break;
    case filebrowser::DoubleClickAction::None:
        break;
    }
}

void ExplorePanel::onContextMenu(const QPoint &pos)
{
    if (!treeView_ || !remoteFileModel_) {
        return;
    }

    QModelIndex index = treeView_->indexAt(pos);
    if (!index.isValid()) {
        return;
    }

    filetype::FileType fileType = remoteFileModel_->fileType(index);
    bool canOperate = deviceConnection_ && deviceConnection_->canPerformOperations();

    bool canAddToPlaylist = false;
    QModelIndexList selectedIndices = treeView_->selectionModel()->selectedRows();
    for (const QModelIndex &selIndex : selectedIndices) {
        if (remoteFileModel_->fileType(selIndex) == filetype::FileType::SidMusic) {
            canAddToPlaylist = true;
            break;
        }
    }

    QString path = remoteFileModel_->filePath(index);
    bool isFav = favoritesController_->isFavorite(path);

    contextMenu_->show(treeView_->viewport()->mapToGlobal(pos), fileType, canOperate,
                       canAddToPlaylist, isFav);
}

void ExplorePanel::onParentFolder()
{
    if (currentDirectory_.isEmpty() || currentDirectory_ == "/") {
        return;
    }

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
    if (!treeView_ || !remoteFileModel_) {
        return;
    }
    QString path = selectedPath();
    filetype::FileType type = remoteFileModel_->fileType(treeView_->currentIndex());
    actionController_->play(path, type);
}

void ExplorePanel::onRun()
{
    if (!treeView_ || !remoteFileModel_) {
        return;
    }
    QString path = selectedPath();
    filetype::FileType type = remoteFileModel_->fileType(treeView_->currentIndex());
    actionController_->run(path, type);
}

void ExplorePanel::onMount()
{
    onMountToDriveA();
}

void ExplorePanel::onMountToDriveA()
{
    actionController_->mountToDrive(selectedPath(), "a");
}

void ExplorePanel::onMountToDriveB()
{
    actionController_->mountToDrive(selectedPath(), "b");
}

void ExplorePanel::onLoadConfig()
{
    if (!treeView_ || !remoteFileModel_) {
        return;
    }
    QString path = selectedPath();
    filetype::FileType type = remoteFileModel_->fileType(treeView_->currentIndex());
    actionController_->loadConfig(path, type);
}

void ExplorePanel::onDownload()
{
    actionController_->download(selectedPath());
}

void ExplorePanel::onRefresh()
{
    refresh();
}

void ExplorePanel::onAddToPlaylist()
{
    if (!playlistManager_ || !treeView_ || !remoteFileModel_) {
        return;
    }

    QModelIndexList selectedIndices = treeView_->selectionModel()->selectedRows();
    if (selectedIndices.isEmpty()) {
        return;
    }

    QList<QPair<QString, filetype::FileType>> items;
    items.reserve(selectedIndices.size());
    for (const QModelIndex &index : selectedIndices) {
        items.append({remoteFileModel_->filePath(index), remoteFileModel_->fileType(index)});
    }

    auto candidates = filebrowser::filterPlaylistCandidates(items);
    for (const auto &candidate : candidates) {
        playlistManager_->addItem(candidate.path);
    }

    int addedCount = candidates.size();
    if (addedCount == 0) {
        emit statusMessage(tr("No SID files in selection"), 3000);
    } else if (addedCount == 1) {
        emit statusMessage(tr("Added 1 SID to playlist"), 3000);
    } else {
        emit statusMessage(tr("Added %1 SIDs to playlist").arg(addedCount), 3000);
    }
}
