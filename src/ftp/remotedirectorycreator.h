#ifndef REMOTEDIRECTORYCREATOR_H
#define REMOTEDIRECTORYCREATOR_H

#include "services/transfercore.h"

#include <QObject>
#include <QString>

class IFtpClient;
class ILocalFileSystem;

/**
 * @brief Coordinator for creating remote directory hierarchies before uploads.
 *
 * Operates on the shared transfer::State and emits signals when directory
 * creation is complete or when each directory is successfully created.
 *
 * Local file system queries (enumerating subdirectories) are routed through
 * ILocalFileSystem, keeping this coordinator independently testable.
 */
class RemoteDirectoryCreator : public QObject
{
    Q_OBJECT

public:
    explicit RemoteDirectoryCreator(transfer::State &state, IFtpClient *ftpClient,
                                    ILocalFileSystem *localFs, QObject *parent = nullptr);

    void setFtpClient(IFtpClient *client);
    void setLocalFileSystem(ILocalFileSystem *fs);

    /// Queue all directories for a local→remote upload
    void queueDirectoriesForUpload(const QString &localDir, const QString &remoteDir);

    /// Issue the next makeDirectory call (or finish if queue is empty)
    void createNextDirectory();

    /// Called by TransferQueue when the FTP client reports a directory was created
    void onDirectoryCreated(const QString &path);

signals:
    void directoryCreationProgress(int created, int total);
    void allDirectoriesCreated();
    void error(const QString &message);

private:
    transfer::State &state_;
    IFtpClient *ftpClient_ = nullptr;
    ILocalFileSystem *localFs_ = nullptr;
};

#endif  // REMOTEDIRECTORYCREATOR_H
