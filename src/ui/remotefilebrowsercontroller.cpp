#include "remotefilebrowsercontroller.h"

#include <QFileInfo>

RemoteFileBrowserController::RemoteFileBrowserController(QObject *parent) : QObject(parent) {}

QString RemoteFileBrowserController::buildNewFolderPath(const QString &currentDirectory,
                                                        const QString &folderName) const
{
    if (folderName.isEmpty()) {
        return {};
    }

    QString remoteDir = currentDirectory.isEmpty() ? QStringLiteral("/") : currentDirectory;
    if (!remoteDir.endsWith('/')) {
        remoteDir += '/';
    }
    return remoteDir + folderName;
}

QString RemoteFileBrowserController::buildRenamePath(const QString &oldPath,
                                                     const QString &newName) const
{
    if (newName.isEmpty() || !isValidName(newName)) {
        return {};
    }

    QFileInfo fileInfo(oldPath);
    QString parentPath = fileInfo.path();
    if (!parentPath.endsWith('/')) {
        parentPath += '/';
    }
    return parentPath + newName;
}

bool RemoteFileBrowserController::isValidName(const QString &name)
{
    return !name.contains('/') && !name.contains('\\');
}

QString RemoteFileBrowserController::buildDeleteConfirmMessage(const QStringList &paths,
                                                               bool isDirectory) const
{
    if (paths.size() == 1) {
        QString fileName = QFileInfo(paths.first()).fileName();
        if (isDirectory) {
            return tr("Are you sure you want to permanently delete the folder '%1' and all its "
                      "contents?\n\nThis cannot be undone.")
                .arg(fileName);
        }
        return tr("Are you sure you want to permanently delete '%1'?\n\nThis cannot be undone.")
            .arg(fileName);
    }
    return tr("Are you sure you want to permanently delete %1 items?\n\nThis cannot be undone.")
        .arg(paths.size());
}
