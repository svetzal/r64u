#ifndef REMOTEFILEOPERATIONSSERVICE_H
#define REMOTEFILEOPERATIONSSERVICE_H

#include <QObject>
#include <QString>

class IFtpClient;

class RemoteFileOperationsService : public QObject
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

private:
    IFtpClient *ftpClient_ = nullptr;
    [[nodiscard]] bool ensureFtpClient(const QString &operationLabel);
};

#endif  // REMOTEFILEOPERATIONSSERVICE_H
