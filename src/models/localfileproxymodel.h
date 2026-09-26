#ifndef LOCALFILEPROXYMODEL_H
#define LOCALFILEPROXYMODEL_H

#include "core/filesortcore.h"
#include "core/filetypecore.h"

#include <QFileSystemModel>
#include <QSortFilterProxyModel>

/**
 * Proxy model that customizes QFileSystemModel display:
 * - Shows file sizes in the app's one human-readable format (filesize::humanSize)
 * - Shows C64-specific file types (SID Music, Program, Disk Image, etc.)
 * - Sorts directories before files, then by the clicked column (Size by raw bytes)
 */
class LocalFileProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit LocalFileProxyModel(QObject *parent = nullptr);

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    /**
     * @brief Dragging a local file is never a move.
     *
     * A view removes the dragged rows from its model when a drag ends as a
     * move, and QFileSystemModel removes rows by deleting the files. Only a
     * copy is offered, so a drag can never delete the user's own files.
     */
    Qt::DropActions supportedDragActions() const override;

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    QFileSystemModel *sourceFileModel() const;
    /// The attributes of the entry at @p nameIndex (a column-0 source index) the filesort rule
    /// uses.
    static filesort::Entry sortableEntry(const QFileSystemModel &fsModel,
                                         const QModelIndex &nameIndex);
    static filetype::FileType detectFileType(const QString &filename);
    static QString fileTypeString(filetype::FileType type);
};

#endif  // LOCALFILEPROXYMODEL_H
