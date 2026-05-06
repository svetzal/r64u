#ifndef LOCALFILEOPERATIONSSERVICE_H
#define LOCALFILEOPERATIONSSERVICE_H

#include "services/errorhandler.h"

#include <QObject>
#include <QStringList>

/**
 * @brief Performs local filesystem operations (create folder, rename, delete).
 *
 * Encapsulates QDir/QFile operations that were previously inline in UI widgets.
 * Emits statusMessage for success confirmations and operationFailed for errors.
 */
class LocalFileOperationsService : public QObject
{
    Q_OBJECT

public:
    explicit LocalFileOperationsService(QObject *parent = nullptr);

    void createFolder(const QString &parentDir, const QString &name);
    void renameItem(const QString &oldPath, const QString &newName);
    void deleteItems(const QStringList &paths);

signals:
    void folderCreated(const QString &path);
    void itemRenamed(const QString &oldPath, const QString &newPath);
    void itemsDeleted(int count);
    void operationFailed(ErrorCategory category, const QString &message);
    void statusMessage(const QString &message, int timeout = 0);
};

#endif  // LOCALFILEOPERATIONSSERVICE_H
