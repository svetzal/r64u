#include "configfileloader.h"
#include "c64uftpclient.h"
#include "c64urestclient.h"

#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
#include <QJsonDocument>

ConfigFileLoader::ConfigFileLoader(QObject *parent)
    : QObject(parent)
{
}

void ConfigFileLoader::setFtpClient(C64UFtpClient *client)
{
    if (ftpClient_) {
        disconnect(ftpClient_, nullptr, this, nullptr);
    }

    ftpClient_ = client;

    if (ftpClient_) {
        connect(ftpClient_, &C64UFtpClient::downloadToMemoryFinished,
                this, &ConfigFileLoader::onDownloadFinished);
    }
}

void ConfigFileLoader::setRestClient(C64URestClient *client)
{
    if (restClient_) {
        disconnect(restClient_, nullptr, this, nullptr);
    }

    restClient_ = client;

    if (restClient_) {
        connect(restClient_, &C64URestClient::configsUpdated,
                this, &ConfigFileLoader::onConfigsUpdated);
        connect(restClient_, &C64URestClient::operationFailed,
                this, &ConfigFileLoader::onOperationFailed);
    }
}

void ConfigFileLoader::loadConfigFile(const QString &remotePath)
{
    if (!ftpClient_ || !restClient_) {
        emit loadFailed(remotePath, tr("FTP or REST client not configured"));
        return;
    }

    pendingPath_ = remotePath;
    emit loadStarted(remotePath);

    ftpClient_->downloadToMemory(remotePath);
}

QJsonObject ConfigFileLoader::parseConfigFile(const QByteArray &data)
{
    QJsonObject result;
    QString content = QString::fromUtf8(data);
    QTextStream stream(&content);

    QString currentSection;
    QJsonObject currentSectionObj;

    // Regex to match section headers like [Section Name]
    QRegularExpression sectionRx("^\\[(.+)\\]\\s*$");
    // Regex to match key=value pairs
    QRegularExpression kvRx("^([^=]+)=(.*)$");

    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();

        // Skip empty lines and comments
        if (line.isEmpty() || line.startsWith('#') || line.startsWith(';')) {
            continue;
        }

        // Check for section header
        QRegularExpressionMatch sectionMatch = sectionRx.match(line);
        if (sectionMatch.hasMatch()) {
            // Save previous section if exists
            if (!currentSection.isEmpty() && !currentSectionObj.isEmpty()) {
                result[currentSection] = currentSectionObj;
            }
            currentSection = sectionMatch.captured(1);
            currentSectionObj = QJsonObject();
            continue;
        }

        // Check for key=value pair
        QRegularExpressionMatch kvMatch = kvRx.match(line);
        if (kvMatch.hasMatch() && !currentSection.isEmpty()) {
            QString key = kvMatch.captured(1).trimmed();
            // Only right-trim the value - preserve leading spaces as they're
            // significant for the API (e.g., " 0 dB" vs "0 dB")
            QString value = kvMatch.captured(2);
            while (value.endsWith(' ') || value.endsWith('\t')) {
                value.chop(1);
            }

            // Try to convert to appropriate JSON type
            bool ok;
            int intVal = value.trimmed().toInt(&ok);
            if (ok && value.trimmed() == value) {
                // Only use int if there's no significant whitespace
                currentSectionObj[key] = intVal;
            } else {
                // Keep as string (preserving leading space)
                currentSectionObj[key] = value;
            }
        }
    }

    // Save last section
    if (!currentSection.isEmpty() && !currentSectionObj.isEmpty()) {
        result[currentSection] = currentSectionObj;
    }

    return result;
}

void ConfigFileLoader::onDownloadFinished(const QString &remotePath, const QByteArray &data)
{
    if (remotePath != pendingPath_) {
        return;
    }

    qDebug() << "ConfigFileLoader: Downloaded" << data.size() << "bytes from" << remotePath;
    qDebug() << "ConfigFileLoader: Raw content:" << QString::fromUtf8(data);

    QJsonObject configs = parseConfigFile(data);

    qDebug() << "ConfigFileLoader: Parsed" << configs.count() << "sections";
    qDebug() << "ConfigFileLoader: JSON to send:" << QJsonDocument(configs).toJson(QJsonDocument::Indented);

    if (configs.isEmpty()) {
        emit loadFailed(pendingPath_, tr("No configuration data found in file"));
        pendingPath_.clear();
        return;
    }

    // Apply configuration via REST API
    restClient_->updateConfigsBatch(configs);
}

void ConfigFileLoader::onConfigsUpdated()
{
    if (!pendingPath_.isEmpty()) {
        emit loadFinished(pendingPath_);
        pendingPath_.clear();
    }
}

void ConfigFileLoader::onOperationFailed(const QString &operation, const QString &error)
{
    if (operation == "updateConfigs" && !pendingPath_.isEmpty()) {
        emit loadFailed(pendingPath_, error);
        pendingPath_.clear();
    }
}
