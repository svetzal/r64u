#ifndef LOCALFILEPROXYMODEL_H
#define LOCALFILEPROXYMODEL_H

#include "services/filetypecore.h"

#include <QFileSystemModel>
#include <QSortFilterProxyModel>

/**
 * Proxy model that customizes QFileSystemModel display:
 * - Shows file sizes in bytes instead of human-readable format
 * - Shows C64-specific file types (SID Music, Program, Disk Image, etc.)
 * - Sorts directories before files
 */
class LocalFileProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit LocalFileProxyModel(QObject *parent = nullptr);

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    QFileSystemModel *sourceFileModel() const;
    static filetype::FileType detectFileType(const QString &filename);
    static QString fileTypeString(filetype::FileType type);
};

#endif  // LOCALFILEPROXYMODEL_H
