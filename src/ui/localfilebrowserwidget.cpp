#include "localfilebrowserwidget.h"
#include "pathnavigationwidget.h"
#include "models/localfileproxymodel.h"

#include <QStandardPaths>
#include <QMessageBox>
#include <QPushButton>
#include <QInputDialog>
#include <QFileInfo>
#include <QDir>
#include <QTreeView>
#include <QToolBar>
#include <QMenu>

LocalFileBrowserWidget::LocalFileBrowserWidget(QWidget *parent)
    : FileBrowserWidget(parent)
{
    currentDirectory_ = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    setupUi();
    setupContextMenu();
    setupConnections();
}

void LocalFileBrowserWidget::setupUi()
{
    // Call base class to create standard UI structure
    FileBrowserWidget::setupUi();

    // Set green style for nav widget
    navWidget_->setStyleGreen();

    // Add local-specific actions to toolbar
    uploadAction_ = toolBar_->addAction(tr("Upload"));
    uploadAction_->setToolTip(tr("Upload selected files to C64U"));
    connect(uploadAction_, &QAction::triggered, this, &LocalFileBrowserWidget::onUpload);

    newFolderAction_ = toolBar_->addAction(tr("New Folder"));
    newFolderAction_->setToolTip(tr("Create new folder in local directory"));
    connect(newFolderAction_, &QAction::triggered, this, &LocalFileBrowserWidget::onNewFolder);

    renameAction_ = toolBar_->addAction(tr("Rename"));
    renameAction_->setToolTip(tr("Rename selected local file or folder"));
    connect(renameAction_, &QAction::triggered, this, &LocalFileBrowserWidget::onRename);

    deleteAction_ = toolBar_->addAction(tr("Delete"));
    deleteAction_->setToolTip(tr("Move selected local file to trash"));
    connect(deleteAction_, &QAction::triggered, this, &LocalFileBrowserWidget::onDelete);

    // Set up the model
    fileModel_ = new QFileSystemModel(this);
    fileModel_->setRootPath(currentDirectory_);

    // Use proxy model to show file sizes in bytes instead of KB/MB
    proxyModel_ = new LocalFileProxyModel(this);
    proxyModel_->setSourceModel(fileModel_);
    treeView_->setModel(proxyModel_);
    treeView_->setRootIndex(proxyModel_->mapFromSource(fileModel_->index(currentDirectory_)));

    updateActions();
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

    // Add local-specific menu items
    contextMenu_->addSeparator();
    contextMenu_->addAction(tr("Upload to C64U"), this, &LocalFileBrowserWidget::onUpload);
    contextMenu_->addSeparator();
    contextMenu_->addAction(tr("New Folder"), this, &LocalFileBrowserWidget::onNewFolder);
    contextMenu_->addAction(tr("Rename"), this, &LocalFileBrowserWidget::onRename);
    contextMenu_->addAction(tr("Delete"), this, &LocalFileBrowserWidget::onDelete);
}

void LocalFileBrowserWidget::setupConnections()
{
    // Call base class to set up standard connections
    FileBrowserWidget::setupConnections();
}

void LocalFileBrowserWidget::updateActions()
{
    bool hasSelection = !selectedPath().isEmpty();

    uploadAction_->setEnabled(hasSelection);
    newFolderAction_->setEnabled(true);
    renameAction_->setEnabled(hasSelection);
    deleteAction_->setEnabled(hasSelection);
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

QAbstractItemModel* LocalFileBrowserWidget::model() const
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
    emit statusMessage(tr("Download destination: %1").arg(displayPath), 2000);

    // Enable/disable up button based on whether we can go up
    QDir dir(path);
    bool canGoUp = dir.cdUp();
    navWidget_->setUpEnabled(canGoUp);
}

void LocalFileBrowserWidget::onUpload()
{
    QString localPath = selectedPath();
    if (localPath.isEmpty()) {
        emit statusMessage(tr("No local file selected"), 3000);
        return;
    }

    QFileInfo fileInfo(localPath);
    emit uploadRequested(localPath, fileInfo.isDir());
}

void LocalFileBrowserWidget::onNewFolder()
{
    bool ok;
    QString folderName = QInputDialog::getText(this, tr("New Local Folder"),
        tr("Folder name:"), QLineEdit::Normal, "", &ok);

    if (!ok || folderName.isEmpty()) {
        return;
    }

    QString newPath = currentDirectory_;
    if (!newPath.endsWith('/')) {
        newPath += '/';
    }
    newPath += folderName;

    QDir dir;
    if (dir.mkdir(newPath)) {
        emit statusMessage(tr("Local folder created: %1").arg(folderName), 3000);
    } else {
        QMessageBox::warning(this, tr("Error"),
            tr("Failed to create folder: %1").arg(newPath));
    }
}

void LocalFileBrowserWidget::onRename()
{
    QString localPath = selectedPath();
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

    if (newName == oldName) {
        return;
    }

    if (newName.contains('/') || newName.contains('\\')) {
        QMessageBox::warning(this, tr("Invalid Name"),
            tr("The name cannot contain '/' or '\\' characters."));
        return;
    }

    QString newPath = fileInfo.absolutePath() + "/" + newName;

    if (QFileInfo::exists(newPath)) {
        QMessageBox::warning(this, tr("Rename Failed"),
            tr("A %1 with the name '%2' already exists.").arg(itemType).arg(newName));
        return;
    }

    QDir dir;
    bool success = dir.rename(localPath, newPath);

    if (success) {
        emit statusMessage(tr("Renamed: %1 -> %2").arg(oldName).arg(newName), 3000);
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
        emit statusMessage(tr("Failed to rename: %1").arg(oldName), 3000);
    }
}

void LocalFileBrowserWidget::onDelete()
{
    QString localPath = selectedPath();
    if (localPath.isEmpty()) {
        return;
    }

    QFileInfo fileInfo(localPath);
    QString itemName = fileInfo.fileName();
    QString itemType = fileInfo.isDir() ? tr("folder") : tr("file");

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Move to Trash"));
    msgBox.setText(tr("Are you sure you want to move the %1 '%2' to the trash?").arg(itemType).arg(itemName));
    msgBox.setIcon(QMessageBox::Question);
    QPushButton *trashButton = msgBox.addButton(tr("Move to Trash"), QMessageBox::AcceptRole);
    msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
    msgBox.setDefaultButton(trashButton);
    msgBox.exec();

    if (msgBox.clickedButton() != trashButton) {
        return;
    }

    QString pathInTrash;
    bool success = QFile::moveToTrash(localPath, &pathInTrash);

    if (success) {
        emit statusMessage(tr("Moved to trash: %1").arg(itemName), 3000);
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
        emit statusMessage(tr("Failed to delete: %1").arg(itemName), 3000);
    }
}
