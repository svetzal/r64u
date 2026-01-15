#include "localfileproxymodel.h"

LocalFileProxyModel::LocalFileProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

QVariant LocalFileProxyModel::data(const QModelIndex &index, int role) const
{
    QFileSystemModel *fsModel = sourceFileModel();
    if (!fsModel) {
        return QSortFilterProxyModel::data(index, role);
    }

    QModelIndex sourceIdx = mapToSource(index);
    QModelIndex nameIdx = sourceIdx.sibling(sourceIdx.row(), 0);

    // Column 1: Size - show bytes instead of human-readable format
    if (index.column() == 1 && role == Qt::DisplayRole) {
        // Don't show size for directories
        if (fsModel->isDir(nameIdx)) {
            return QVariant();
        }
        qint64 size = fsModel->size(nameIdx);
        return QString::number(size);
    }

    // Column 2: Type - show C64-specific file types
    if (index.column() == 2 && role == Qt::DisplayRole) {
        if (fsModel->isDir(nameIdx)) {
            return tr("Folder");
        }
        QString fileName = fsModel->fileName(nameIdx);
        FileType type = detectFileType(fileName);
        return fileTypeString(type);
    }

    return QSortFilterProxyModel::data(index, role);
}

QVariant LocalFileProxyModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    // Override Type column header
    if (orientation == Qt::Horizontal && section == 2 && role == Qt::DisplayRole) {
        return tr("Type");
    }
    return QSortFilterProxyModel::headerData(section, orientation, role);
}

bool LocalFileProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    QFileSystemModel *fsModel = sourceFileModel();
    if (!fsModel) {
        return QSortFilterProxyModel::lessThan(left, right);
    }

    // Get column 0 indices for directory check
    QModelIndex leftName = left.sibling(left.row(), 0);
    QModelIndex rightName = right.sibling(right.row(), 0);

    bool leftIsDir = fsModel->isDir(leftName);
    bool rightIsDir = fsModel->isDir(rightName);

    // Directories come before files
    if (leftIsDir && !rightIsDir) {
        return sortOrder() == Qt::AscendingOrder;
    }
    if (!leftIsDir && rightIsDir) {
        return sortOrder() != Qt::AscendingOrder;
    }

    // Both are directories or both are files - use default sorting
    return QSortFilterProxyModel::lessThan(left, right);
}

QFileSystemModel *LocalFileProxyModel::sourceFileModel() const
{
    return qobject_cast<QFileSystemModel*>(sourceModel());
}

LocalFileProxyModel::FileType LocalFileProxyModel::detectFileType(const QString &filename)
{
    QString ext = filename.section('.', -1).toLower();

    if (ext == "sid" || ext == "psid" || ext == "rsid") {
        return FileType::SidMusic;
    }
    if (ext == "mod" || ext == "xm" || ext == "s3m" || ext == "it") {
        return FileType::ModMusic;
    }
    if (ext == "prg" || ext == "p00") {
        return FileType::Program;
    }
    if (ext == "crt") {
        return FileType::Cartridge;
    }
    if (ext == "d64" || ext == "d71" || ext == "d81" || ext == "g64" || ext == "g71") {
        return FileType::DiskImage;
    }
    if (ext == "tap" || ext == "t64") {
        return FileType::TapeImage;
    }
    if (ext == "rom" || ext == "bin") {
        return FileType::Rom;
    }
    if (ext == "cfg") {
        return FileType::Config;
    }

    return FileType::Unknown;
}

QString LocalFileProxyModel::fileTypeString(FileType type)
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
