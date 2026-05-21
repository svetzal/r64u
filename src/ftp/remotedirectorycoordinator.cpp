/**
 * @file remotedirectorycoordinator.cpp
 * @brief Implementation of RemoteDirectoryCoordinator.
 */

#include "remotedirectorycoordinator.h"

#include "services/iftpclient.h"
#include "services/ilocalfilesystem.h"
#include "utils/logging.h"

RemoteDirectoryCoordinator::RemoteDirectoryCoordinator(transfer::State &state,
                                                       IFtpClient *ftpClient,
                                                       ILocalFileSystem *localFs, QObject *parent)
    : QObject(parent), state_(state), ftpClient_(ftpClient), localFs_(localFs)
{
}

void RemoteDirectoryCoordinator::setFtpClient(IFtpClient *client)
{
    ftpClient_ = client;
}

void RemoteDirectoryCoordinator::setLocalFileSystem(ILocalFileSystem *fs)
{
    localFs_ = fs;
}

void RemoteDirectoryCoordinator::queueDirectoriesForUpload(const QString &localDir,
                                                           const QString &remoteDir)
{
    // Queue root directory
    transfer::PendingMkdir rootMkdir;
    rootMkdir.remotePath = remoteDir;
    rootMkdir.localDir = localDir;
    rootMkdir.remoteBase = remoteDir;
    state_.pendingMkdirs.enqueue(rootMkdir);

    // Queue all subdirectories via gateway
    if (!localFs_) {
        qCWarning(LogTransfer) << "RemoteDirectoryCoordinator: localFs_ is null, cannot enumerate"
                               << localDir;
    }
    if (localFs_) {
        const QStringList subdirs = localFs_->listSubdirectoriesRecursively(localDir);
        for (const QString &subDir : subdirs) {
            const QString relativePath = localFs_->relativePath(localDir, subDir);
            const QString remotePath = remoteDir + '/' + relativePath;

            transfer::PendingMkdir mkdir;
            mkdir.remotePath = remotePath;
            mkdir.localDir = subDir;
            mkdir.remoteBase = remoteDir;
            state_.pendingMkdirs.enqueue(mkdir);
        }
    }

    state_.directoriesCreated = 0;
    state_.totalDirectoriesToCreate = state_.pendingMkdirs.size();
    emit directoryCreationProgress(0, state_.totalDirectoriesToCreate);
}

void RemoteDirectoryCoordinator::createNextDirectory()
{
    if (state_.pendingMkdirs.isEmpty()) {
        qCDebug(LogTransfer) << "RemoteDirectoryCoordinator: All directories created";
        emit allDirectoriesCreated();
        return;
    }

    transfer::PendingMkdir mkdir = state_.pendingMkdirs.head();
    if (!ftpClient_) {
        qCWarning(LogTransfer) << "RemoteDirectoryCoordinator: ftpClient_ is null, cannot create"
                               << mkdir.remotePath;
        emit error(tr("Cannot create directory: not connected"));
        return;
    }
    ftpClient_->makeDirectory(mkdir.remotePath);
}

void RemoteDirectoryCoordinator::onDirectoryCreated(const QString &path)
{
    qCDebug(LogTransfer) << "RemoteDirectoryCoordinator: onDirectoryCreated:" << path;

    if (state_.queueState != transfer::QueueState::CreatingDirectories) {
        qCWarning(LogTransfer)
            << "RemoteDirectoryCoordinator::onDirectoryCreated: unexpected state,"
            << "ignoring" << path;
        return;
    }

    if (!state_.pendingMkdirs.isEmpty()) {
        state_.pendingMkdirs.dequeue();
        state_.directoriesCreated++;

        emit directoryCreationProgress(state_.directoriesCreated, state_.totalDirectoriesToCreate);

        if (state_.pendingMkdirs.isEmpty()) {
            qCDebug(LogTransfer) << "RemoteDirectoryCoordinator: All directories created";
            emit allDirectoriesCreated();
        } else {
            createNextDirectory();
        }
    }
}
