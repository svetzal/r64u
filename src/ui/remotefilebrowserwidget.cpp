#include "remotefilebrowserwidget.h"

#include "pathnavigationwidget.h"
#include "refreshpolicymanager.h"
#include "remotefilebrowsercontroller.h"

#include "models/remotefilemodel.h"
#include "services/errorhandler.h"
#include "services/remotefileoperationsservice.h"
#include "utils/logging.h"

#include <QAction>
#include <QFileInfo>
#include <QHeaderView>
#include <QMenu>
#include <QShowEvent>
#include <QToolBar>
#include <QTreeView>

RemoteFileBrowserWidget::RemoteFileBrowserWidget(RemoteFileModel *fileModel,
                                                 ErrorHandler *errorHandler, QWidget *parent)
    : FileBrowserWidget(errorHandler, parent), remoteFileModel_(fileModel),
      controller_(new RemoteFileBrowserController(this)),
      refreshPolicy_(new RefreshPolicyManager(this))
{
    Q_ASSERT(remoteFileModel_ && "RemoteFileModel is required");
    currentDirectory_ = QStringLiteral("/");
    refreshPolicy_->setModel(remoteFileModel_);
    refreshPolicy_->setConnected(false);

    // NOLINT: Qt template-method initialization pattern. These virtual methods are defined
    // in this concrete class and are intentionally called here to set up the widget.
    // Virtual dispatch is safe because RemoteFileBrowserWidget is not designed to be subclassed.
    RemoteFileBrowserWidget::setupUi();  // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
    RemoteFileBrowserWidget::
        setupContextMenu();  // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
    RemoteFileBrowserWidget::
        setupConnections();  // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
}

void RemoteFileBrowserWidget::setupUi()
{
    // Call base class to create standard UI structure
    FileBrowserWidget::setupUi();

    // Set blue style for nav widget
    navWidget_->setStyleBlue();

    // Add remote-specific actions to toolbar
    downloadAction_ = toolBar_->addAction(tr("Download"));
    downloadAction_->setToolTip(tr("Download selected files from C64U"));
    connect(downloadAction_, &QAction::triggered, this, &RemoteFileBrowserWidget::onDownload);

    newFolderAction_ = toolBar_->addAction(tr("New Folder"));
    newFolderAction_->setToolTip(tr("Create new folder on C64U"));
    connect(newFolderAction_, &QAction::triggered, this, &RemoteFileBrowserWidget::onNewFolder);

    renameAction_ = toolBar_->addAction(tr("Rename"));
    renameAction_->setToolTip(tr("Rename selected file or folder on C64U"));
    connect(renameAction_, &QAction::triggered, this, &RemoteFileBrowserWidget::onRename);

    deleteAction_ = toolBar_->addAction(tr("Delete"));
    deleteAction_->setToolTip(tr("Delete selected file or folder on C64U"));
    connect(deleteAction_, &QAction::triggered, this, &RemoteFileBrowserWidget::onDelete);

    toolBar_->addSeparator();

    refreshAction_ = toolBar_->addAction(tr("Refresh"));
    refreshAction_->setToolTip(tr("Refresh file listing"));
    connect(refreshAction_, &QAction::triggered, this, &RemoteFileBrowserWidget::onRefresh);

    // Set up the model
    treeView_->setModel(remoteFileModel_);

    // Set column widths: Size and Type get fixed widths, Name stretches
    treeView_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    treeView_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    treeView_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    RemoteFileBrowserWidget::updateActions();  // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
}

void RemoteFileBrowserWidget::setupContextMenu()
{
    // Call base class to create menu with "Set as Destination" action
    FileBrowserWidget::setupContextMenu();

    // Connect the set destination action with remote-specific logic
    connect(setDestAction_, &QAction::triggered, this, [this]() {
        if (!treeView_ || !remoteFileModel_) {
            qCDebug(LogUi) << "setupContextMenu: treeView or remoteFileModel is null";
            return;
        }
        QModelIndex index = treeView_->currentIndex();
        if (index.isValid() && remoteFileModel_->isDirectory(index)) {
            QString path = remoteFileModel_->filePath(index);
            setCurrentDirectory(path);
        }
    });

    // Add remote-specific menu items
    contextMenu_->addSeparator();
    contextMenu_->addAction(tr("Download to Local Directory"), this,
                            &RemoteFileBrowserWidget::onDownload);
    contextMenu_->addAction(tr("Delete"), this, &RemoteFileBrowserWidget::onDelete);
    contextMenu_->addSeparator();
    contextMenu_->addAction(tr("New Folder"), this, &RemoteFileBrowserWidget::onNewFolder);
    contextMenu_->addAction(tr("Refresh"), this, &RemoteFileBrowserWidget::onRefresh);
}

void RemoteFileBrowserWidget::setupConnections()
{
    // Call base class to set up standard connections (doubleClicked, contextMenu, selectionChanged)
    FileBrowserWidget::setupConnections();
}

void RemoteFileBrowserWidget::setFileOperations(RemoteFileOperationsService *ops)
{
    fileOperations_ = ops;
    if (fileOperations_) {
        connect(this, &RemoteFileBrowserWidget::createFolderRequested, fileOperations_,
                &RemoteFileOperationsService::createFolder);
        connect(this, &RemoteFileBrowserWidget::renameRequested, fileOperations_,
                &RemoteFileOperationsService::renameItem);
        connect(fileOperations_, &RemoteFileOperationsService::folderCreated, this,
                &RemoteFileBrowserWidget::onDirectoryCreated);
        connect(fileOperations_, &RemoteFileOperationsService::itemRenamed, this,
                &RemoteFileBrowserWidget::onFileRenamed);
        connect(fileOperations_, &RemoteFileOperationsService::itemRemoved, this,
                &RemoteFileBrowserWidget::onFileRemoved);
        connect(fileOperations_, &RemoteFileOperationsService::statusMessage, this,
                [this](const QString &message, int /*timeout*/) {
                    errorHandler_->info(ErrorCategory::FileOperation, message);
                });
    }
}

void RemoteFileBrowserWidget::updateActions()
{
    bool hasSelection = !RemoteFileBrowserWidget::selectedPath()
                             .isEmpty();  // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)

    updateCommonActions(connected_);
    downloadAction_->setEnabled(connected_ && hasSelection);
    refreshAction_->setEnabled(connected_);
}

void RemoteFileBrowserWidget::setCurrentDirectory(const QString &path)
{
    currentDirectory_ = path;

    // Update tree view to show this folder as root
    if (remoteFileModel_) {
        remoteFileModel_->setRootPath(path);
    }

    if (navWidget_) {
        navWidget_->setPath(path);
    }
    emit currentDirectoryChanged(path);
    errorHandler_->info(ErrorCategory::FileOperation, tr("Upload destination: %1").arg(path));

    // Enable/disable up button based on whether we can go up
    bool canGoUp = (path != "/" && !path.isEmpty());
    if (navWidget_) {
        navWidget_->setUpEnabled(canGoUp && connected_);
    }
}

void RemoteFileBrowserWidget::setDownloadEnabled(bool enabled)
{
    bool hasSelection = !selectedPath().isEmpty();
    downloadAction_->setEnabled(enabled && hasSelection && connected_);
}

void RemoteFileBrowserWidget::onConnectionStateChanged(bool connected)
{
    connected_ = connected;
    refreshPolicy_->setConnected(connected);
    updateActions();

    bool canGoUp = (currentDirectory_ != "/" && !currentDirectory_.isEmpty());
    if (navWidget_) {
        navWidget_->setUpEnabled(canGoUp && connected);
    }
}

QString RemoteFileBrowserWidget::selectedPath() const
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

bool RemoteFileBrowserWidget::isSelectedDirectory() const
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

QAbstractItemModel *RemoteFileBrowserWidget::model() const
{
    return remoteFileModel_;
}

QString RemoteFileBrowserWidget::filePath(const QModelIndex &index) const
{
    if (!remoteFileModel_ || !index.isValid()) {
        return {};
    }
    return remoteFileModel_->filePath(index);
}

bool RemoteFileBrowserWidget::isDirectory(const QModelIndex &index) const
{
    if (!remoteFileModel_ || !index.isValid()) {
        return false;
    }
    return remoteFileModel_->isDirectory(index);
}

void RemoteFileBrowserWidget::navigateToDirectory(const QString &path)
{
    setCurrentDirectory(path);
}

void RemoteFileBrowserWidget::refresh()
{
    onRefresh();
}

void RemoteFileBrowserWidget::refreshIfStale()
{
    refreshPolicy_->refreshIfStale();
}

void RemoteFileBrowserWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    // Auto-refresh stale data when panel becomes visible
    refreshIfStale();
}

void RemoteFileBrowserWidget::onParentFolder()
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

void RemoteFileBrowserWidget::onContextMenu(const QPoint &pos)
{
    if (!treeView_ || !remoteFileModel_) {
        qCDebug(LogUi) << "onContextMenu: treeView or remoteFileModel is null";
        return;
    }

    QModelIndex index = treeView_->indexAt(pos);
    if (index.isValid()) {
        bool isDir = remoteFileModel_->isDirectory(index);
        if (setDestAction_) {
            setDestAction_->setEnabled(isDir);
        }
        if (contextMenu_) {
            contextMenu_->exec(treeView_->viewport()->mapToGlobal(pos));
        }
    }
}

bool RemoteFileBrowserWidget::requireConnected(const QString &cannotMessage)
{
    if (!connected_) {
        errorHandler_->handleError(ErrorCategory::Connection, ErrorSeverity::Warning, cannotMessage,
                                   tr("Not connected to device"));
        return false;
    }
    return true;
}

void RemoteFileBrowserWidget::onDownload()
{
    auto entries = selectedEntries();
    if (entries.isEmpty()) {
        errorHandler_->handleError(ErrorCategory::Validation, ErrorSeverity::Warning,
                                   tr("No remote file selected"));
        return;
    }

    for (const auto &e : entries) {
        emit downloadRequested(e.path, e.isDirectory);
    }
}

bool RemoteFileBrowserWidget::canModify(const QString &actionLabel)
{
    return requireConnected(tr("Cannot %1").arg(actionLabel));
}

void RemoteFileBrowserWidget::performNewFolder(const QString &folderName)
{
    QString newPath = controller_->buildNewFolderPath(currentDirectory_, folderName);
    if (newPath.isEmpty()) {
        return;
    }

    emit createFolderRequested(newPath);
    QString remoteDir = currentDirectory_.isEmpty() ? QStringLiteral("/") : currentDirectory_;
    if (!remoteDir.endsWith('/')) {
        remoteDir += '/';
    }
    errorHandler_->info(ErrorCategory::FileOperation,
                        tr("Creating folder %1 in %2...").arg(folderName).arg(remoteDir));
}

void RemoteFileBrowserWidget::performRename(const QString &path, const QString &newName)
{
    QString newPath = controller_->buildRenamePath(path, newName);
    if (newPath.isEmpty()) {
        return;
    }

    QString oldName = QFileInfo(path).fileName();
    emit renameRequested(path, newPath);
    errorHandler_->info(ErrorCategory::FileOperation, tr("Renaming %1...").arg(oldName));
}

void RemoteFileBrowserWidget::performDelete(const QList<SelectedEntry> &entries)
{
    QStringList paths;
    for (const auto &e : entries) {
        paths.append(e.path);
        emit deleteRequested(e.path, e.isDirectory);
    }

    if (paths.size() == 1) {
        errorHandler_->info(ErrorCategory::FileOperation,
                            tr("Deleting %1...").arg(QFileInfo(paths.first()).fileName()));
    } else {
        errorHandler_->info(ErrorCategory::FileOperation,
                            tr("Deleting %1 items...").arg(paths.size()));
    }
}

void RemoteFileBrowserWidget::onRefresh()
{
    if (!connected_ || !treeView_ || !remoteFileModel_) {
        return;
    }

    QModelIndex index = treeView_->currentIndex();
    if (index.isValid() && remoteFileModel_->isDirectory(index)) {
        remoteFileModel_->refresh(index);
    } else {
        remoteFileModel_->refresh();
    }
}

void RemoteFileBrowserWidget::setSuppressAutoRefresh(bool suppress)
{
    refreshPolicy_->setSuppressed(suppress);
}

RemoteFileBrowserWidget::AutoRefreshSuppressor::AutoRefreshSuppressor(
    RemoteFileBrowserWidget *widget)
    : widget_(widget)
{
    widget_->setSuppressAutoRefresh(true);
}

RemoteFileBrowserWidget::AutoRefreshSuppressor::~AutoRefreshSuppressor()
{
    widget_->setSuppressAutoRefresh(false);
}

RemoteFileBrowserWidget::AutoRefreshSuppressor RemoteFileBrowserWidget::suppressRefresh()
{
    return AutoRefreshSuppressor(this);
}

void RemoteFileBrowserWidget::onDirectoryCreated(const QString &path)
{
    errorHandler_->info(ErrorCategory::FileOperation,
                        tr("Folder created: %1").arg(QFileInfo(path).fileName()));
    if (!refreshPolicy_->isSuppressed() && remoteFileModel_) {
        remoteFileModel_->refresh();
    }
}

void RemoteFileBrowserWidget::onFileRemoved(const QString &path)
{
    // Don't emit status messages or refresh during bulk delete operations
    if (!refreshPolicy_->isSuppressed() && remoteFileModel_) {
        errorHandler_->info(ErrorCategory::FileOperation,
                            tr("Deleted: %1").arg(QFileInfo(path).fileName()));
        remoteFileModel_->refresh();
    }
}

void RemoteFileBrowserWidget::onFileRenamed(const QString &oldPath, const QString &newPath)
{
    QString oldName = QFileInfo(oldPath).fileName();
    QString newName = QFileInfo(newPath).fileName();
    errorHandler_->info(ErrorCategory::FileOperation,
                        tr("Renamed: %1 -> %2").arg(oldName).arg(newName));
    if (!refreshPolicy_->isSuppressed() && remoteFileModel_) {
        remoteFileModel_->refresh();
    }
}
