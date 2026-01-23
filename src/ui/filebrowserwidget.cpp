#include "filebrowserwidget.h"
#include "pathnavigationwidget.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QTreeView>
#include <QToolBar>
#include <QMenu>
#include <QAbstractItemModel>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QFileInfo>

FileBrowserWidget::FileBrowserWidget(QWidget *parent)
    : QWidget(parent)
{
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
    connect(navWidget_, &PathNavigationWidget::upClicked,
            this, &FileBrowserWidget::onParentFolder);
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
        connect(treeView_, &QTreeView::doubleClicked,
                this, &FileBrowserWidget::onDoubleClicked);
        connect(treeView_, &QTreeView::customContextMenuRequested,
                this, &FileBrowserWidget::onContextMenu);

        if (treeView_->selectionModel()) {
            connect(treeView_->selectionModel(), &QItemSelectionModel::selectionChanged,
                    this, [this]() {
                updateActions();
                emit selectionChanged();
            });
        }
    }
}

void FileBrowserWidget::onDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) {
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
