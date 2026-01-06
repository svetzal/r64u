#include "remotefilemodel.h"
#include <QStyle>
#include <QApplication>
#include <QDebug>

RemoteFileModel::RemoteFileModel(QObject *parent)
    : QAbstractItemModel(parent)
    , rootNode_(new TreeNode)
{
    rootNode_->name = "/";
    rootNode_->fullPath = "/";
    rootNode_->isDirectory = true;
    rootNode_->fileType = FileType::Directory;
}

RemoteFileModel::~RemoteFileModel()
{
    delete rootNode_;
}

void RemoteFileModel::setFtpClient(C64UFtpClient *client)
{
    if (ftpClient_) {
        disconnect(ftpClient_, nullptr, this, nullptr);
    }

    ftpClient_ = client;

    if (ftpClient_) {
        connect(ftpClient_, &C64UFtpClient::directoryListed,
                this, &RemoteFileModel::onDirectoryListed);
        connect(ftpClient_, &C64UFtpClient::error,
                this, &RemoteFileModel::onFtpError);
    }
}

void RemoteFileModel::setRootPath(const QString &path)
{
    beginResetModel();

    rootPath_ = path;
    delete rootNode_;
    rootNode_ = new TreeNode;
    rootNode_->name = path;
    rootNode_->fullPath = path;
    rootNode_->isDirectory = true;
    rootNode_->fileType = FileType::Directory;
    pendingFetches_.clear();

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
    return 3; // Name, Size, Type
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
        case 0: return node->name;
        case 1: return node->isDirectory ? QVariant() : QString::number(node->size);
        case 2: return fileTypeString(node->fileType);
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
    }

    return QVariant();
}

QVariant RemoteFileModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QVariant();
    }

    switch (section) {
    case 0: return tr("Name");
    case 1: return tr("Size");
    case 2: return tr("Type");
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

    return node->isDirectory && !node->fetched && !node->fetching;
}

void RemoteFileModel::fetchMore(const QModelIndex &parent)
{
    TreeNode *node = nodeFromIndex(parent);
    if (!node || !ftpClient_ || node->fetching || node->fetched) {
        return;
    }

    node->fetching = true;
    pendingFetches_[node->fullPath] = node;

    emit loadingStarted(node->fullPath);
    ftpClient_->list(node->fullPath);
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

RemoteFileModel::FileType RemoteFileModel::fileType(const QModelIndex &index) const
{
    TreeNode *node = nodeFromIndex(index);
    return node ? node->fileType : FileType::Unknown;
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
        return;
    }

    // Clear children and refetch
    QModelIndex parentIndex = indexFromNode(node);
    if (!node->children.isEmpty()) {
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

RemoteFileModel::FileType RemoteFileModel::detectFileType(const QString &filename)
{
    QString ext = filename.section('.', -1).toLower();

    if (ext == "sid" || ext == "psid" || ext == "rsid") {
        return FileType::SidMusic;
    } else if (ext == "mod" || ext == "xm" || ext == "s3m" || ext == "it") {
        return FileType::ModMusic;
    } else if (ext == "prg" || ext == "p00") {
        return FileType::Program;
    } else if (ext == "crt") {
        return FileType::Cartridge;
    } else if (ext == "d64" || ext == "d71" || ext == "d81" || ext == "g64" || ext == "g71") {
        return FileType::DiskImage;
    } else if (ext == "tap" || ext == "t64") {
        return FileType::TapeImage;
    } else if (ext == "rom" || ext == "bin") {
        return FileType::Rom;
    } else if (ext == "cfg") {
        return FileType::Config;
    }

    return FileType::Unknown;
}

QIcon RemoteFileModel::iconForFileType(FileType type)
{
    // Use standard icons as placeholders - can be replaced with custom icons
    QStyle *style = QApplication::style();

    switch (type) {
    case FileType::Directory:
        return style->standardIcon(QStyle::SP_DirIcon);
    case FileType::SidMusic:
    case FileType::ModMusic:
        return style->standardIcon(QStyle::SP_MediaVolume);
    case FileType::Program:
        return style->standardIcon(QStyle::SP_FileIcon);
    case FileType::Cartridge:
        return style->standardIcon(QStyle::SP_DriveHDIcon);
    case FileType::DiskImage:
        return style->standardIcon(QStyle::SP_DriveFDIcon);
    case FileType::TapeImage:
        return style->standardIcon(QStyle::SP_DriveCDIcon);
    case FileType::Rom:
        return style->standardIcon(QStyle::SP_FileDialogDetailedView);
    case FileType::Config:
        return style->standardIcon(QStyle::SP_FileDialogInfoView);
    default:
        return style->standardIcon(QStyle::SP_FileIcon);
    }
}

QString RemoteFileModel::fileTypeString(FileType type)
{
    switch (type) {
    case FileType::Directory: return tr("Folder");
    case FileType::SidMusic: return tr("SID Music");
    case FileType::ModMusic: return tr("MOD Music");
    case FileType::Program: return tr("Program");
    case FileType::Cartridge: return tr("Cartridge");
    case FileType::DiskImage: return tr("Disk Image");
    case FileType::TapeImage: return tr("Tape Image");
    case FileType::Rom: return tr("ROM");
    case FileType::Config: return tr("Configuration");
    default: return tr("File");
    }
}

void RemoteFileModel::onDirectoryListed(const QString &path, const QList<FtpEntry> &entries)
{
    qDebug() << "Model: onDirectoryListed path:" << path << "entries:" << entries.size();
    qDebug() << "Model: pendingFetches_ keys:" << pendingFetches_.keys();

    TreeNode *node = pendingFetches_.take(path);
    if (!node) {
        // Might be for root
        if (path == rootPath_ || path.isEmpty()) {
            qDebug() << "Model: Using root node for path:" << path;
            node = rootNode_;
        } else {
            qDebug() << "Model: No node found for path:" << path << "- ignoring!";
            return;
        }
    } else {
        qDebug() << "Model: Found pending node:" << node->name << "fullPath:" << node->fullPath;
    }

    node->fetching = false;
    node->fetched = true;

    populateNode(node, entries);
    emit loadingFinished(path);
}

void RemoteFileModel::onFtpError(const QString &message)
{
    // Mark any pending fetches as failed
    for (TreeNode *node : pendingFetches_.values()) {
        node->fetching = false;
    }
    pendingFetches_.clear();

    emit errorOccurred(message);
}

RemoteFileModel::TreeNode* RemoteFileModel::nodeFromIndex(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return rootNode_;
    }
    return static_cast<TreeNode*>(index.internalPointer());
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

RemoteFileModel::TreeNode* RemoteFileModel::findNodeByPath(const QString &path) const
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
    qDebug() << "Model: populateNode for" << node->name << "parentIndex valid:" << parentIndex.isValid()
             << "entries:" << entries.count() << "existing children:" << node->children.count();

    if (!entries.isEmpty()) {
        qDebug() << "Model: beginInsertRows parentIndex:" << parentIndex << "rows 0 to" << entries.count() - 1;
        beginInsertRows(parentIndex, 0, entries.count() - 1);

        for (const FtpEntry &entry : entries) {
            TreeNode *child = new TreeNode;
            child->name = entry.name;
            child->isDirectory = entry.isDirectory;
            child->size = entry.size;
            child->parent = node;

            // Build full path
            QString parentPath = node->fullPath;
            if (!parentPath.endsWith('/')) {
                parentPath += '/';
            }
            child->fullPath = parentPath + entry.name;

            // Detect file type
            if (entry.isDirectory) {
                child->fileType = FileType::Directory;
            } else {
                child->fileType = detectFileType(entry.name);
            }

            node->children.append(child);
            qDebug() << "Model: Added child:" << child->name << "isDir:" << child->isDirectory;
        }

        endInsertRows();
        qDebug() << "Model: endInsertRows, node now has" << node->children.count() << "children";
    }
}
