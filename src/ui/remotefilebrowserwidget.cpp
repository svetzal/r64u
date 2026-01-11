#include "remotefilebrowserwidget.h"
#include "pathnavigationwidget.h"
#include "models/remotefilemodel.h"
#include "services/c64uftpclient.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QInputDialog>
#include <QFileInfo>
#include <QShowEvent>

RemoteFileBrowserWidget::RemoteFileBrowserWidget(RemoteFileModel *model,
                                                 C64UFtpClient *ftpClient,
                                                 QWidget *parent)
    : QWidget(parent)
    , remoteFileModel_(model)
    , ftpClient_(ftpClient)
    , currentDirectory_("/")
{
    // These dependencies are required - assert in debug builds
    Q_ASSERT(remoteFileModel_ && "RemoteFileModel is required");
    // ftpClient_ may be null if connection not established yet

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
    connect(navWidget_, &PathNavigationWidget::upClicked, this, &RemoteFileBrowserWidget::onParentFolder);
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
    treeView_->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    connect(treeView_, &QTreeView::customContextMenuRequested,
            this, &RemoteFileBrowserWidget::onContextMenu);
    connect(treeView_, &QTreeView::doubleClicked,
            this, &RemoteFileBrowserWidget::onDoubleClicked);
    // Guard against null selection model
    if (auto *selModel = treeView_->selectionModel()) {
        connect(selModel, &QItemSelectionModel::selectionChanged,
                this, [this]() { updateActions(); emit selectionChanged(); });
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
            return;
        }
        QModelIndex index = treeView_->currentIndex();
        if (index.isValid() && remoteFileModel_->isDirectory(index)) {
            QString path = remoteFileModel_->filePath(index);
            setCurrentDirectory(path);
        }
    });
    contextMenu_->addSeparator();
    contextMenu_->addAction(tr("Download to Local Directory"), this, &RemoteFileBrowserWidget::onDownload);
    contextMenu_->addAction(tr("Delete"), this, &RemoteFileBrowserWidget::onDelete);
    contextMenu_->addSeparator();
    contextMenu_->addAction(tr("New Folder"), this, &RemoteFileBrowserWidget::onNewFolder);
    contextMenu_->addAction(tr("Refresh"), this, &RemoteFileBrowserWidget::onRefresh);
}

void RemoteFileBrowserWidget::setupConnections()
{
    // FTP client signals - guard against null ftpClient_
    if (ftpClient_) {
        connect(ftpClient_, &C64UFtpClient::directoryCreated,
                this, &RemoteFileBrowserWidget::onDirectoryCreated);
        connect(ftpClient_, &C64UFtpClient::fileRemoved,
                this, &RemoteFileBrowserWidget::onFileRemoved);
        connect(ftpClient_, &C64UFtpClient::fileRenamed,
                this, &RemoteFileBrowserWidget::onFileRenamed);
    }
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
    emit statusMessage(tr("Upload destination: %1").arg(path), 2000);

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

void RemoteFileBrowserWidget::refresh()
{
    onRefresh();
}

void RemoteFileBrowserWidget::refreshIfStale()
{
    if (!connected_ || suppressAutoRefresh_) {
        return;
    }

    if (remoteFileModel_) {
        remoteFileModel_->refreshIfStale();
    }
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
    QString remotePath = selectedPath();
    if (remotePath.isEmpty()) {
        emit statusMessage(tr("No remote file selected"), 3000);
        return;
    }

    emit downloadRequested(remotePath, isSelectedDirectory());
}

void RemoteFileBrowserWidget::onNewFolder()
{
    if (!connected_ || !ftpClient_) {
        return;
    }

    QString remoteDir = currentDirectory_;
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
    ftpClient_->makeDirectory(newPath);
    emit statusMessage(tr("Creating folder %1 in %2...").arg(folderName).arg(remoteDir));
}

void RemoteFileBrowserWidget::onRename()
{
    if (!connected_ || !ftpClient_) {
        return;
    }

    QString remotePath = selectedPath();
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

    if (newName == oldName) {
        return;
    }

    if (newName.contains('/') || newName.contains('\\')) {
        QMessageBox::warning(this, tr("Invalid Name"),
            tr("The name cannot contain '/' or '\\' characters."));
        return;
    }

    QString parentPath = fileInfo.path();
    if (!parentPath.endsWith('/')) {
        parentPath += '/';
    }
    QString newPath = parentPath + newName;

    ftpClient_->rename(remotePath, newPath);
    emit statusMessage(tr("Renaming %1...").arg(oldName));
}

void RemoteFileBrowserWidget::onDelete()
{
    if (!connected_) {
        return;
    }

    QString remotePath = selectedPath();
    if (remotePath.isEmpty()) {
        return;
    }

    QString fileName = QFileInfo(remotePath).fileName();
    bool isDir = isSelectedDirectory();

    QString confirmMessage = isDir
        ? tr("Are you sure you want to permanently delete the folder '%1' and all its contents?\n\nThis cannot be undone.").arg(fileName)
        : tr("Are you sure you want to permanently delete '%1'?\n\nThis cannot be undone.").arg(fileName);

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

    emit deleteRequested(remotePath, isDir);
    emit statusMessage(tr("Deleting %1...").arg(fileName));
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
    suppressAutoRefresh_ = suppress;
}

void RemoteFileBrowserWidget::onDirectoryCreated(const QString &path)
{
    emit statusMessage(tr("Folder created: %1").arg(QFileInfo(path).fileName()), 3000);
    if (!suppressAutoRefresh_ && remoteFileModel_) {
        remoteFileModel_->refresh();
    }
}

void RemoteFileBrowserWidget::onFileRemoved(const QString &path)
{
    // Don't emit status messages or refresh during bulk delete operations
    if (!suppressAutoRefresh_ && remoteFileModel_) {
        emit statusMessage(tr("Deleted: %1").arg(QFileInfo(path).fileName()), 3000);
        remoteFileModel_->refresh();
    }
}

void RemoteFileBrowserWidget::onFileRenamed(const QString &oldPath, const QString &newPath)
{
    QString oldName = QFileInfo(oldPath).fileName();
    QString newName = QFileInfo(newPath).fileName();
    emit statusMessage(tr("Renamed: %1 -> %2").arg(oldName).arg(newName), 3000);
    if (!suppressAutoRefresh_ && remoteFileModel_) {
        remoteFileModel_->refresh();
    }
}
