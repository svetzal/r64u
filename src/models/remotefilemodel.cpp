#include "remotefilemodel.h"

#include "core/filetypecore.h"
#include "core/remotefiletreecore.h"
#include "utils/logging.h"

#include <QApplication>
#include <QStyle>

#include <functional>

RemoteFileModel::RemoteFileModel(QObject *parent)
    : QAbstractItemModel(parent), coordinator_(new RemoteListingCoordinator(this)),
      rootNode_(new TreeNode)
{
    rootNode_->name = "/";
    rootNode_->fullPath = "/";
    rootNode_->isDirectory = true;
    rootNode_->fileType = filetype::FileType::Directory;

    connect(coordinator_, &RemoteListingCoordinator::listingReady, this,
            &RemoteFileModel::onListingReady);
    connect(coordinator_, &RemoteListingCoordinator::listingFailed, this,
            &RemoteFileModel::onListingFailed);
}

RemoteFileModel::~RemoteFileModel()
{
    // Disconnect coordinator from FTP client BEFORE deleting nodes to prevent
    // signals from being delivered to slots that access deleted memory.
    coordinator_->setFtpClient(nullptr);

    delete rootNode_;
}

void RemoteFileModel::setFtpClient(IFtpClient *client)
{
    coordinator_->setFtpClient(client);
}

void RemoteFileModel::setRootPath(const QString &path)
{
    beginResetModel();

    // Cancel pending operations BEFORE deleting nodes to prevent dangling pointer access
    // if a signal is delivered between deletion and clearing
    coordinator_->cancelPending();
    pendingFetches_.clear();

    rootPath_ = path;
    delete rootNode_;
    rootNode_ = new TreeNode;
    rootNode_->name = path;
    rootNode_->fullPath = path;
    rootNode_->isDirectory = true;
    rootNode_->fileType = filetype::FileType::Directory;

    endResetModel();
}

QModelIndex RemoteFileModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    TreeNode *parentNode = nodeFromIndex(parent);
    if (!parentNode || row >= parentNode->children.count()) {
        return QModelIndex();
    }

    return createIndex(row, column, parentNode->children.at(row));
}

QModelIndex RemoteFileModel::parent(const QModelIndex &child) const
{
    if (!child.isValid()) {
        return QModelIndex();
    }

    TreeNode *childNode = nodeFromIndex(child);
    if (!childNode || !childNode->parent || childNode->parent == rootNode_) {
        return QModelIndex();
    }

    TreeNode *parentNode = childNode->parent;
    TreeNode *grandparent = parentNode->parent;

    if (!grandparent) {
        return QModelIndex();
    }

    int row = grandparent->children.indexOf(parentNode);
    return createIndex(row, 0, parentNode);
}

int RemoteFileModel::rowCount(const QModelIndex &parent) const
{
    TreeNode *node = nodeFromIndex(parent);
    return node ? node->children.count() : 0;
}

int RemoteFileModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 3;  // Name, Size, Type
}

QVariant RemoteFileModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    TreeNode *node = nodeFromIndex(index);
    if (!node) {
        return QVariant();
    }

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case 0:
            return node->name;
        case 1:
            return node->isDirectory ? QVariant() : QString::number(node->size);
        case 2:
            return fileTypeString(node->fileType);
        default:
            break;
        }
        break;

    case Qt::DecorationRole:
        if (index.column() == 0) {
            return iconForFileType(node->fileType);
        }
        break;

    case Qt::TextAlignmentRole:
        if (index.column() == 1) {
            return Qt::AlignRight;
        }
        break;

    case FilePathRole:
        return node->fullPath;

    case IsDirectoryRole:
        return node->isDirectory;

    case FileSizeRole:
        return node->size;

    case FileTypeRole:
        return static_cast<int>(node->fileType);

    default:
        break;
    }

    return QVariant();
}

QVariant RemoteFileModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QVariant();
    }

    switch (section) {
    case 0:
        return tr("Name");
    case 1:
        return tr("Size");
    case 2:
        return tr("Type");
    default:
        break;
    }

    return QVariant();
}

bool RemoteFileModel::hasChildren(const QModelIndex &parent) const
{
    TreeNode *node = nodeFromIndex(parent);
    if (!node) {
        return false;
    }

    // Directories always potentially have children until fetched
    if (node->isDirectory && !node->fetched) {
        return true;
    }

    return !node->children.isEmpty();
}

bool RemoteFileModel::canFetchMore(const QModelIndex &parent) const
{
    TreeNode *node = nodeFromIndex(parent);
    if (!node) {
        return false;
    }

    if (!node->isDirectory || node->fetching) {
        return false;
    }

    // Can fetch if not yet fetched, or if stale (TTL expired)
    if (!node->fetched) {
        return true;
    }

    // Check if stale
    return isStale(parent);
}

void RemoteFileModel::fetchMore(const QModelIndex &parent)
{
    TreeNode *node = nodeFromIndex(parent);
    if (!node || node->fetching) {
        qCDebug(LogFileOps) << "fetchMore: skipped (node=" << (node ? node->fullPath : "null")
                            << "fetching=" << (node ? node->fetching : false) << ")";
        return;
    }
    if (!coordinator_->hasFtpClient()) {
        qCWarning(LogFileOps) << "fetchMore: no FTP client, cannot fetch" << node->fullPath;
        emit errorOccurred(tr("Cannot list %1: not connected to device").arg(node->fullPath));
        return;
    }

    // If already fetched but stale, clear children first
    if (node->fetched && !node->children.isEmpty()) {
        beginRemoveRows(parent, 0, node->children.count() - 1);
        qDeleteAll(node->children);
        node->children.clear();
        endRemoveRows();
    }

    node->fetched = false;
    node->fetching = true;
    pendingFetches_[node->fullPath] = node;

    emit loadingStarted(node->fullPath);
    if (!coordinator_->requestListing(node->fullPath)) {
        qCWarning(LogFileOps) << "RemoteFileModel: requestListing failed for" << node->fullPath;
    }
}

Qt::ItemFlags RemoteFileModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QString RemoteFileModel::filePath(const QModelIndex &index) const
{
    TreeNode *node = nodeFromIndex(index);
    return node ? node->fullPath : QString();
}

bool RemoteFileModel::isDirectory(const QModelIndex &index) const
{
    TreeNode *node = nodeFromIndex(index);
    return node ? node->isDirectory : false;
}

filetype::FileType RemoteFileModel::fileType(const QModelIndex &index) const
{
    TreeNode *node = nodeFromIndex(index);
    return node ? node->fileType : filetype::FileType::Unknown;
}

qint64 RemoteFileModel::fileSize(const QModelIndex &index) const
{
    TreeNode *node = nodeFromIndex(index);
    return node ? node->size : 0;
}

void RemoteFileModel::refresh()
{
    setRootPath(rootPath_);
}

void RemoteFileModel::refresh(const QModelIndex &index)
{
    TreeNode *node = nodeFromIndex(index);
    if (!node || !node->isDirectory) {
        qCDebug(LogFileOps) << "refresh(index): skipped, node is null or not a directory";
        return;
    }

    // Clear children and refetch
    QModelIndex parentIndex = indexFromNode(node);
    if (!node->children.isEmpty()) {
        // Remove any pending operations for nodes we're about to delete.
        // This prevents dangling pointers in pendingFetches_ if a directory
        // listing arrives after the node is deleted.
        std::function<void(TreeNode *)> cleanupPendingOps = [this,
                                                             &cleanupPendingOps](TreeNode *n) {
            pendingFetches_.remove(n->fullPath);
            coordinator_->cancelPath(n->fullPath);
            for (TreeNode *child : n->children) {
                cleanupPendingOps(child);
            }
        };
        for (TreeNode *child : node->children) {
            cleanupPendingOps(child);
        }

        beginRemoveRows(parentIndex, 0, node->children.count() - 1);
        qDeleteAll(node->children);
        node->children.clear();
        endRemoveRows();
    }

    node->fetched = false;
    node->fetching = false;

    // Trigger refetch
    fetchMore(parentIndex);
}

void RemoteFileModel::clear()
{
    beginResetModel();

    // Cancel pending operations BEFORE deleting nodes to prevent dangling pointer access
    coordinator_->cancelPending();
    pendingFetches_.clear();

    // Clear all children but keep the root node structure
    if (rootNode_) {
        qDeleteAll(rootNode_->children);
        rootNode_->children.clear();
        remotefiletree::markStale(rootNode_->fetched, rootNode_->fetchedAt);
        rootNode_->fetching = false;
    }

    endResetModel();
}

void RemoteFileModel::invalidateCache()
{
    // Recursively mark all nodes as stale
    std::function<void(TreeNode *)> invalidateNode = [&](TreeNode *node) {
        if (node->fetched) {
            remotefiletree::markStale(node->fetched, node->fetchedAt);
        }
        for (TreeNode *child : node->children) {
            invalidateNode(child);
        }
    };

    if (rootNode_) {
        invalidateNode(rootNode_);
    }
}

void RemoteFileModel::invalidatePath(const QString &path)
{
    TreeNode *node = findNodeByPath(path);
    if (node) {
        remotefiletree::markStale(node->fetched, node->fetchedAt);
    }
}

bool RemoteFileModel::isStale(const QModelIndex &index) const
{
    TreeNode *node = nodeFromIndex(index);
    if (!node || !node->isDirectory) {
        return false;
    }
    return remotefiletree::isStale(node->fetched, node->fetchedAt, QDateTime::currentDateTime(),
                                   cacheTtlSeconds_);
}

void RemoteFileModel::refreshIfStale()
{
    if (!coordinator_->hasFtpClient()) {
        qCDebug(LogFileOps) << "refreshIfStale: skipped, FTP client not set";
        return;
    }

    // Check if root is stale and refresh if so
    if (isStale(QModelIndex())) {
        refresh();
    }
}

filetype::FileType RemoteFileModel::detectFileType(const QString &filename)
{
    return filetype::detectFromFilename(filename);
}

QIcon RemoteFileModel::iconForFileType(filetype::FileType type)
{
    QStyle *style = QApplication::style();
    return style->standardIcon(remotefiletree::standardPixmapFor(type));
}

QString RemoteFileModel::fileTypeString(filetype::FileType type)
{
    return filetype::displayName(type);
}

void RemoteFileModel::onListingReady(const QString &path, const QList<FtpEntry> &entries)
{
    qCDebug(LogFileOps) << "Model: onListingReady path:" << path << "entries:" << entries.size();
    qCDebug(LogFileOps) << "Model: pendingFetches_ keys:" << pendingFetches_.keys();

    TreeNode *node = pendingFetches_.take(path);
    if (!node) {
        // The coordinator already filtered out unrequested listings, so reaching
        // here without a node in pendingFetches_ indicates a bug.
        qCWarning(LogFileOps) << "Model: Listing for" << path << "arrived but no pending node found"
                              << "- this indicates a bug. Ignoring to prevent corruption.";
        emit loadingFinished(path);
        return;
    }

    qCDebug(LogFileOps) << "Model: Found pending node:" << node->name
                        << "fullPath:" << node->fullPath;

    // Validate that the node's path matches what we requested
    // This catches dangling pointers or state corruption
    if (node->fullPath != path) {
        qCWarning(LogFileOps) << "Model: Node path mismatch - expected:" << path
                              << "got:" << node->fullPath << "- ignoring to prevent corruption!";
        node->fetching = false;
        emit loadingFinished(path);
        return;
    }

    node->fetching = false;
    remotefiletree::markFetched(node->fetched, node->fetchedAt, QDateTime::currentDateTime());

    populateNode(node, entries);
    emit loadingFinished(path);
}

void RemoteFileModel::onListingFailed(const QString &message)
{
    // Mark any pending fetches as failed
    for (TreeNode *node : pendingFetches_.values()) {
        node->fetching = false;
    }
    pendingFetches_.clear();

    emit errorOccurred(message);
}

RemoteFileModel::TreeNode *RemoteFileModel::nodeFromIndex(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return rootNode_;
    }
    return static_cast<TreeNode *>(index.internalPointer());
}

QModelIndex RemoteFileModel::indexFromNode(TreeNode *node) const
{
    if (!node || node == rootNode_) {
        return QModelIndex();
    }

    TreeNode *parent = node->parent;
    if (!parent) {
        return QModelIndex();
    }

    int row = parent->children.indexOf(node);
    return createIndex(row, 0, node);
}

RemoteFileModel::TreeNode *RemoteFileModel::findNodeByPath(const QString &path) const
{
    if (path == rootPath_ || path == "/") {
        return rootNode_;
    }

    QStringList parts = path.split('/', Qt::SkipEmptyParts);
    TreeNode *current = rootNode_;

    for (const QString &part : parts) {
        bool found = false;
        for (TreeNode *child : current->children) {
            if (child->name == part) {
                current = child;
                found = true;
                break;
            }
        }
        if (!found) {
            return nullptr;
        }
    }

    return current;
}

void RemoteFileModel::populateNode(TreeNode *node, const QList<FtpEntry> &entries)
{
    QModelIndex parentIndex = indexFromNode(node);
    qCDebug(LogFileOps) << "Model: populateNode for" << node->name
                        << "parentIndex valid:" << parentIndex.isValid()
                        << "entries:" << entries.count()
                        << "existing children:" << node->children.count();

    // Defensive check: clear any existing children before populating
    // This prevents model corruption if a listing is processed twice due to edge cases
    if (!node->children.isEmpty()) {
        qCWarning(LogFileOps)
            << "Model: populateNode called on node with existing children - clearing them first";
        beginRemoveRows(parentIndex, 0, node->children.count() - 1);
        qDeleteAll(node->children);
        node->children.clear();
        endRemoveRows();
    }

    if (!entries.isEmpty()) {
        // Sort entries: directories first, then alphabetically by name (case-insensitive)
        QList<FtpEntry> sortedEntries = remotefiletree::sortEntries(entries);

        qCDebug(LogFileOps) << "Model: beginInsertRows parentIndex:" << parentIndex << "rows 0 to"
                            << sortedEntries.count() - 1;
        beginInsertRows(parentIndex, 0, sortedEntries.count() - 1);

        for (const FtpEntry &entry : sortedEntries) {
            auto *child = new TreeNode;
            child->name = entry.name;
            child->isDirectory = entry.isDirectory;
            child->size = entry.size;
            child->parent = node;
            child->fullPath = remotefiletree::childPath(node->fullPath, entry.name);

            // Detect file type
            if (entry.isDirectory) {
                child->fileType = filetype::FileType::Directory;
            } else {
                child->fileType = detectFileType(entry.name);
            }

            node->children.append(child);
            qCDebug(LogFileOps) << "Model: Added child:" << child->name
                                << "isDir:" << child->isDirectory;
        }

        endInsertRows();
        qCDebug(LogFileOps) << "Model: endInsertRows, node now has" << node->children.count()
                            << "children";
    }
}
