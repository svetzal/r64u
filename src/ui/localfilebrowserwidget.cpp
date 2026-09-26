#include "localfilebrowserwidget.h"

#include "pathnavigationwidget.h"
#include "pixelicons.h"

#include "models/localfileproxymodel.h"
#include "services/errorhandler.h"
#include "services/localfileoperationsservice.h"
#include "utils/logging.h"

#include <QDir>
#include <QHeaderView>
#include <QMenu>
#include <QStandardPaths>
#include <QToolBar>
#include <QTreeView>

LocalFileBrowserWidget::LocalFileBrowserWidget(ErrorHandler *errorHandler, QWidget *parent)
    : FileBrowserWidget(errorHandler, parent)
{
    currentDirectory_ = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    // NOLINT: Qt template-method initialization pattern. These virtual methods are defined
    // in this concrete class and are intentionally called here to set up the widget.
    // Virtual dispatch is safe because LocalFileBrowserWidget is not designed to be subclassed.
    LocalFileBrowserWidget::setupUi();  // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
    LocalFileBrowserWidget::
        setupContextMenu();  // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
    LocalFileBrowserWidget::
        setupConnections();  // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
}

void LocalFileBrowserWidget::setupUi()
{
    // Call base class to create standard UI structure
    FileBrowserWidget::setupUi();

    // Set green style for nav widget
    navWidget_->setStyleGreen();

    // Add local-specific actions to toolbar
    using pixeliconcore::Icon;
    uploadAction_ = toolBar_->addAction(pixelicons::icon(Icon::Upload), tr("Upload"));
    uploadAction_->setToolTip(tr("Upload selected files to C64U"));
    connect(uploadAction_, &QAction::triggered, this, &LocalFileBrowserWidget::onUpload);

    newFolderAction_ = toolBar_->addAction(pixelicons::icon(Icon::NewFolder), tr("New Folder"));
    newFolderAction_->setToolTip(tr("Create new folder in local directory"));
    connect(newFolderAction_, &QAction::triggered, this, &LocalFileBrowserWidget::onNewFolder);

    renameAction_ = toolBar_->addAction(pixelicons::icon(Icon::Rename), tr("Rename"));
    renameAction_->setToolTip(tr("Rename selected local file or folder"));
    connect(renameAction_, &QAction::triggered, this, &LocalFileBrowserWidget::onRename);

    deleteAction_ = toolBar_->addAction(pixelicons::icon(Icon::Delete), tr("Delete"));
    deleteAction_->setToolTip(tr("Move selected local file to trash"));
    connect(deleteAction_, &QAction::triggered, this, &LocalFileBrowserWidget::onDelete);

    // Set up the model
    fileModel_ = new QFileSystemModel(this);
    fileModel_->setRootPath(currentDirectory_);

    // Use proxy model for C64-specific file types and byte-sized display
    proxyModel_ = new LocalFileProxyModel(this);
    proxyModel_->setSourceModel(fileModel_);
    treeView_->setModel(proxyModel_);
    treeView_->setRootIndex(proxyModel_->mapFromSource(fileModel_->index(currentDirectory_)));

    // QFileSystemModel columns: 0=Name, 1=Size, 2=Type, 3=Date Modified
    // Show Name, Size, Type (with C64-specific types); hide Date Modified
    treeView_->hideColumn(3);  // Date Modified

    // Set column widths: Size and Type get fixed widths, Name stretches
    treeView_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    treeView_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    treeView_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    fileOps_ = new LocalFileOperationsService(this);
    connect(fileOps_, &LocalFileOperationsService::statusMessage, this,
            [this](const QString &message, int /*timeout*/) {
                errorHandler_->info(ErrorCategory::FileOperation, message);
            });
    errorHandler_->registerSource(fileOps_);

    LocalFileBrowserWidget::updateActions();  // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
}

void LocalFileBrowserWidget::setupContextMenu()
{
    // Call base class to create menu
    FileBrowserWidget::setupContextMenu();

    // Connect the set destination action
    connect(setDestAction_, &QAction::triggered, this, [this]() {
        QModelIndex proxyIndex = treeView_->currentIndex();
        if (proxyIndex.isValid()) {
            QModelIndex sourceIndex = proxyModel_->mapToSource(proxyIndex);
            if (fileModel_->isDir(sourceIndex)) {
                QString path = fileModel_->filePath(sourceIndex);
                setCurrentDirectory(path);
            }
        }
    });

    // Add local-specific menu items: the toolbar's own actions, so enablement is shared
    using pixeliconcore::Icon;
    contextMenu_->addSeparator();
    contextMenu_->addAction(pixelicons::icon(Icon::Upload), tr("Upload to C64U"), this,
                            &LocalFileBrowserWidget::onUpload);
    contextMenu_->addSeparator();
    contextMenu_->addAction(newFolderAction_);
    contextMenu_->addAction(renameAction_);
    contextMenu_->addAction(deleteAction_);
}

void LocalFileBrowserWidget::setupConnections()
{
    // Call base class to set up standard connections
    FileBrowserWidget::setupConnections();
}

void LocalFileBrowserWidget::updateActions()
{
    bool hasSelection = !LocalFileBrowserWidget::selectedPath()
                             .isEmpty();  // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)

    updateCommonActions();
    uploadAction_->setEnabled(hasSelection);
}

void LocalFileBrowserWidget::setUploadEnabled(bool enabled)
{
    bool hasSelection = !selectedPath().isEmpty();
    uploadAction_->setEnabled(enabled && hasSelection);
}

QString LocalFileBrowserWidget::selectedPath() const
{
    QModelIndex proxyIndex = treeView_->currentIndex();
    if (proxyIndex.isValid()) {
        QModelIndex sourceIndex = proxyModel_->mapToSource(proxyIndex);
        return fileModel_->filePath(sourceIndex);
    }
    return QString();
}

bool LocalFileBrowserWidget::isSelectedDirectory() const
{
    QModelIndex proxyIndex = treeView_->currentIndex();
    if (proxyIndex.isValid()) {
        QModelIndex sourceIndex = proxyModel_->mapToSource(proxyIndex);
        return fileModel_->isDir(sourceIndex);
    }
    return false;
}

QAbstractItemModel *LocalFileBrowserWidget::model() const
{
    return proxyModel_;
}

QString LocalFileBrowserWidget::filePath(const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid()) {
        return QString();
    }
    QModelIndex sourceIndex = proxyModel_->mapToSource(proxyIndex);
    return fileModel_->filePath(sourceIndex);
}

bool LocalFileBrowserWidget::isDirectory(const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid()) {
        return false;
    }
    QModelIndex sourceIndex = proxyModel_->mapToSource(proxyIndex);
    return fileModel_->isDir(sourceIndex);
}

void LocalFileBrowserWidget::navigateToDirectory(const QString &path)
{
    setCurrentDirectory(path);
}

void LocalFileBrowserWidget::setCurrentDirectory(const QString &path)
{
    currentDirectory_ = path;

    // Update tree view to show this folder as root
    fileModel_->setRootPath(path);
    treeView_->setRootIndex(proxyModel_->mapFromSource(fileModel_->index(path)));

    // Shorten path for display by replacing home with ~
    QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QString displayPath = path;
    if (displayPath.startsWith(homePath)) {
        displayPath = "~" + displayPath.mid(homePath.length());
    }

    navWidget_->setPath(displayPath);
    emit currentDirectoryChanged(path);

    // Enable/disable up button based on whether we can go up
    QDir dir(path);
    bool canGoUp = dir.cdUp();
    navWidget_->setUpEnabled(canGoUp);
}

void LocalFileBrowserWidget::onUpload()
{
    auto entries = selectedEntries();
    if (entries.isEmpty()) {
        errorHandler_->handleError(ErrorCategory::Validation, ErrorSeverity::Warning,
                                   tr("No local file selected"));
        return;
    }

    for (const auto &e : entries) {
        emit uploadRequested(e.path, e.isDirectory);
    }
}

void LocalFileBrowserWidget::executeDropPlan(const dropcore::DropPlan &plan)
{
    if (plan.kind != dropcore::DropPlan::Kind::Download) {
        qCWarning(LogUi) << "LocalFileBrowserWidget: ignoring a drop plan that is not a download";
        return;
    }
    for (const auto &entry : plan.entries) {
        emit downloadRequested(entry.path, entry.isDirectory, entry.size,
                               plan.destinationDirectory);
    }
}

void LocalFileBrowserWidget::performNewFolder(const QString &folderName)
{
    fileOps_->createFolder(currentDirectory_, folderName);
}

void LocalFileBrowserWidget::performRename(const QString &path, const QString &newName)
{
    fileOps_->renameItem(path, newName);
}

void LocalFileBrowserWidget::performDelete(const QList<SelectedEntry> &entries)
{
    QStringList paths;
    for (const auto &e : entries) {
        paths.append(e.path);
    }
    fileOps_->deleteItems(paths);
}

QString LocalFileBrowserWidget::deleteVerbPhrase() const
{
    return tr("move to the trash");
}

QString LocalFileBrowserWidget::deleteActionLabel() const
{
    return tr("Move to Trash");
}

IMessagePresenter::MessageIcon LocalFileBrowserWidget::deleteIcon() const
{
    return IMessagePresenter::MessageIcon::Question;
}
