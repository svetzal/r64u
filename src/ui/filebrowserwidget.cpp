#include "filebrowserwidget.h"

#include "pathnavigationwidget.h"

#include "core/filebrowsercore.h"

#include "services/errorhandler.h"
#include "utils/logging.h"

#include <QAbstractItemModel>
#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>

FileBrowserWidget::FileBrowserWidget(ErrorHandler *errorHandler, QWidget *parent)
    : QWidget(parent), errorHandler_(errorHandler)
{
    Q_ASSERT(errorHandler_ && "ErrorHandler is required");
}

void FileBrowserWidget::setCurrentDirectory(const QString &path)
{
    if (currentDirectory_ == path) {
        return;
    }

    currentDirectory_ = path;
    if (navWidget_) {
        navWidget_->setPath(path);
    }
    emit currentDirectoryChanged(path);
}

void FileBrowserWidget::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    // Label
    auto *label = new QLabel(labelText());
    label->setStyleSheet("font-weight: bold;");
    layout->addWidget(label);

    // Path navigation widget
    navWidget_ = new PathNavigationWidget(navLabelText());
    connect(navWidget_, &PathNavigationWidget::upClicked, this, &FileBrowserWidget::onParentFolder);
    layout->addWidget(navWidget_);

    // Toolbar - subclasses will add their specific actions
    toolBar_ = new QToolBar();
    toolBar_->setIconSize(QSize(16, 16));
    toolBar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    layout->addWidget(toolBar_);

    // Tree view
    treeView_ = new QTreeView();
    treeView_->setAlternatingRowColors(true);
    treeView_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    treeView_->setContextMenuPolicy(Qt::CustomContextMenu);
    treeView_->setSortingEnabled(true);
    treeView_->sortByColumn(0, Qt::AscendingOrder);  // Sort by name, folders first via proxy
    // Note: setSectionResizeMode is called by subclasses after they set the model
    layout->addWidget(treeView_);

    // Initialize nav widget with current directory
    navWidget_->setPath(currentDirectory_);
}

void FileBrowserWidget::setupContextMenu()
{
    contextMenu_ = new QMenu(this);
    setDestAction_ = contextMenu_->addAction(tr("Set as Destination"));
}

void FileBrowserWidget::setupConnections()
{
    if (treeView_) {
        connect(treeView_, &QTreeView::doubleClicked, this, &FileBrowserWidget::onDoubleClicked);
        connect(treeView_, &QTreeView::customContextMenuRequested, this,
                &FileBrowserWidget::onContextMenu);

        if (treeView_->selectionModel()) {
            connect(treeView_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                    [this]() {
                        updateActions();
                        emit selectionChanged();
                    });
        }
    } else {
        qCDebug(LogUi) << "setupConnections: treeView_ is null, skipping connection setup";
    }
}

QList<FileBrowserWidget::SelectedEntry> FileBrowserWidget::selectedEntries() const
{
    QList<SelectedEntry> entries;
    if (!treeView_ || !treeView_->selectionModel()) {
        return entries;
    }

    QModelIndexList selectedIndexes = treeView_->selectionModel()->selectedIndexes();
    QSet<QString> seenPaths;

    for (const QModelIndex &index : selectedIndexes) {
        if (index.column() != 0) {
            continue;
        }
        QString path = filePath(index);
        if (!path.isEmpty() && !seenPaths.contains(path)) {
            seenPaths.insert(path);
            entries.append({path, isDirectory(index)});
        }
    }

    return entries;
}

QStringList FileBrowserWidget::selectedPaths() const
{
    QStringList paths;
    for (const auto &entry : selectedEntries()) {
        paths.append(entry.path);
    }
    return paths;
}

bool FileBrowserWidget::confirmDestructiveAction(const QString &title, const QString &message,
                                                 const QString &acceptText, QMessageBox::Icon icon)
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(title);
    msgBox.setText(message);
    msgBox.setIcon(icon);
    QPushButton *acceptButton = msgBox.addButton(acceptText, QMessageBox::DestructiveRole);
    msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
    msgBox.exec();
    return msgBox.clickedButton() == acceptButton;
}

QString FileBrowserWidget::promptForNewName(const QString &title, const QString &oldName) const
{
    bool ok;
    QString newName = QInputDialog::getText(const_cast<FileBrowserWidget *>(this), title,
                                            tr("New name:"), QLineEdit::Normal, oldName, &ok);
    if (!ok || newName.isEmpty() || newName == oldName) {
        return {};
    }
    if (newName.contains('/') || newName.contains('\\')) {
        QMessageBox::warning(const_cast<FileBrowserWidget *>(this), tr("Invalid Name"),
                             tr("The name cannot contain '/' or '\\' characters."));
        return {};
    }
    return newName;
}

void FileBrowserWidget::updateCommonActions(bool extraCondition)
{
    bool hasSelection = !selectedPath().isEmpty();

    if (newFolderAction_) {
        newFolderAction_->setEnabled(extraCondition);
    }
    if (renameAction_) {
        renameAction_->setEnabled(extraCondition && hasSelection);
    }
    if (deleteAction_) {
        deleteAction_->setEnabled(extraCondition && hasSelection);
    }
}

void FileBrowserWidget::onDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) {
        qCDebug(LogUi) << "onDoubleClicked: invalid index, skipping";
        return;
    }

    if (isDirectory(index)) {
        QString path = filePath(index);
        navigateToDirectory(path);
    }
}

void FileBrowserWidget::onContextMenu(const QPoint &pos)
{
    QModelIndex index = treeView_->indexAt(pos);
    if (!index.isValid()) {
        qCDebug(LogUi) << "onContextMenu: no item at position, skipping context menu";
        return;
    }

    if (contextMenu_) {
        contextMenu_->exec(treeView_->viewport()->mapToGlobal(pos));
    }
}

void FileBrowserWidget::onParentFolder()
{
    QFileInfo info(currentDirectory_);
    QString parentPath = info.absolutePath();

    if (parentPath != currentDirectory_) {
        navigateToDirectory(parentPath);
    }
}

bool FileBrowserWidget::canModify(const QString & /*actionLabel*/)
{
    return true;
}

QString FileBrowserWidget::deleteVerbPhrase() const
{
    return tr("delete");
}

QString FileBrowserWidget::deleteActionLabel() const
{
    return tr("Delete");
}

QMessageBox::Icon FileBrowserWidget::deleteIcon() const
{
    return QMessageBox::Warning;
}

void FileBrowserWidget::onNewFolder()
{
    if (!canModify(tr("create folder"))) {
        return;
    }

    bool ok;
    QString folderName = QInputDialog::getText(this, tr("New Folder"), tr("Folder name:"),
                                               QLineEdit::Normal, QString(), &ok);
    if (!ok || folderName.isEmpty()) {
        return;
    }

    performNewFolder(folderName);
}

void FileBrowserWidget::onRename()
{
    if (!canModify(tr("rename"))) {
        return;
    }

    QString path = selectedPath();
    if (path.isEmpty()) {
        return;
    }

    QFileInfo fileInfo(path);
    QString oldName = fileInfo.fileName();
    QString itemType = isSelectedDirectory() ? tr("folder") : tr("file");

    QString newName = promptForNewName(tr("Rename %1").arg(itemType), oldName);
    if (newName.isEmpty()) {
        return;
    }

    performRename(path, newName);
}

void FileBrowserWidget::onDelete()
{
    if (!canModify(tr("delete"))) {
        return;
    }

    auto entries = selectedEntries();
    if (entries.isEmpty()) {
        return;
    }

    QStringList paths;
    for (const auto &e : entries) {
        paths.append(e.path);
    }

    QString confirmMessage =
        filebrowser::buildDeleteConfirmMessage(paths, isSelectedDirectory(), deleteVerbPhrase());

    if (!confirmDestructiveAction(deleteActionLabel(), confirmMessage, deleteActionLabel(),
                                  deleteIcon())) {
        return;
    }

    performDelete(entries);
}
