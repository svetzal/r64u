#ifndef C64URESTCLIENT_H
#define C64URESTCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>

struct DeviceInfo {
    QString product;
    QString firmwareVersion;
    QString fpgaVersion;
    QString coreVersion;
    QString hostname;
    QString uniqueId;
    QString apiVersion;
};

struct DriveInfo {
    QString name;
    bool enabled = false;
    int busId = 0;
    QString type;
    QString rom;
    QString imageFile;
    QString imagePath;
    QString lastError;
};

class C64URestClient : public QObject
{
    Q_OBJECT

public:
    explicit C64URestClient(QObject *parent = nullptr);
    ~C64URestClient() override;

    void setHost(const QString &host);
    QString host() const { return host_; }

    void setPassword(const QString &password);
    bool hasPassword() const { return !password_.isEmpty(); }

    // Device information
    void getVersion();
    void getInfo();

    // Runners - play/run content
    void playSid(const QString &filePath, int songNumber = -1);
    void playMod(const QString &filePath);
    void loadPrg(const QString &filePath);
    void runPrg(const QString &filePath);
    void runCrt(const QString &filePath);

    // Drive control
    void getDrives();
    void mountImage(const QString &drive, const QString &imagePath,
                    const QString &mode = "readwrite");
    void unmountImage(const QString &drive);
    void resetDrive(const QString &drive);

    // Machine control
    void resetMachine();
    void rebootMachine();
    void pauseMachine();
    void resumeMachine();
    void powerOffMachine();
    void pressMenuButton();

    // File operations
    void getFileInfo(const QString &path);
    void createD64(const QString &path, const QString &diskName = QString(),
                   int tracks = 35);
    void createD81(const QString &path, const QString &diskName = QString());

signals:
    void versionReceived(const QString &version);
    void infoReceived(const DeviceInfo &info);
    void drivesReceived(const QList<DriveInfo> &drives);
    void fileInfoReceived(const QString &path, qint64 size, const QString &extension);

    void operationSucceeded(const QString &operation);
    void operationFailed(const QString &operation, const QString &error);

    void connectionError(const QString &error);

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    QNetworkRequest createRequest(const QString &endpoint) const;
    void sendGetRequest(const QString &endpoint, const QString &operation);
    void sendPutRequest(const QString &endpoint, const QString &operation,
                        const QByteArray &data = QByteArray());
    void sendPostRequest(const QString &endpoint, const QString &operation,
                         const QByteArray &data,
                         const QString &contentType = "application/octet-stream");

    void handleVersionResponse(const QJsonObject &json);
    void handleInfoResponse(const QJsonObject &json);
    void handleDrivesResponse(const QJsonObject &json);
    void handleFileInfoResponse(const QJsonObject &json);
    void handleGenericResponse(const QString &operation, const QJsonObject &json);

    QStringList extractErrors(const QJsonObject &json) const;

    QNetworkAccessManager *networkManager_ = nullptr;
    QString host_;
    QString password_;

    // Map reply to operation name for response handling
    QHash<QNetworkReply*, QString> pendingOperations_;
};

#endif // C64URESTCLIENT_H
