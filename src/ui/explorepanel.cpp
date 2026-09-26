#include "explorepanel.h"

#include "drivestatuswidget.h"
#include "explorecontextmenucontroller.h"
#include "explorefavoritescontroller.h"
#include "explorenavigationcontroller.h"
#include "explorepanelcore.h"
#include "fileactioncontroller.h"
#include "filedetailspanel.h"
#include "navigationviewadapter.h"
#include "pathnavigationwidget.h"
#include "pixelicons.h"
#include "playlistwidget.h"
#include "previewcoordinator.h"

#include "core/filebrowsercore.h"
#include "models/remotefilemodel.h"
#include "services/configfileloaderservice.h"
#include "services/deviceactionservice.h"
#include "services/deviceconnectionmanager.h"
#include "services/errorhandler.h"
#include "services/explorepanelservices.h"
#include "services/favoritesservice.h"
#include "services/filepreviewservice.h"
#include "services/metadataservicebundle.h"
#include "services/playlistservice.h"
#include "utils/logging.h"

#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QSettings>
#include <QShowEvent>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

ExplorePanel::ExplorePanel(DeviceConnectionManager *connection, RemoteFileModel *model,
                           const ExplorePanelServices &services, ErrorHandler *errorHandler,
                           QWidget *parent)
    : QWidget(parent), deviceConnection_(connection), remoteFileModel_(model),
      playlistService_(services.playlistService)
{
    Q_ASSERT(deviceConnection_ && "DeviceConnectionManager is required");
    Q_ASSERT(services.deviceActionService && "DeviceActionService is required");
    Q_ASSERT(remoteFileModel_ && "RemoteFileModel is required");
    Q_ASSERT(services.configLoader && "ConfigFileLoaderService is required");
    Q_ASSERT(services.previewService && "FilePreviewService is required");
    Q_ASSERT(services.favoritesService && "FavoritesService is required");
    Q_ASSERT(playlistService_ && "PlaylistService is required");

    actionController_ = new FileActionController(services.deviceActionService, deviceConnection_,
                                                 services.configLoader, errorHandler, this);
    actionController_->setPlaylistService(playlistService_);
    // selectionView_ and treeView_ are wired after setupUi() via setSelectionSource
    favoritesController_ = new ExploreFavoritesController(services.favoritesService, this);
    contextMenu_ = new ExploreContextMenuController(this);

    setupUi();

    // navViewAdapter_ and navController_ are constructed after setupUi() so treeView_ and
    // navWidget_ exist
    navViewAdapter_ = new NavigationViewAdapter(treeView_, navWidget_, this);
    navController_ = new ExploreNavigationController(deviceConnection_, remoteFileModel_,
                                                     navViewAdapter_, favoritesController_, this);

    previewCoordinator_ =
        new PreviewCoordinator(services.previewService, fileDetailsPanel_, playlistService_, this);
    previewCoordinator_->setRemoteFileModel(remoteFileModel_);

    actionController_->setSelectionSource(treeView_, remoteFileModel_);

    setupConnections();

    connect(services.configLoader, &ConfigFileLoaderService::loadFinished, previewCoordinator_,
            &PreviewCoordinator::onConfigLoadFinished);
    connect(services.configLoader, &ConfigFileLoaderService::loadFailed, previewCoordinator_,
            &PreviewCoordinator::onConfigLoadFailed);
}

void ExplorePanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    splitter_ = new QSplitter(Qt::Horizontal);

    // Left side: remote file browser with toolbar
    auto *remoteWidget = new QWidget();
    auto *remoteLayout = new QVBoxLayout(remoteWidget);
    remoteLayout->setContentsMargins(4, 4, 4, 4);

    auto *remoteLabel = new QLabel(tr("C64U Files"));
    remoteLabel->setStyleSheet("font-weight: bold;");
    remoteLayout->addWidget(remoteLabel);

    // Path navigation widget
    navWidget_ = new PathNavigationWidget(tr("Location:"));
    connect(navWidget_, &PathNavigationWidget::upClicked, this, &ExplorePanel::onParentFolder);
    remoteLayout->addWidget(navWidget_);

    // Panel toolbar
    toolBar_ = new QToolBar();
    toolBar_->setIconSize(QSize(16, 16));
    toolBar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    using pixeliconcore::Icon;
    playAction_ = toolBar_->addAction(pixelicons::icon(Icon::Play), tr("Play"));
    playAction_->setToolTip(tr("Play selected SID/MOD file"));
    connect(playAction_, &QAction::triggered, actionController_,
            &FileActionController::playSelection);

    runAction_ = toolBar_->addAction(pixelicons::icon(Icon::Run), tr("Run"));
    runAction_->setToolTip(tr("Run selected PRG/CRT file"));
    connect(runAction_, &QAction::triggered, actionController_,
            &FileActionController::runSelection);

    mountAction_ = toolBar_->addAction(pixelicons::icon(Icon::Mount), tr("Mount"));
    mountAction_->setToolTip(tr("Mount selected disk image"));
    connect(mountAction_, &QAction::triggered, this,
            [this]() { actionController_->mountToDriveSelection("a"); });

    toolBar_->addSeparator();

    refreshAction_ = toolBar_->addAction(pixelicons::icon(Icon::Refresh), tr("Refresh"));
    refreshAction_->setToolTip(tr("Refresh file listing"));
    connect(refreshAction_, &QAction::triggered, this, [this]() { refresh(); });

    toolBar_->addSeparator();

    toggleFavoriteAction_ =
        toolBar_->addAction(pixelicons::icon(Icon::StarOutline), tr("Favorite"));
    toggleFavoriteAction_->setToolTip(tr("Add/remove current path from favorites"));
    toggleFavoriteAction_->setCheckable(true);
    toggleFavoriteAction_->setEnabled(true);
    connect(toggleFavoriteAction_, &QAction::triggered, this, [this]() {
        QString path =
            selectedPath().isEmpty() ? navController_->currentDirectory() : selectedPath();
        favoritesController_->onToggleFavorite(path);
    });

    favoritesMenu_ = new QMenu(tr("Favorites"), this);
    auto *favoritesMenuAction =
        toolBar_->addAction(pixelicons::icon(Icon::Favorites), tr("Favorites"));
    favoritesMenuAction->setToolTip(tr("Quick access to favorite locations"));
    favoritesMenuAction->setMenu(favoritesMenu_);
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
    // Rows can be dragged onto the playlist (or the Transfer panes)
    treeView_->setDragEnabled(true);
    treeView_->setDragDropMode(QAbstractItemView::DragOnly);
    treeView_->setDefaultDropAction(Qt::CopyAction);

    if (auto *selModel = treeView_->selectionModel()) {
        connect(selModel, &QItemSelectionModel::selectionChanged, this,
                &ExplorePanel::onSelectionChanged);
    }
    connect(treeView_, &QTreeView::doubleClicked, this, &ExplorePanel::onDoubleClicked);
    connect(treeView_, &QTreeView::customContextMenuRequested, this, &ExplorePanel::onContextMenu);

    remoteLayout->addWidget(treeView_);

    drive8Status_ = new DriveStatusWidget(tr("Drive A (8)"));
    remoteLayout->addWidget(drive8Status_);

    drive9Status_ = new DriveStatusWidget(tr("Drive B (9)"));
    remoteLayout->addWidget(drive9Status_);

    splitter_->addWidget(remoteWidget);

    // Right side: vertical splitter for details and playlist
    rightSplitter_ = new QSplitter(Qt::Vertical);

    fileDetailsPanel_ = new FileDetailsPanel();
    rightSplitter_->addWidget(fileDetailsPanel_);

    playlistWidget_ = new PlaylistWidget(playlistService_);
    connect(playlistWidget_, &PlaylistWidget::statusMessage, this, &ExplorePanel::statusMessage);
    connect(playlistWidget_, &PlaylistWidget::collapsedChanged, this,
            &ExplorePanel::onPlaylistCollapsedChanged);
    rightSplitter_->addWidget(playlistWidget_);

    // The details screen takes whatever the playlist gives up
    rightSplitter_->setStretchFactor(0, 1);
    rightSplitter_->setStretchFactor(1, 0);
    rightSplitter_->setCollapsible(1, false);
    rightSplitter_->setSizes({350, 150});

    splitter_->addWidget(rightSplitter_);
    // The file list is what the user works in, so it gets the width by default
    splitter_->setSizes({560, 440});

    layout->addWidget(splitter_);

    actionController_->setActions(playAction_, runAction_, mountAction_);
    favoritesController_->setToggleAction(toggleFavoriteAction_);
    favoritesController_->setFavoritesMenu(favoritesMenu_);
}

void ExplorePanel::setupConnections()
{
    if (deviceConnection_) {
        connect(deviceConnection_, &DeviceConnectionManager::stateChanged, this,
                &ExplorePanel::onConnectionStateChanged);
    }

    if (drive8Status_) {
        connect(drive8Status_, &DriveStatusWidget::ejectClicked, this,
                &ExplorePanel::ejectDriveARequested);
    }
    if (drive9Status_) {
        connect(drive9Status_, &DriveStatusWidget::ejectClicked, this,
                &ExplorePanel::ejectDriveBRequested);
    }

    connect(favoritesController_, &ExploreFavoritesController::navigateToPath, this,
            &ExplorePanel::setCurrentDirectory);
    connect(favoritesController_, &ExploreFavoritesController::statusMessage, this,
            &ExplorePanel::statusMessage);

    connect(navController_, &ExploreNavigationController::statusMessage, this,
            &ExplorePanel::statusMessage);
    connect(navController_, &ExploreNavigationController::directoryChanged, previewCoordinator_,
            &PreviewCoordinator::onDirectoryChanged);

    connect(contextMenu_, &ExploreContextMenuController::playRequested, actionController_,
            &FileActionController::playSelection);
    connect(contextMenu_, &ExploreContextMenuController::runRequested, actionController_,
            &FileActionController::runSelection);
    connect(contextMenu_, &ExploreContextMenuController::mountARequested, this,
            [this]() { actionController_->mountToDriveSelection("a"); });
    connect(contextMenu_, &ExploreContextMenuController::mountBRequested, this,
            [this]() { actionController_->mountToDriveSelection("b"); });
    connect(contextMenu_, &ExploreContextMenuController::downloadRequested, actionController_,
            &FileActionController::downloadSelection);
    connect(contextMenu_, &ExploreContextMenuController::loadConfigRequested, actionController_,
            &FileActionController::loadConfigSelection);
    connect(contextMenu_, &ExploreContextMenuController::toggleFavoriteRequested, this, [this]() {
        QString path =
            selectedPath().isEmpty() ? navController_->currentDirectory() : selectedPath();
        favoritesController_->onToggleFavorite(path);
    });
    connect(contextMenu_, &ExploreContextMenuController::addToPlaylistRequested, actionController_,
            &FileActionController::addToPlaylistSelection);
    connect(contextMenu_, &ExploreContextMenuController::refreshRequested, this,
            [this]() { refresh(); });
    connect(actionController_, &FileActionController::statusMessage, this,
            &ExplorePanel::statusMessage);

    connect(fileDetailsPanel_, &FileDetailsPanel::contentRequested, previewCoordinator_,
            &PreviewCoordinator::onFileContentRequested);
    connect(previewCoordinator_, &PreviewCoordinator::statusMessage, this,
            &ExplorePanel::statusMessage);
}

// ============================================================================
// Public API (delegated to navController_)
// ============================================================================

void ExplorePanel::setCurrentDirectory(const QString &path)
{
    navController_->setCurrentDirectory(path);
}

QString ExplorePanel::currentDirectory() const
{
    return navController_->currentDirectory();
}

void ExplorePanel::refresh()
{
    navController_->refresh();
}

void ExplorePanel::refreshIfStale()
{
    navController_->refreshIfStale();
}

void ExplorePanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refreshIfStale();
}

void ExplorePanel::updateDriveInfo()
{
    bool connected = deviceConnection_ && deviceConnection_->canPerformOperations();
    QList<DriveInfo> drives = connected ? deviceConnection_->driveInfo() : QList<DriveInfo>();
    auto state = explorepanel::calculateDriveDisplay(connected, drives);

    if (drive8Status_) {
        drive8Status_->setImageName(state.drive8Image);
        drive8Status_->setMounted(state.drive8Mounted);
    }
    if (drive9Status_) {
        drive9Status_->setImageName(state.drive9Image);
        drive9Status_->setMounted(state.drive9Mounted);
    }
}

void ExplorePanel::loadSettings()
{
    QSettings settings;
    QString savedDir = settings.value("directories/exploreRemote", "/").toString();
    navController_->setCurrentDirectory(savedDir);

    const QByteArray splitterState = settings.value("layout/exploreSplitter").toByteArray();
    if (!splitterState.isEmpty()) {
        splitter_->restoreState(splitterState);
    }
    const QByteArray rightState = settings.value("layout/exploreRightSplitter").toByteArray();
    if (!rightState.isEmpty()) {
        rightSplitter_->restoreState(rightState);
    }
    playlistWidget_->setCollapsed(settings.value("layout/playlistCollapsed", false).toBool());
    const QByteArray headerState = settings.value("layout/exploreHeader").toByteArray();
    if (!headerState.isEmpty()) {
        treeView_->header()->restoreState(headerState);
    }
}

void ExplorePanel::saveSettings()
{
    QSettings settings;
    settings.setValue("directories/exploreRemote", navController_->currentDirectory());
    settings.setValue("layout/exploreSplitter", splitter_->saveState());
    // A collapsed playlist is a header row high: keep the last open layout for when it reopens
    if (!playlistWidget_->isCollapsed()) {
        settings.setValue("layout/exploreRightSplitter", rightSplitter_->saveState());
    }
    settings.setValue("layout/playlistCollapsed", playlistWidget_->isCollapsed());
    settings.setValue("layout/exploreHeader", treeView_->header()->saveState());
}

void ExplorePanel::onPlaylistCollapsedChanged(bool collapsed)
{
    const QList<int> sizes = rightSplitter_->sizes();
    if (sizes.size() != 2) {
        return;
    }
    const int total = sizes[0] + sizes[1];
    if (collapsed) {
        expandedPlaylistHeight_ = sizes[1];
        const int header = playlistWidget_->maximumHeight();
        rightSplitter_->setSizes({total - header, header});
    } else {
        const int playlist = std::clamp(expandedPlaylistHeight_, 0, total);
        rightSplitter_->setSizes({total - playlist, playlist});
    }
}

void ExplorePanel::setMetadataServices(const MetadataServiceBundle &bundle)
{
    if (fileDetailsPanel_) {
        fileDetailsPanel_->setSonglengthsDatabase(bundle.songlengthsDatabase);
        fileDetailsPanel_->setHVSCMetadataService(bundle.hvscMetadataService);
        fileDetailsPanel_->setGameBase64Service(bundle.gameBase64Service);
    }
}

void ExplorePanel::setStreamingService(StreamingService *manager)
{
    actionController_->setStreamingService(manager);
}

// ============================================================================
// Event handlers
// ============================================================================

void ExplorePanel::onConnectionStateChanged()
{
    bool canOperate = deviceConnection_ && deviceConnection_->canPerformOperations();

    actionController_->updateActionStates(filetype::FileType::Unknown, false);

    auto enablement = explorepanel::calculateActionEnablement(
        canOperate, false, filetype::FileType::Unknown, navController_->currentDirectory());
    if (refreshAction_) {
        refreshAction_->setEnabled(enablement.canRefresh);
    }
    if (navWidget_) {
        navWidget_->setUpEnabled(enablement.canGoUp);
    }

    if (!canOperate && fileDetailsPanel_) {
        fileDetailsPanel_->clear();
    }
}

QString ExplorePanel::selectedPath() const
{
    if (!treeView_ || !remoteFileModel_) {
        qCDebug(LogUi) << "ExplorePanel::selectedPath: treeView or remoteFileModel is null";
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
        qCDebug(LogUi) << "ExplorePanel::isSelectedDirectory: treeView or remoteFileModel is null";
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

    filetype::FileType fileType = (hasSelection && treeView_ && remoteFileModel_)
                                      ? remoteFileModel_->fileType(treeView_->currentIndex())
                                      : filetype::FileType::Unknown;

    actionController_->updateActionStates(fileType, canOperate && hasSelection);

    QString pathToCheck = hasSelection ? selected : navController_->currentDirectory();
    favoritesController_->updateForPath(pathToCheck);

    QModelIndex index = treeView_ ? treeView_->currentIndex() : QModelIndex();
    previewCoordinator_->onSelectionChanged(index);
}

void ExplorePanel::onDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid() || !remoteFileModel_) {
        qCDebug(LogUi) << "onDoubleClicked: invalid index or null remoteFileModel";
        return;
    }

    filetype::FileType type = remoteFileModel_->fileType(index);
    bool isDirectory = remoteFileModel_->isDirectory(index);
    auto action = filebrowser::resolveDoubleClickAction(type, isDirectory);

    switch (action) {
    case filebrowser::DoubleClickAction::Navigate:
        navController_->setCurrentDirectory(remoteFileModel_->filePath(index));
        break;
    case filebrowser::DoubleClickAction::Play:
        actionController_->play(selectedPath(), type);
        break;
    case filebrowser::DoubleClickAction::Run:
        actionController_->run(selectedPath(), type);
        break;
    case filebrowser::DoubleClickAction::Mount:
        actionController_->mountToDrive(selectedPath(), "a");
        break;
    case filebrowser::DoubleClickAction::LoadConfig:
        actionController_->loadConfig(selectedPath(), type);
        break;
    case filebrowser::DoubleClickAction::None:
        break;
    }
}

void ExplorePanel::onContextMenu(const QPoint &pos)
{
    if (!treeView_ || !remoteFileModel_) {
        qCDebug(LogUi) << "onContextMenu: treeView or remoteFileModel is null";
        return;
    }

    QModelIndex index = treeView_->indexAt(pos);
    if (!index.isValid()) {
        qCDebug(LogUi) << "onContextMenu: no item at position" << pos;
        return;
    }

    filetype::FileType fileType = remoteFileModel_->fileType(index);
    bool canOperate = deviceConnection_ && deviceConnection_->canPerformOperations();

    bool canAddToPlaylist = false;
    const QModelIndexList selectedIndices = treeView_->selectionModel()->selectedRows();
    for (const QModelIndex &selIndex : selectedIndices) {
        if (remoteFileModel_->fileType(selIndex) == filetype::FileType::SidMusic) {
            canAddToPlaylist = true;
            break;
        }
    }

    bool isFav = favoritesController_->isFavorite(remoteFileModel_->filePath(index));
    auto enablement = explorepanel::calculateActionEnablement(canOperate, true, fileType,
                                                              navController_->currentDirectory());
    contextMenu_->showForSelection(treeView_->viewport()->mapToGlobal(pos), enablement,
                                   canAddToPlaylist, isFav);
}

void ExplorePanel::onParentFolder()
{
    navController_->navigateToParent();
}
