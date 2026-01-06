#ifndef CONFIGFILELOADER_H
#define CONFIGFILELOADER_H

#include <QObject>
#include <QJsonObject>

class C64UFtpClient;
class C64URestClient;

class ConfigFileLoader : public QObject
{
    Q_OBJECT

public:
    explicit ConfigFileLoader(QObject *parent = nullptr);

    void setFtpClient(C64UFtpClient *client);
    void setRestClient(C64URestClient *client);

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

private:
    C64UFtpClient *ftpClient_ = nullptr;
    C64URestClient *restClient_ = nullptr;
    QString pendingPath_;
};

#endif // CONFIGFILELOADER_H
