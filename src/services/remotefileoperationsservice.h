#ifndef REMOTEFILEOPERATIONSSERVICE_H
#define REMOTEFILEOPERATIONSSERVICE_H

#include "ierroremitter.h"
#include "iftpclient.h"

#include <QSet>
#include <QString>

class RemoteFileOperationsService : public IErrorEmitter
{
    Q_OBJECT

public:
    explicit RemoteFileOperationsService(IFtpClient *ftpClient, QObject *parent = nullptr);

    void createFolder(const QString &path);
    void renameItem(const QString &oldPath, const QString &newPath);

signals:
    void folderCreated(const QString &path);
    void itemRenamed(const QString &oldPath, const QString &newPath);
    void itemRemoved(const QString &path);
    void statusMessage(const QString &message, int timeout = 0);
    void operationFailed(const QString &operation, const QString &error);

private slots:
    void onFtpOperationFailed(IFtpClient::Operation operation, const QString &remotePath,
                              const QString &localPath, const QString &message);

private:
    IFtpClient *ftpClient_ = nullptr;
    /// Paths of this service's requests in flight on the shared client
    QSet<QString> pendingFolders_;
    QSet<QString> pendingRenames_;  ///< Keyed by the old path

    [[nodiscard]] bool ensureFtpClient(const QString &operationLabel);
    void reportFailure(const QString &operationLabel, const QString &message);
};

#endif  // REMOTEFILEOPERATIONSSERVICE_H
