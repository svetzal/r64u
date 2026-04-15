#ifndef REMOTEFILEOPERATIONS_H
#define REMOTEFILEOPERATIONS_H

#include <QObject>
#include <QString>

class IFtpClient;

class RemoteFileOperations : public QObject
{
    Q_OBJECT

public:
    explicit RemoteFileOperations(IFtpClient *ftpClient, QObject *parent = nullptr);

    void createFolder(const QString &path);
    void renameItem(const QString &oldPath, const QString &newPath);

signals:
    void folderCreated(const QString &path);
    void itemRenamed(const QString &oldPath, const QString &newPath);
    void itemRemoved(const QString &path);
    void statusMessage(const QString &message, int timeout = 0);

private:
    IFtpClient *ftpClient_ = nullptr;
};

#endif  // REMOTEFILEOPERATIONS_H
