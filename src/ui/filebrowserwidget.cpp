#include "filebrowserwidget.h"

#include "pathnavigationwidget.h"

#include "services/errorhandler.h"
#include "utils/logging.h"

#include <QAbstractItemModel>
#include <QFileInfo>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMenu>
#include <QSet>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>

FileBrowserWidget::FileBrowserWidget(QWidget *parent) : QWidget(parent) {}

void FileBrowserWidget::setErrorHandler(ErrorHandler *handler)
{
    errorHandler_ = handler;
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

QStringList FileBrowserWidget::selectedPaths() const
{
    QStringList paths;
    if (!treeView_ || !treeView_->selectionModel()) {
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
        QString path = filePath(index);
        if (!path.isEmpty() && !seenPaths.contains(path)) {
            seenPaths.insert(path);
            paths.append(path);
        }
    }

    return paths;
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
