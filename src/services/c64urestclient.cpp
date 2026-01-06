#include "c64urestclient.h"

#include <QJsonDocument>
#include <QUrlQuery>

C64URestClient::C64URestClient(QObject *parent)
    : QObject(parent)
    , networkManager_(new QNetworkAccessManager(this))
{
    connect(networkManager_, &QNetworkAccessManager::finished,
            this, &C64URestClient::onReplyFinished);
}

C64URestClient::~C64URestClient() = default;

void C64URestClient::setHost(const QString &host)
{
    host_ = host;
    // Remove trailing slash if present
    if (host_.endsWith('/')) {
        host_.chop(1);
    }
    // Add http:// if no scheme present
    if (!host_.startsWith("http://") && !host_.startsWith("https://")) {
        host_ = "http://" + host_;
    }
}

void C64URestClient::setPassword(const QString &password)
{
    password_ = password;
}

QNetworkRequest C64URestClient::createRequest(const QString &endpoint) const
{
    QUrl url(host_ + endpoint);
    QNetworkRequest request(url);

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    if (!password_.isEmpty()) {
        request.setRawHeader("X-Password", password_.toUtf8());
    }

    return request;
}

void C64URestClient::sendGetRequest(const QString &endpoint, const QString &operation)
{
    QNetworkRequest request = createRequest(endpoint);
    QNetworkReply *reply = networkManager_->get(request);
    pendingOperations_[reply] = operation;
}

void C64URestClient::sendPutRequest(const QString &endpoint, const QString &operation,
                                     const QByteArray &data)
{
    QNetworkRequest request = createRequest(endpoint);
    QNetworkReply *reply = networkManager_->put(request, data);
    pendingOperations_[reply] = operation;
}

void C64URestClient::sendPostRequest(const QString &endpoint, const QString &operation,
                                      const QByteArray &data, const QString &contentType)
{
    QNetworkRequest request = createRequest(endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    QNetworkReply *reply = networkManager_->post(request, data);
    pendingOperations_[reply] = operation;
}

// Device information

void C64URestClient::getVersion()
{
    sendGetRequest("/v1/version", "version");
}

void C64URestClient::getInfo()
{
    sendGetRequest("/v1/info", "info");
}

// Runners

void C64URestClient::playSid(const QString &filePath, int songNumber)
{
    QString endpoint = "/v1/runners:sidplay?file=" + QUrl::toPercentEncoding(filePath);
    if (songNumber >= 0) {
        endpoint += "&songnr=" + QString::number(songNumber);
    }
    sendPutRequest(endpoint, "playSid");
}

void C64URestClient::playMod(const QString &filePath)
{
    QString endpoint = "/v1/runners:modplay?file=" + QUrl::toPercentEncoding(filePath);
    sendPutRequest(endpoint, "playMod");
}

void C64URestClient::loadPrg(const QString &filePath)
{
    QString endpoint = "/v1/runners:load_prg?file=" + QUrl::toPercentEncoding(filePath);
    sendPutRequest(endpoint, "loadPrg");
}

void C64URestClient::runPrg(const QString &filePath)
{
    QString endpoint = "/v1/runners:run_prg?file=" + QUrl::toPercentEncoding(filePath);
    sendPutRequest(endpoint, "runPrg");
}

void C64URestClient::runCrt(const QString &filePath)
{
    QString endpoint = "/v1/runners:run_crt?file=" + QUrl::toPercentEncoding(filePath);
    sendPutRequest(endpoint, "runCrt");
}

// Drive control

void C64URestClient::getDrives()
{
    sendGetRequest("/v1/drives", "drives");
}

void C64URestClient::mountImage(const QString &drive, const QString &imagePath,
                                 const QString &mode)
{
    QString endpoint = "/v1/drives/" + drive + ":mount?image=" +
                       QUrl::toPercentEncoding(imagePath);
    if (!mode.isEmpty()) {
        endpoint += "&mode=" + mode;
    }
    sendPutRequest(endpoint, "mount");
}

void C64URestClient::unmountImage(const QString &drive)
{
    QString endpoint = "/v1/drives/" + drive + ":remove";
    sendPutRequest(endpoint, "unmount");
}

void C64URestClient::resetDrive(const QString &drive)
{
    QString endpoint = "/v1/drives/" + drive + ":reset";
    sendPutRequest(endpoint, "resetDrive");
}

// Machine control

void C64URestClient::resetMachine()
{
    sendPutRequest("/v1/machine:reset", "reset");
}

void C64URestClient::rebootMachine()
{
    sendPutRequest("/v1/machine:reboot", "reboot");
}

void C64URestClient::pauseMachine()
{
    sendPutRequest("/v1/machine:pause", "pause");
}

void C64URestClient::resumeMachine()
{
    sendPutRequest("/v1/machine:resume", "resume");
}

void C64URestClient::powerOffMachine()
{
    sendPutRequest("/v1/machine:poweroff", "poweroff");
}

void C64URestClient::pressMenuButton()
{
    sendPutRequest("/v1/machine:menu_button", "menuButton");
}

// File operations

void C64URestClient::getFileInfo(const QString &path)
{
    QString endpoint = "/v1/files/" + QUrl::toPercentEncoding(path, "/") + ":info";
    sendGetRequest(endpoint, "fileInfo");
}

void C64URestClient::createD64(const QString &path, const QString &diskName, int tracks)
{
    QString endpoint = "/v1/files/" + QUrl::toPercentEncoding(path, "/") + ":create_d64";
    endpoint += "?tracks=" + QString::number(tracks);
    if (!diskName.isEmpty()) {
        endpoint += "&diskname=" + QUrl::toPercentEncoding(diskName);
    }
    sendPutRequest(endpoint, "createD64");
}

void C64URestClient::createD81(const QString &path, const QString &diskName)
{
    QString endpoint = "/v1/files/" + QUrl::toPercentEncoding(path, "/") + ":create_d81";
    if (!diskName.isEmpty()) {
        endpoint += "?diskname=" + QUrl::toPercentEncoding(diskName);
    }
    sendPutRequest(endpoint, "createD81");
}

// Configuration management

void C64URestClient::updateConfigsBatch(const QJsonObject &configs)
{
    QJsonDocument doc(configs);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    sendPostRequest("/v1/configs", "updateConfigs", data, "application/json");
}

// Response handling

void C64URestClient::onReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();

    QString operation = pendingOperations_.take(reply);
    if (operation.isEmpty()) {
        return;
    }

    if (reply->error() == QNetworkReply::ConnectionRefusedError ||
        reply->error() == QNetworkReply::HostNotFoundError ||
        reply->error() == QNetworkReply::TimeoutError) {
        emit connectionError(reply->errorString());
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        // Try to read the response body for more detailed error info
        QByteArray errorData = reply->readAll();
        QString errorMsg = reply->errorString();
        if (!errorData.isEmpty()) {
            QJsonDocument errorDoc = QJsonDocument::fromJson(errorData);
            if (errorDoc.isObject()) {
                QJsonObject errorJson = errorDoc.object();
                QStringList errors = extractErrors(errorJson);
                if (!errors.isEmpty()) {
                    errorMsg = errors.join("; ");
                }
            } else {
                // Not JSON, include raw response
                errorMsg += " - Response: " + QString::fromUtf8(errorData).left(200);
            }
        }
        qDebug() << "REST error for" << operation << ":" << errorMsg;
        emit operationFailed(operation, errorMsg);
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (!doc.isObject()) {
        emit operationFailed(operation, "Invalid JSON response");
        return;
    }

    QJsonObject json = doc.object();
    QStringList errors = extractErrors(json);

    if (!errors.isEmpty()) {
        emit operationFailed(operation, errors.join("; "));
        return;
    }

    // Route to specific handler
    if (operation == "version") {
        handleVersionResponse(json);
    } else if (operation == "info") {
        handleInfoResponse(json);
    } else if (operation == "drives") {
        handleDrivesResponse(json);
    } else if (operation == "fileInfo") {
        handleFileInfoResponse(json);
    } else if (operation == "updateConfigs") {
        emit configsUpdated();
        emit operationSucceeded(operation);
    } else {
        handleGenericResponse(operation, json);
    }
}

void C64URestClient::handleVersionResponse(const QJsonObject &json)
{
    QString version = json["version"].toString();
    emit versionReceived(version);
}

void C64URestClient::handleInfoResponse(const QJsonObject &json)
{
    DeviceInfo info;
    info.product = json["product"].toString();
    info.firmwareVersion = json["firmware_version"].toString();
    info.fpgaVersion = json["fpga_version"].toString();
    info.coreVersion = json["core_version"].toString();
    info.hostname = json["hostname"].toString();
    info.uniqueId = json["unique_id"].toString();
    emit infoReceived(info);
}

void C64URestClient::handleDrivesResponse(const QJsonObject &json)
{
    QList<DriveInfo> drives;
    QJsonArray drivesArray = json["drives"].toArray();

    for (const QJsonValue &driveVal : drivesArray) {
        QJsonObject driveObj = driveVal.toObject();
        // Each drive is an object with a single key (drive name)
        for (const QString &driveName : driveObj.keys()) {
            QJsonObject driveData = driveObj[driveName].toObject();
            DriveInfo drive;
            drive.name = driveName;
            drive.enabled = driveData["enabled"].toBool();
            drive.busId = driveData["bus_id"].toInt();
            drive.type = driveData["type"].toString();
            drive.rom = driveData["rom"].toString();
            drive.imageFile = driveData["image_file"].toString();
            drive.imagePath = driveData["image_path"].toString();
            drive.lastError = driveData["last_error"].toString();
            drives.append(drive);
        }
    }

    emit drivesReceived(drives);
}

void C64URestClient::handleFileInfoResponse(const QJsonObject &json)
{
    QJsonObject files = json["files"].toObject();
    QString path = files["path"].toString();
    qint64 size = files["size"].toInteger();
    QString extension = files["extension"].toString();
    emit fileInfoReceived(path, size, extension);
}

void C64URestClient::handleGenericResponse(const QString &operation, const QJsonObject &json)
{
    Q_UNUSED(json)
    emit operationSucceeded(operation);
}

QStringList C64URestClient::extractErrors(const QJsonObject &json) const
{
    QStringList errors;
    QJsonArray errorsArray = json["errors"].toArray();
    for (const QJsonValue &error : errorsArray) {
        QString errorStr = error.toString();
        if (!errorStr.isEmpty()) {
            errors.append(errorStr);
        }
    }
    return errors;
}
