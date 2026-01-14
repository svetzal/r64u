#include "localfileproxymodel.h"

LocalFileProxyModel::LocalFileProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

QVariant LocalFileProxyModel::data(const QModelIndex &index, int role) const
{
    // For size column (column 1), show bytes instead of human-readable format
    if (index.column() == 1 && role == Qt::DisplayRole) {
        QFileSystemModel *fsModel = sourceFileModel();
        if (fsModel) {
            QModelIndex sourceIdx = mapToSource(index);
            // Get the source index for column 0 to access file info
            QModelIndex nameIdx = sourceIdx.sibling(sourceIdx.row(), 0);

            // Don't show size for directories
            if (fsModel->isDir(nameIdx)) {
                return QVariant();
            }

            qint64 size = fsModel->size(nameIdx);
            return QString::number(size);
        }
    }

    return QSortFilterProxyModel::data(index, role);
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
