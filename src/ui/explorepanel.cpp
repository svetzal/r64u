#include "explorepanel.h"

#include "drivestatuswidget.h"
#include "explorecontextmenu.h"
#include "explorefavoritescontroller.h"
#include "explorenavigationcontroller.h"
#include "explorepanelcore.h"
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
#include "services/metadataservicebundle.h"
#include "services/playlistmanager.h"

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
      playlistManager_(playlistManager)
{
    Q_ASSERT(deviceConnection_ && "DeviceConnection is required");
    Q_ASSERT(remoteFileModel_ && "RemoteFileModel is required");
    Q_ASSERT(configLoader && "ConfigFileLoader is required");
    Q_ASSERT(previewService && "FilePreviewService is required");
    Q_ASSERT(favoritesManager && "FavoritesManager is required");
    Q_ASSERT(playlistManager_ && "PlaylistManager is required");

    actionController_ = new FileActionController(deviceConnection_, configLoader, this);
    actionController_->setPlaylistManager(playlistManager_);
    favoritesController_ = new ExploreFavoritesController(favoritesManager, this);
    contextMenu_ = new ExploreContextMenu(this);

    setupUi();

    // navController_ is constructed after setupUi() so treeView_ and navWidget_ exist
    navController_ = new ExploreNavigationController(deviceConnection_, remoteFileModel_, treeView_,
                                                     navWidget_, favoritesController_, this);

    previewCoordinator_ =
        new PreviewCoordinator(previewService, fileDetailsPanel_, playlistManager_, this);

    setupConnections();

    connect(configLoader, &ConfigFileLoader::loadFinished, previewCoordinator_,
            &PreviewCoordinator::onConfigLoadFinished);
    connect(configLoader, &ConfigFileLoader::loadFailed, previewCoordinator_,
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

    playAction_ = toolBar_->addAction(tr("Play"));
    playAction_->setToolTip(tr("Play selected SID/MOD file"));
    connect(playAction_, &QAction::triggered, this, [this]() {
        if (!treeView_ || !remoteFileModel_)
            return;
        actionController_->play(selectedPath(),
                                remoteFileModel_->fileType(treeView_->currentIndex()));
    });

    runAction_ = toolBar_->addAction(tr("Run"));
    runAction_->setToolTip(tr("Run selected PRG/CRT file"));
    connect(runAction_, &QAction::triggered, this, [this]() {
        if (!treeView_ || !remoteFileModel_)
            return;
        actionController_->run(selectedPath(),
                               remoteFileModel_->fileType(treeView_->currentIndex()));
    });

    mountAction_ = toolBar_->addAction(tr("Mount"));
    mountAction_->setToolTip(tr("Mount selected disk image"));
    connect(mountAction_, &QAction::triggered, this,
            [this]() { actionController_->mountToDrive(selectedPath(), "a"); });

    toolBar_->addSeparator();

    refreshAction_ = toolBar_->addAction(tr("Refresh"));
    refreshAction_->setToolTip(tr("Refresh file listing"));
    connect(refreshAction_, &QAction::triggered, this, [this]() { refresh(); });

    toolBar_->addSeparator();

    toggleFavoriteAction_ = toolBar_->addAction(QString::fromUtf8("☆"));
    toggleFavoriteAction_->setToolTip(tr("Add/remove current path from favorites"));
    toggleFavoriteAction_->setCheckable(true);
    toggleFavoriteAction_->setEnabled(true);
    connect(toggleFavoriteAction_, &QAction::triggered, this, [this]() {
        QString path =
            selectedPath().isEmpty() ? navController_->currentDirectory() : selectedPath();
        favoritesController_->onToggleFavorite(path);
    });

    favoritesMenu_ = new QMenu(tr("Favorites"), this);
    auto *favoritesMenuAction = toolBar_->addAction(tr("Favorites"));
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

    if (auto *selModel = treeView_->selectionModel()) {
        connect(selModel, &QItemSelectionModel::selectionChanged, this,
                &ExplorePanel::onSelectionChanged);
    }
    connect(treeView_, &QTreeView::doubleClicked, this, &ExplorePanel::onDoubleClicked);
    connect(treeView_, &QTreeView::customContextMenuRequested, this, &ExplorePanel::onContextMenu);

    remoteLayout->addWidget(treeView_);

    drive8Status_ = new DriveStatusWidget(tr("Drive 8:"));
    remoteLayout->addWidget(drive8Status_);

    drive9Status_ = new DriveStatusWidget(tr("Drive 9:"));
    remoteLayout->addWidget(drive9Status_);

    splitter_->addWidget(remoteWidget);

    // Right side: vertical splitter for details and playlist
    rightSplitter_ = new QSplitter(Qt::Vertical);

    fileDetailsPanel_ = new FileDetailsPanel();
    rightSplitter_->addWidget(fileDetailsPanel_);

    playlistWidget_ = new PlaylistWidget(playlistManager_);
    connect(playlistWidget_, &PlaylistWidget::statusMessage, this, &ExplorePanel::statusMessage);
    rightSplitter_->addWidget(playlistWidget_);

    rightSplitter_->setSizes({350, 150});

    splitter_->addWidget(rightSplitter_);
    splitter_->setSizes({400, 600});

    layout->addWidget(splitter_);

    actionController_->setActions(playAction_, runAction_, mountAction_);
    favoritesController_->setToggleAction(toggleFavoriteAction_);
    favoritesController_->setFavoritesMenu(favoritesMenu_);
}

void ExplorePanel::setupConnections()
{
    if (deviceConnection_) {
        connect(deviceConnection_, &DeviceConnection::stateChanged, this,
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

    connect(actionController_, &FileActionController::statusMessage, this,
            &ExplorePanel::statusMessage);

    connect(favoritesController_, &ExploreFavoritesController::navigateToPath, this,
            &ExplorePanel::setCurrentDirectory);
    connect(favoritesController_, &ExploreFavoritesController::statusMessage, this,
            &ExplorePanel::statusMessage);

    connect(navController_, &ExploreNavigationController::statusMessage, this,
            &ExplorePanel::statusMessage);

    connect(contextMenu_, &ExploreContextMenu::playRequested, this, [this]() {
        if (!treeView_ || !remoteFileModel_)
            return;
        actionController_->play(selectedPath(),
                                remoteFileModel_->fileType(treeView_->currentIndex()));
    });
    connect(contextMenu_, &ExploreContextMenu::runRequested, this, [this]() {
        if (!treeView_ || !remoteFileModel_)
            return;
        actionController_->run(selectedPath(),
                               remoteFileModel_->fileType(treeView_->currentIndex()));
    });
    connect(contextMenu_, &ExploreContextMenu::mountARequested, this,
            [this]() { actionController_->mountToDrive(selectedPath(), "a"); });
    connect(contextMenu_, &ExploreContextMenu::mountBRequested, this,
            [this]() { actionController_->mountToDrive(selectedPath(), "b"); });
    connect(contextMenu_, &ExploreContextMenu::downloadRequested, this,
            [this]() { actionController_->download(selectedPath()); });
    connect(contextMenu_, &ExploreContextMenu::loadConfigRequested, this, [this]() {
        if (!treeView_ || !remoteFileModel_)
            return;
        actionController_->loadConfig(selectedPath(),
                                      remoteFileModel_->fileType(treeView_->currentIndex()));
    });
    connect(contextMenu_, &ExploreContextMenu::toggleFavoriteRequested, this, [this]() {
        QString path =
            selectedPath().isEmpty() ? navController_->currentDirectory() : selectedPath();
        favoritesController_->onToggleFavorite(path);
    });
    connect(contextMenu_, &ExploreContextMenu::addToPlaylistRequested, this, [this]() {
        if (!treeView_ || !remoteFileModel_)
            return;
        QModelIndexList selectedIndices = treeView_->selectionModel()->selectedRows();
        if (selectedIndices.isEmpty())
            return;
        QList<QPair<QString, filetype::FileType>> items;
        items.reserve(selectedIndices.size());
        for (const QModelIndex &idx : selectedIndices) {
            items.append({remoteFileModel_->filePath(idx), remoteFileModel_->fileType(idx)});
        }
        actionController_->addToPlaylist(items);
    });
    connect(contextMenu_, &ExploreContextMenu::refreshRequested, this, [this]() { refresh(); });

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
}

void ExplorePanel::saveSettings()
{
    QSettings settings;
    settings.setValue("directories/exploreRemote", navController_->currentDirectory());
}

void ExplorePanel::setMetadataServices(const MetadataServiceBundle &bundle)
{
    if (fileDetailsPanel_) {
        fileDetailsPanel_->setSonglengthsDatabase(bundle.songlengthsDatabase);
        fileDetailsPanel_->setHVSCMetadataService(bundle.hvscMetadataService);
        fileDetailsPanel_->setGameBase64Service(bundle.gameBase64Service);
    }
}

void ExplorePanel::setStreamingManager(StreamingManager *manager)
{
    actionController_->setStreamingManager(manager);
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

    QString pathToCheck = hasSelection ? selected : navController_->currentDirectory();
    favoritesController_->updateForPath(pathToCheck);

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
        return;
    }

    QModelIndex index = treeView_->indexAt(pos);
    if (!index.isValid()) {
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
