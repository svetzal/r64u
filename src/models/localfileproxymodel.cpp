#include "localfileproxymodel.h"

#include "core/filesizecore.h"
#include "core/filetypecore.h"
#include "ui/pixelicons.h"

LocalFileProxyModel::LocalFileProxyModel(QObject *parent) : QSortFilterProxyModel(parent) {}

QVariant LocalFileProxyModel::data(const QModelIndex &index, int role) const
{
    QFileSystemModel *fsModel = sourceFileModel();
    if (!fsModel) {
        return QSortFilterProxyModel::data(index, role);
    }

    QModelIndex sourceIdx = mapToSource(index);
    QModelIndex nameIdx = sourceIdx.sibling(sourceIdx.row(), 0);

    // Column 0: pixel icon for the entry's C64 file type
    if (index.column() == 0 && role == Qt::DecorationRole) {
        if (fsModel->isDir(nameIdx)) {
            return pixelicons::fileTypeIcon(filetype::FileType::Directory);
        }
        return pixelicons::fileTypeIcon(detectFileType(fsModel->fileName(nameIdx)));
    }

    // Column 1: Size - the same format as the remote list and the details screen
    if (index.column() == 1 && role == Qt::DisplayRole) {
        // Don't show size for directories
        if (fsModel->isDir(nameIdx)) {
            return QVariant();
        }
        return filesize::humanSize(fsModel->size(nameIdx));
    }

    // Column 2: Type - show C64-specific file types
    if (index.column() == 2 && role == Qt::DisplayRole) {
        if (fsModel->isDir(nameIdx)) {
            return tr("Folder");
        }
        QString fileName = fsModel->fileName(nameIdx);
        filetype::FileType type = detectFileType(fileName);
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

    const filesort::Entry leftEntry = sortableEntry(*fsModel, left.sibling(left.row(), 0));
    const filesort::Entry rightEntry = sortableEntry(*fsModel, right.sibling(right.row(), 0));

    // The proxy reverses this comparison for a descending sort, which would put
    // files first; answer so that directories stay ahead either way.
    if (leftEntry.isDirectory != rightEntry.isDirectory) {
        return leftEntry.isDirectory == (sortOrder() == Qt::AscendingOrder);
    }

    // Both are directories or both are files: the shared rule, on the clicked column
    return filesort::lessThan(leftEntry, rightEntry,
                              {filesort::keyForColumn(sortColumn()), Qt::AscendingOrder});
}

QFileSystemModel *LocalFileProxyModel::sourceFileModel() const
{
    return qobject_cast<QFileSystemModel *>(sourceModel());
}

filesort::Entry LocalFileProxyModel::sortableEntry(const QFileSystemModel &fsModel,
                                                   const QModelIndex &nameIndex)
{
    const QString name = fsModel.fileName(nameIndex);
    if (fsModel.isDir(nameIndex)) {
        return {name, true, 0, tr("Folder")};
    }
    return {name, false, fsModel.size(nameIndex), fileTypeString(detectFileType(name))};
}

filetype::FileType LocalFileProxyModel::detectFileType(const QString &filename)
{
    return filetype::detectFromFilename(filename);
}

QString LocalFileProxyModel::fileTypeString(filetype::FileType type)
{
    return filetype::displayName(type);
}
