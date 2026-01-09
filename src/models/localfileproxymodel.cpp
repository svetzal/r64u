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

QFileSystemModel *LocalFileProxyModel::sourceFileModel() const
{
    return qobject_cast<QFileSystemModel*>(sourceModel());
}
