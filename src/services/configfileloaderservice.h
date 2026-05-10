#ifndef CONFIGFILELOADERSERVICE_H
#define CONFIGFILELOADERSERVICE_H

#include <QJsonObject>
#include <QObject>

class IFtpClient;
class IRestClient;

class ConfigFileLoaderService : public QObject
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
    void onFtpClientDestroyed();
    void onRestClientDestroyed();

private:
    IFtpClient *ftpClient_ = nullptr;
    IRestClient *restClient_ = nullptr;
    QString pendingPath_;
};

#endif  // CONFIGFILELOADERSERVICE_H
