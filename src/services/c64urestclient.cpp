#include "c64urestclient.h"

#include "restresponsecore.h"

#include <QJsonDocument>
#include <QTimer>
#include <QUrlQuery>

C64URestClient::C64URestClient(QObject *parent)
    : IRestClient(parent), networkManager_(new QNetworkAccessManager(this))
{
    connect(networkManager_, &QNetworkAccessManager::finished, this,
            &C64URestClient::onReplyFinished);
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

    // Set request timeout
    request.setTransferTimeout(RequestTimeoutMs);

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

void C64URestClient::mountImage(const QString &drive, const QString &imagePath, const QString &mode)
{
    QString endpoint = "/v1/drives/" + drive + ":mount?image=" + QUrl::toPercentEncoding(imagePath);
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

void C64URestClient::writeMem(const QString &address, const QByteArray &data)
{
    // Convert data to hex string
    QString hexData;
    for (char byte : data) {
        hexData += QString("%1").arg(static_cast<quint8>(byte), 2, 16, QChar('0'));
    }

    QString endpoint = QString("/v1/machine:writemem?address=%1&data=%2").arg(address, hexData);
    sendPutRequest(endpoint, "writeMem");
}

void C64URestClient::typeText(const QString &text)
{
    // C64 keyboard buffer is at $0277 (10 bytes max)
    // Character count is at $C6
    // For text longer than 10 chars, we need to send in chunks with delays
    constexpr int KeyboardBufferSize = 10;

    if (text.isEmpty()) {
        return;
    }

    // Convert ASCII to PETSCII (simple conversion for printable chars)
    auto toPetscii = [](const QString &str) -> QByteArray {
        QByteArray petscii;
        for (QChar c : str) {
            char ch = c.toLatin1();
            // Convert lowercase to uppercase (PETSCII uses uppercase)
            if (ch >= 'a' && ch <= 'z') {
                ch = ch - 'a' + 'A';
            }
            // Convert newline to RETURN (CHR$(13))
            else if (ch == '\n') {
                ch = 13;
            }
            petscii.append(ch);
        }
        return petscii;
    };

    // If text fits in buffer, send it directly
    if (text.length() <= KeyboardBufferSize) {
        QByteArray petscii = toPetscii(text);
        writeMem("0277", petscii);
        QByteArray count;
        count.append(static_cast<char>(petscii.size()));
        writeMem("c6", count);
        return;
    }

    // For longer text, send first chunk now, schedule rest with delays
    QString remaining = text;
    QString chunk = remaining.left(KeyboardBufferSize);
    remaining = remaining.mid(KeyboardBufferSize);

    QByteArray petscii = toPetscii(chunk);
    writeMem("0277", petscii);
    QByteArray count;
    count.append(static_cast<char>(petscii.size()));
    writeMem("c6", count);

    // Schedule remaining chunks with 200ms delays to let C64 consume buffer
    if (!remaining.isEmpty()) {
        QTimer::singleShot(200, this, [this, remaining]() { typeText(remaining); });
    }
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

void C64URestClient::getConfigCategories()
{
    sendGetRequest("/v1/configs", "configCategories");
}

void C64URestClient::getConfigCategoryItems(const QString &category)
{
    // Use wildcard to get full item details including available options
    QString endpoint = "/v1/configs/" + QUrl::toPercentEncoding(category) + "/*";
    // Encode category in operation name for response routing
    sendGetRequest(endpoint, "configCategoryItems:" + category);
}

void C64URestClient::getConfigItem(const QString &category, const QString &item)
{
    QString endpoint =
        "/v1/configs/" + QUrl::toPercentEncoding(category) + "/" + QUrl::toPercentEncoding(item);
    // Encode category:item in operation name for response routing
    sendGetRequest(endpoint, "getConfigItem:" + category + ":" + item);
}

void C64URestClient::setConfigItem(const QString &category, const QString &item,
                                   const QVariant &value)
{
    QString endpoint = "/v1/configs/" + QUrl::toPercentEncoding(category) + "/" +
                       QUrl::toPercentEncoding(item) +
                       "?value=" + QUrl::toPercentEncoding(value.toString());
    // Encode category:item in operation name for response routing
    sendPutRequest(endpoint, "setConfigItem:" + category + ":" + item);
}

void C64URestClient::updateConfigsBatch(const QJsonObject &configs)
{
    QJsonDocument doc(configs);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    sendPostRequest("/v1/configs", "updateConfigs", data, "application/json");
}

void C64URestClient::saveConfigToFlash()
{
    sendPutRequest("/v1/configs:save_to_flash", "saveConfigToFlash");
}

void C64URestClient::loadConfigFromFlash()
{
    sendPutRequest("/v1/configs:load_from_flash", "loadConfigFromFlash");
}

void C64URestClient::resetConfigToDefaults()
{
    sendPutRequest("/v1/configs:reset_to_default", "resetConfigToDefaults");
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
                QStringList errors = restresponse::extractErrors(errorJson);
                if (!errors.isEmpty()) {
                    errorMsg = errors.join("; ");
                }
            } else {
                // Not JSON, include raw response
                errorMsg +=
                    " - Response: " + QString::fromUtf8(errorData).left(ErrorResponsePreviewLength);
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
    QStringList errors = restresponse::extractErrors(json);

    if (!errors.isEmpty()) {
        emit operationFailed(operation, errors.join("; "));
        return;
    }

    // Route to specific handler
    if (operation == "version") {
        emit versionReceived(restresponse::parseVersionResponse(json));
    } else if (operation == "info") {
        emit infoReceived(restresponse::parseInfoResponse(json));
    } else if (operation == "drives") {
        emit drivesReceived(restresponse::parseDrivesResponse(json));
    } else if (operation == "fileInfo") {
        auto fi = restresponse::parseFileInfoResponse(json);
        emit fileInfoReceived(fi.path, fi.size, fi.extension);
    } else if (operation == "configCategories") {
        emit configCategoriesReceived(restresponse::parseConfigCategoriesResponse(json));
    } else if (operation.startsWith("configCategoryItems:")) {
        QString category = operation.mid(20);  // Length of "configCategoryItems:"
        emit configCategoryItemsReceived(
            category, restresponse::parseConfigCategoryItemsResponse(category, json));
    } else if (operation.startsWith("getConfigItem:")) {
        // Parse category:item from operation name
        QString catItem = operation.mid(14);  // Length of "getConfigItem:"
        int colonPos = catItem.indexOf(':');
        if (colonPos > 0) {
            QString category = catItem.left(colonPos);
            QString item = catItem.mid(colonPos + 1);
            // Parse value from response - expects {"category": {"item": value}}
            QJsonObject categoryObj = json[category].toObject();
            QVariant value = categoryObj[item].toVariant();
            emit configItemReceived(category, item, value);
        }
        emit operationSucceeded(operation);
    } else if (operation.startsWith("setConfigItem:")) {
        // Parse category:item from operation name
        QString catItem = operation.mid(14);  // Length of "setConfigItem:"
        int colonPos = catItem.indexOf(':');
        if (colonPos > 0) {
            QString category = catItem.left(colonPos);
            QString item = catItem.mid(colonPos + 1);
            emit configItemSet(category, item);
        }
        emit operationSucceeded(operation);
    } else if (operation == "updateConfigs") {
        emit configsUpdated();
        emit operationSucceeded(operation);
    } else if (operation == "saveConfigToFlash") {
        emit configSavedToFlash();
        emit operationSucceeded(operation);
    } else if (operation == "loadConfigFromFlash") {
        emit configLoadedFromFlash();
        emit operationSucceeded(operation);
    } else if (operation == "resetConfigToDefaults") {
        emit configResetToDefaults();
        emit operationSucceeded(operation);
    } else {
        handleGenericResponse(operation, json);
    }
}

void C64URestClient::handleGenericResponse(const QString &operation, const QJsonObject &json)
{
    Q_UNUSED(json)
    emit operationSucceeded(operation);
}
