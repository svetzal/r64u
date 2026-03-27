#include "localfileproxymodel.h"

#include "services/filetypecore.h"

LocalFileProxyModel::LocalFileProxyModel(QObject *parent) : QSortFilterProxyModel(parent) {}

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

    // Both are directories or both are files - use case-insensitive sorting
    QString leftFileName = fsModel->fileName(leftName);
    QString rightFileName = fsModel->fileName(rightName);
    return leftFileName.compare(rightFileName, Qt::CaseInsensitive) < 0;
}

QFileSystemModel *LocalFileProxyModel::sourceFileModel() const
{
    return qobject_cast<QFileSystemModel *>(sourceModel());
}

LocalFileProxyModel::FileType LocalFileProxyModel::detectFileType(const QString &filename)
{
    return static_cast<FileType>(filetype::detectFromFilename(filename));
}

QString LocalFileProxyModel::fileTypeString(FileType type)
{
    return filetype::displayName(static_cast<filetype::FileType>(type));
}
