#include "remotefilebrowsercontroller.h"

#include "core/filebrowsercore.h"

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
    return filebrowser::buildDeleteConfirmMessage(paths, isDirectory, QStringLiteral("delete"));
}
