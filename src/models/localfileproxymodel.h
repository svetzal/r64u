#ifndef LOCALFILEPROXYMODEL_H
#define LOCALFILEPROXYMODEL_H

#include <QSortFilterProxyModel>
#include <QFileSystemModel>

/**
 * Proxy model that displays file sizes in bytes instead of human-readable format.
 * QFileSystemModel by default shows "1.2 KB" etc., this proxy shows "1234" bytes.
 */
class LocalFileProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit LocalFileProxyModel(QObject *parent = nullptr);

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    QFileSystemModel *sourceFileModel() const;
};

#endif // LOCALFILEPROXYMODEL_H
