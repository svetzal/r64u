#include "filebrowserwidget.h"

#include "pathnavigationwidget.h"

#include "core/filebrowsercore.h"
#include "services/errorhandler.h"
#include "utils/logging.h"

#include <QAbstractItemModel>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QSet>
#include <QStyle>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>

namespace {

/// The entries a drop carries: remote paths as encoded by RemoteFileModel, or local file URLs.
QList<dropcore::DropEntry> entriesFrom(const QMimeData &mime)
{
    const QString remoteType = QString::fromLatin1(dropcore::kRemotePathsMimeType);
    if (mime.hasFormat(remoteType)) {
        return dropcore::decodeRemoteEntries(mime.data(remoteType));
    }

    QList<dropcore::DropEntry> entries;
    const QList<QUrl> urls = mime.urls();
    for (const QUrl &url : urls) {
        if (!url.isLocalFile()) {
            continue;
        }
        const QFileInfo info(url.toLocalFile());
        entries.append({info.filePath(), info.isDir(), info.isDir() ? 0 : info.size()});
    }
    return entries;
}

}  // namespace

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

    // Rows can be dragged to the other pane (or the Finder) and the other pane's
    // rows dropped here. Every transfer is a copy: a drag must never end as a
    // move, which would have the view remove the dragged rows from its model.
    treeView_->setDragEnabled(true);
    treeView_->setAcceptDrops(true);
    treeView_->setDropIndicatorShown(true);
    treeView_->setDragDropMode(QAbstractItemView::DragDrop);
    treeView_->setDefaultDropAction(Qt::CopyAction);
    treeView_->viewport()->installEventFilter(this);
    layout->addWidget(treeView_);

    // Initialize nav widget with current directory
    navWidget_->setPath(currentDirectory_);
}

void FileBrowserWidget::setupContextMenu()
{
    contextMenu_ = new QMenu(this);
    contextMenu_->setObjectName(QStringLiteral("ItemMenu"));
    setDestAction_ = contextMenu_->addAction(tr("Set as Destination"));

    emptySpaceMenu_ = new QMenu(this);
    emptySpaceMenu_->setObjectName(QStringLiteral("EmptySpaceMenu"));
    if (newFolderAction_) {
        emptySpaceMenu_->addAction(newFolderAction_);
    }
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

bool FileBrowserWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (!treeView_ || watched != treeView_->viewport()) {
        return QWidget::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::DragEnter:
    case QEvent::DragMove: {
        auto *dragEvent = static_cast<QDragMoveEvent *>(event);
        const bool fromThisPane = dragEvent->source() == treeView_;
        if (canAcceptDrag(dragEvent->mimeData(), fromThisPane)) {
            dragEvent->setDropAction(Qt::CopyAction);
            dragEvent->accept();
            setDropActive(true);
        } else {
            dragEvent->ignore();
        }
        return true;
    }
    case QEvent::DragLeave:
        setDropActive(false);
        return true;
    case QEvent::Drop: {
        auto *dropEvent = static_cast<QDropEvent *>(event);
        setDropActive(false);
        const bool fromThisPane = dropEvent->source() == treeView_;
        if (handleDrop(dropEvent->mimeData(), dropEvent->position().toPoint(), fromThisPane)) {
            dropEvent->setDropAction(Qt::CopyAction);
            dropEvent->accept();
        } else {
            dropEvent->ignore();
        }
        return true;
    }
    default:
        return QWidget::eventFilter(watched, event);
    }
}

std::optional<dropcore::Source> FileBrowserWidget::dragSource(const QMimeData *mime,
                                                              bool fromThisPane) const
{
    if (!mime) {
        return std::nullopt;
    }
    return dropcore::classifySource(
        fromThisPane, pane(), mime->hasFormat(QString::fromLatin1(dropcore::kRemotePathsMimeType)),
        mime->hasUrls());
}

bool FileBrowserWidget::canAcceptDrag(const QMimeData *mime, bool fromThisPane) const
{
    const auto source = dragSource(mime, fromThisPane);
    return source.has_value() && dropcore::canAccept(*source, pane());
}

bool FileBrowserWidget::handleDrop(const QMimeData *mime, const QPoint &viewportPos,
                                   bool fromThisPane)
{
    const auto source = dragSource(mime, fromThisPane);
    if (!source || !treeView_) {
        return false;
    }

    const QModelIndex rowUnderCursor = treeView_->indexAt(viewportPos);
    dropcore::DropTarget target;
    target.pane = pane();
    target.currentDirectory = currentDirectory_;
    target.onDirectoryRow = rowUnderCursor.isValid() && isDirectory(rowUnderCursor);
    target.rowPath = target.onDirectoryRow ? filePath(rowUnderCursor) : QString();

    const auto plan = dropcore::planDrop(*source, entriesFrom(*mime), target);
    if (!plan) {
        return false;
    }
    executeDropPlan(*plan);
    return true;
}

void FileBrowserWidget::setDropActive(bool active)
{
    if (!treeView_ || treeView_->property("dropActive").toBool() == active) {
        return;
    }
    treeView_->setProperty("dropActive", active);
    treeView_->style()->unpolish(treeView_);
    treeView_->style()->polish(treeView_);
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
            entries.append({path, isDirectory(index), fileSize(index)});
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

void FileBrowserWidget::setMessagePresenter(IMessagePresenter *presenter)
{
    presenter_ = presenter ? presenter : &defaultPresenter_;
}

QByteArray FileBrowserWidget::headerState() const
{
    return treeView_ ? treeView_->header()->saveState() : QByteArray();
}

void FileBrowserWidget::restoreHeaderState(const QByteArray &state)
{
    if (treeView_ && !state.isEmpty()) {
        treeView_->header()->restoreState(state);
    }
}

bool FileBrowserWidget::confirmDestructiveAction(const QString &title, const QString &message,
                                                 const QString &acceptText,
                                                 IMessagePresenter::MessageIcon icon)
{
    const QList<IMessagePresenter::DialogButton> buttons = {
        {acceptText, IMessagePresenter::ButtonRole::Destructive},
        {tr("Cancel"), IMessagePresenter::ButtonRole::Reject},
    };
    // Enter must never trigger the destructive action: Cancel is the default.
    constexpr int kCancelIndex = 1;
    const int result = presenter_->confirm(this, title, message, buttons, icon, kCancelIndex);
    return result == 0;
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
        navigateToDirectory(filePath(index));
    } else {
        requestTransferOfSelection();
    }
}

void FileBrowserWidget::onContextMenu(const QPoint &pos)
{
    const QModelIndex index = treeView_->indexAt(pos);
    QMenu *menu = index.isValid() ? contextMenu_ : emptySpaceMenu_;
    if (!menu) {
        return;
    }
    if (index.isValid() && setDestAction_) {
        setDestAction_->setEnabled(isDirectory(index));
    }
    menu->exec(treeView_->viewport()->mapToGlobal(pos));
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

IMessagePresenter::MessageIcon FileBrowserWidget::deleteIcon() const
{
    return IMessagePresenter::MessageIcon::Warning;
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
