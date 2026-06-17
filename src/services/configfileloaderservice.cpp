#include "configfileloaderservice.h"

#include "iftpclient.h"
#include "irestclient.h"

#include "core/ftpclientmixin.h"
#include "utils/logging.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTextStream>

ConfigFileLoaderService::ConfigFileLoaderService(QObject *parent) : IErrorEmitter(parent)
{
    // Forward loadFailed to the uniform IErrorEmitter signal
    connect(this, &ConfigFileLoaderService::loadFailed, this,
            [this](const QString &path, const QString &error) {
                emit errorReported(ErrorCategory::Validation, ErrorSeverity::Warning,
                                   tr("Loading %1").arg(QFileInfo(path).fileName()), error);
            });
}

ConfigFileLoaderService::~ConfigFileLoaderService()
{
    // Disconnect from clients BEFORE this object is destroyed to prevent
    // signals from being delivered to slots after member variables are destroyed.
    disconnectFtpClient(ftpClient_, this);
    if (restClient_) {
        disconnect(restClient_, nullptr, this, nullptr);
    }
}

void ConfigFileLoaderService::setFtpClient(IFtpClient *client)
{
    disconnectFtpClient(ftpClient_, this);

    ftpClient_ = client;

    if (ftpClient_) {
        connect(ftpClient_, &IFtpClient::downloadToMemoryFinished, this,
                &ConfigFileLoaderService::onDownloadFinished);
        // Track destruction to avoid dangling pointer access
        connect(ftpClient_, &QObject::destroyed, this,
                &ConfigFileLoaderService::onFtpClientDestroyed);
    }
}

void ConfigFileLoaderService::setRestClient(IRestClient *client)
{
    if (restClient_) {
        disconnect(restClient_, nullptr, this, nullptr);
    }

    restClient_ = client;

    if (restClient_) {
        connect(restClient_, &IRestClient::configsUpdated, this,
                &ConfigFileLoaderService::onConfigsUpdated);
        connect(restClient_, &IRestClient::operationFailed, this,
                &ConfigFileLoaderService::onOperationFailed);
        // Track destruction to avoid dangling pointer access
        connect(restClient_, &QObject::destroyed, this,
                &ConfigFileLoaderService::onRestClientDestroyed);
    }
}

void ConfigFileLoaderService::loadConfigFile(const QString &remotePath)
{
    if (!ftpClient_ || !restClient_) {
        qCWarning(LogConfig) << "loadConfigFile skipped: FTP or REST client not configured for"
                             << remotePath;
        emit loadFailed(remotePath, tr("FTP or REST client not configured"));
        return;
    }

    pendingPath_ = remotePath;
    emit loadStarted(remotePath);

    ftpClient_->downloadToMemory(remotePath);
}

QJsonObject ConfigFileLoaderService::parseConfigFile(const QByteArray &data)
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

void ConfigFileLoaderService::onDownloadFinished(const QString &remotePath, const QByteArray &data)
{
    if (remotePath != pendingPath_) {
        qCDebug(LogConfig) << "ConfigFileLoaderService: ignoring download for unrequested path"
                           << remotePath;
        return;
    }

    qCDebug(LogConfig) << "ConfigFileLoaderService: Downloaded" << data.size() << "bytes from"
                       << remotePath;
    qCDebug(LogConfig) << "ConfigFileLoaderService: Raw content:" << QString::fromUtf8(data);

    QJsonObject configs = parseConfigFile(data);

    qCDebug(LogConfig) << "ConfigFileLoaderService: Parsed" << configs.count() << "sections";
    qCDebug(LogConfig) << "ConfigFileLoaderService: JSON to send:"
                       << QJsonDocument(configs).toJson(QJsonDocument::Indented);

    if (configs.isEmpty()) {
        emit loadFailed(pendingPath_, tr("No configuration data found in file"));
        pendingPath_.clear();
        return;
    }

    // Apply configuration via REST API
    restClient_->updateConfigsBatch(configs);
}

void ConfigFileLoaderService::onConfigsUpdated()
{
    if (!pendingPath_.isEmpty()) {
        emit loadFinished(pendingPath_);
        pendingPath_.clear();
    }
}

void ConfigFileLoaderService::onOperationFailed(const QString &operation, const QString &error)
{
    if (operation == "updateConfigs" && !pendingPath_.isEmpty()) {
        emit loadFailed(pendingPath_, error);
        pendingPath_.clear();
    }
}

void ConfigFileLoaderService::onFtpClientDestroyed()
{
    // Client is being destroyed - null our pointer to avoid dangling access
    ftpClient_ = nullptr;
}

void ConfigFileLoaderService::onRestClientDestroyed()
{
    // Client is being destroyed - null our pointer to avoid dangling access
    restClient_ = nullptr;
}
