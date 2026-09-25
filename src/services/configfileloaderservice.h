#ifndef CONFIGFILELOADERSERVICE_H
#define CONFIGFILELOADERSERVICE_H

#include "ierroremitter.h"
#include "iftpclient.h"

#include <QJsonObject>

class IRestClient;

class ConfigFileLoaderService : public IErrorEmitter
{
    Q_OBJECT

public:
    explicit ConfigFileLoaderService(QObject *parent = nullptr);
    ~ConfigFileLoaderService() override;

    void setFtpClient(IFtpClient *client);
    void setRestClient(IRestClient *client);

    void loadConfigFile(const QString &remotePath);

    static QJsonObject parseConfigFile(const QByteArray &data);

signals:
    void loadStarted(const QString &path);
    void loadFinished(const QString &path);
    void loadFailed(const QString &path, const QString &error);

private slots:
    void onDownloadFinished(const QString &remotePath, const QByteArray &data);
    void onConfigsUpdated();
    void onOperationFailed(const QString &operation, const QString &error);
    void onFtpOperationFailed(IFtpClient::Operation operation, const QString &remotePath,
                              const QString &localPath, const QString &message);
    void onFtpClientDestroyed();
    void onRestClientDestroyed();

private:
    IFtpClient *ftpClient_ = nullptr;
    IRestClient *restClient_ = nullptr;
    QString pendingPath_;
};

#endif  // CONFIGFILELOADERSERVICE_H
