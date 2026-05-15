#include "remotefilebrowserwidget.h"

#include "pathnavigationwidget.h"
#include "refreshpolicy.h"
#include "remotefilebrowsercontroller.h"

#include "models/remotefilemodel.h"
#include "services/errorhandler.h"
#include "services/remotefileoperationsservice.h"
#include "utils/logging.h"

#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QShowEvent>
#include <QVBoxLayout>

RemoteFileBrowserWidget::RemoteFileBrowserWidget(RemoteFileModel *model, QWidget *parent)
    : QWidget(parent), remoteFileModel_(model), currentDirectory_("/"),
      controller_(new RemoteFileBrowserController(this)), refreshPolicy_(new RefreshPolicy(this))
{
    Q_ASSERT(remoteFileModel_ && "RemoteFileModel is required");
    refreshPolicy_->setModel(remoteFileModel_);
    refreshPolicy_->setConnected(false);

    setupUi();
    setupContextMenu();
    setupConnections();
}

void RemoteFileBrowserWidget::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *label = new QLabel(tr("C64U Files"));
    label->setStyleSheet("font-weight: bold;");
    layout->addWidget(label);

    // Path navigation widget
    navWidget_ = new PathNavigationWidget(tr("Upload to:"));
    connect(navWidget_, &PathNavigationWidget::upClicked, this,
            &RemoteFileBrowserWidget::onParentFolder);
    layout->addWidget(navWidget_);

    // Toolbar
    toolBar_ = new QToolBar();
    toolBar_->setIconSize(QSize(16, 16));
    toolBar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

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

    layout->addWidget(toolBar_);

    // Tree view
    treeView_ = new QTreeView();
    if (remoteFileModel_) {
        treeView_->setModel(remoteFileModel_);
    }
    treeView_->setAlternatingRowColors(true);
    treeView_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    treeView_->setContextMenuPolicy(Qt::CustomContextMenu);

    // Set column widths: Size and Type get fixed widths, Name stretches
    treeView_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    treeView_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    treeView_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    connect(treeView_, &QTreeView::customContextMenuRequested, this,
            &RemoteFileBrowserWidget::onContextMenu);
    connect(treeView_, &QTreeView::doubleClicked, this, &RemoteFileBrowserWidget::onDoubleClicked);
    // Guard against null selection model
    if (auto *selModel = treeView_->selectionModel()) {
        connect(selModel, &QItemSelectionModel::selectionChanged, this, [this]() {
            updateActions();
            emit selectionChanged();
        });
    }

    layout->addWidget(treeView_);

    // Initialize nav widget
    navWidget_->setPath(currentDirectory_);
    updateActions();
}

void RemoteFileBrowserWidget::setupContextMenu()
{
    contextMenu_ = new QMenu(this);
    setDestAction_ = contextMenu_->addAction(tr("Set as Upload Destination"), this, [this]() {
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
    contextMenu_->addSeparator();
    contextMenu_->addAction(tr("Download to Local Directory"), this,
                            &RemoteFileBrowserWidget::onDownload);
    contextMenu_->addAction(tr("Delete"), this, &RemoteFileBrowserWidget::onDelete);
    contextMenu_->addSeparator();
    contextMenu_->addAction(tr("New Folder"), this, &RemoteFileBrowserWidget::onNewFolder);
    contextMenu_->addAction(tr("Refresh"), this, &RemoteFileBrowserWidget::onRefresh);
}

void RemoteFileBrowserWidget::setupConnections() {}

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
                    if (errorHandler_) {
                        errorHandler_->info(ErrorCategory::FileOperation, message);
                    }
                });
    }
}

void RemoteFileBrowserWidget::setErrorHandler(ErrorHandler *handler)
{
    errorHandler_ = handler;
}

void RemoteFileBrowserWidget::updateActions()
{
    bool hasSelection = !selectedPath().isEmpty();

    downloadAction_->setEnabled(connected_ && hasSelection);
    newFolderAction_->setEnabled(connected_);
    renameAction_->setEnabled(connected_ && hasSelection);
    deleteAction_->setEnabled(connected_ && hasSelection);
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
    if (errorHandler_) {
        errorHandler_->info(ErrorCategory::FileOperation, tr("Upload destination: %1").arg(path));
    }

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

QStringList RemoteFileBrowserWidget::selectedPaths() const
{
    QStringList paths;
    if (!treeView_ || !treeView_->selectionModel() || !remoteFileModel_) {
        return paths;
    }

    // Get all selected indexes, filter to column 0 only (avoid duplicates from multi-column
    // selection)
    QModelIndexList selectedIndexes = treeView_->selectionModel()->selectedIndexes();
    QSet<QString> seenPaths;  // Deduplicate

    for (const QModelIndex &index : selectedIndexes) {
        if (index.column() != 0) {
            continue;  // Only process first column to avoid duplicates
        }
        QString path = remoteFileModel_->filePath(index);
        if (!path.isEmpty() && !seenPaths.contains(path)) {
            seenPaths.insert(path);
            paths.append(path);
        }
    }

    return paths;
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

void RemoteFileBrowserWidget::onDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid() || !remoteFileModel_) {
        qCDebug(LogUi) << "onDoubleClicked: invalid index or null remoteFileModel";
        return;
    }

    if (remoteFileModel_->isDirectory(index)) {
        QString path = remoteFileModel_->filePath(index);
        setCurrentDirectory(path);
    }
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

void RemoteFileBrowserWidget::onDownload()
{
    QStringList paths = selectedPaths();
    if (paths.isEmpty()) {
        if (errorHandler_) {
            errorHandler_->handleError(ErrorCategory::Validation, ErrorSeverity::Warning,
                                       tr("No remote file selected"));
        }
        return;
    }

    // Emit download request for each selected file
    for (const QString &remotePath : paths) {
        // Find the index for this path to check if it's a directory
        bool isDir = false;
        QModelIndexList matches = treeView_->selectionModel()->selectedIndexes();
        for (const QModelIndex &idx : matches) {
            if (idx.column() == 0 && remoteFileModel_->filePath(idx) == remotePath) {
                isDir = remoteFileModel_->isDirectory(idx);
                break;
            }
        }
        emit downloadRequested(remotePath, isDir);
    }
}

void RemoteFileBrowserWidget::onNewFolder()
{
    if (!connected_) {
        qCWarning(LogUi) << "onNewFolder skipped: not connected";
        return;
    }

    bool ok;
    QString folderName = QInputDialog::getText(this, tr("New Remote Folder"), tr("Folder name:"),
                                               QLineEdit::Normal, "", &ok);

    if (!ok || folderName.isEmpty()) {
        return;
    }

    QString newPath = controller_->buildNewFolderPath(currentDirectory_, folderName);
    if (newPath.isEmpty()) {
        return;
    }

    emit createFolderRequested(newPath);
    if (errorHandler_) {
        QString remoteDir = currentDirectory_.isEmpty() ? QStringLiteral("/") : currentDirectory_;
        if (!remoteDir.endsWith('/')) {
            remoteDir += '/';
        }
        errorHandler_->info(ErrorCategory::FileOperation,
                            tr("Creating folder %1 in %2...").arg(folderName).arg(remoteDir));
    }
}

void RemoteFileBrowserWidget::onRename()
{
    if (!connected_) {
        qCWarning(LogUi) << "onRename skipped: not connected";
        return;
    }

    QString remotePath = selectedPath();
    if (remotePath.isEmpty()) {
        return;
    }

    QString oldName = QFileInfo(remotePath).fileName();
    QString itemType = isSelectedDirectory() ? tr("folder") : tr("file");

    bool ok;
    QString newName = QInputDialog::getText(this, tr("Rename Remote %1").arg(itemType),
                                            tr("New name:"), QLineEdit::Normal, oldName, &ok);

    if (!ok || newName.isEmpty() || newName == oldName) {
        return;
    }

    if (!controller_->isValidName(newName)) {
        QMessageBox::warning(this, tr("Invalid Name"),
                             tr("The name cannot contain '/' or '\\' characters."));
        return;
    }

    QString newPath = controller_->buildRenamePath(remotePath, newName);
    if (newPath.isEmpty()) {
        return;
    }

    emit renameRequested(remotePath, newPath);
    if (errorHandler_) {
        errorHandler_->info(ErrorCategory::FileOperation, tr("Renaming %1...").arg(oldName));
    }
}

void RemoteFileBrowserWidget::onDelete()
{
    if (!connected_) {
        return;
    }

    QStringList paths = selectedPaths();
    if (paths.isEmpty()) {
        return;
    }

    QString confirmMessage = controller_->buildDeleteConfirmMessage(paths, isSelectedDirectory());

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Delete"));
    msgBox.setText(confirmMessage);
    msgBox.setIcon(QMessageBox::Warning);
    QPushButton *deleteButton = msgBox.addButton(tr("Delete"), QMessageBox::DestructiveRole);
    msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
    msgBox.exec();

    if (msgBox.clickedButton() != deleteButton) {
        return;
    }

    // Delete each selected file
    for (const QString &remotePath : paths) {
        // Find the index to check if it's a directory
        bool isDir = false;
        QModelIndexList matches = treeView_->selectionModel()->selectedIndexes();
        for (const QModelIndex &idx : matches) {
            if (idx.column() == 0 && remoteFileModel_->filePath(idx) == remotePath) {
                isDir = remoteFileModel_->isDirectory(idx);
                break;
            }
        }
        emit deleteRequested(remotePath, isDir);
    }

    if (errorHandler_) {
        if (paths.size() == 1) {
            errorHandler_->info(ErrorCategory::FileOperation,
                                tr("Deleting %1...").arg(QFileInfo(paths.first()).fileName()));
        } else {
            errorHandler_->info(ErrorCategory::FileOperation,
                                tr("Deleting %1 items...").arg(paths.size()));
        }
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
    if (errorHandler_) {
        errorHandler_->info(ErrorCategory::FileOperation,
                            tr("Folder created: %1").arg(QFileInfo(path).fileName()));
    }
    if (!refreshPolicy_->isSuppressed() && remoteFileModel_) {
        remoteFileModel_->refresh();
    }
}

void RemoteFileBrowserWidget::onFileRemoved(const QString &path)
{
    // Don't emit status messages or refresh during bulk delete operations
    if (!refreshPolicy_->isSuppressed() && remoteFileModel_) {
        if (errorHandler_) {
            errorHandler_->info(ErrorCategory::FileOperation,
                                tr("Deleted: %1").arg(QFileInfo(path).fileName()));
        }
        remoteFileModel_->refresh();
    }
}

void RemoteFileBrowserWidget::onFileRenamed(const QString &oldPath, const QString &newPath)
{
    QString oldName = QFileInfo(oldPath).fileName();
    QString newName = QFileInfo(newPath).fileName();
    if (errorHandler_) {
        errorHandler_->info(ErrorCategory::FileOperation,
                            tr("Renamed: %1 -> %2").arg(oldName).arg(newName));
    }
    if (!refreshPolicy_->isSuppressed() && remoteFileModel_) {
        remoteFileModel_->refresh();
    }
}
