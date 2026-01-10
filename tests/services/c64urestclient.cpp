#include "c64urestclient.h"

C64URestClient::C64URestClient(QObject *parent)
    : QObject(parent)
{
}

void C64URestClient::setHost(const QString &host)
{
    host_ = host;
}

void C64URestClient::setPassword(const QString &password)
{
    password_ = password;
}

void C64URestClient::getVersion()
{
    emit versionReceived("1.0");
}

void C64URestClient::getInfo()
{
    DeviceInfo info;
    info.product = "Mock Device";
    emit infoReceived(info);
}

void C64URestClient::playSid(const QString &filePath, int songNumber)
{
    Q_UNUSED(filePath)
    Q_UNUSED(songNumber)
}

void C64URestClient::playMod(const QString &filePath)
{
    Q_UNUSED(filePath)
}

void C64URestClient::loadPrg(const QString &filePath)
{
    Q_UNUSED(filePath)
}

void C64URestClient::runPrg(const QString &filePath)
{
    Q_UNUSED(filePath)
}

void C64URestClient::runCrt(const QString &filePath)
{
    Q_UNUSED(filePath)
}

void C64URestClient::getDrives()
{
    emit drivesReceived(QList<DriveInfo>());
}

void C64URestClient::mountImage(const QString &drive, const QString &imagePath,
                                 const QString &mode)
{
    Q_UNUSED(drive)
    Q_UNUSED(imagePath)
    Q_UNUSED(mode)
}

void C64URestClient::unmountImage(const QString &drive)
{
    Q_UNUSED(drive)
}

void C64URestClient::resetDrive(const QString &drive)
{
    Q_UNUSED(drive)
}

void C64URestClient::resetMachine()
{
}

void C64URestClient::rebootMachine()
{
}

void C64URestClient::pauseMachine()
{
}

void C64URestClient::resumeMachine()
{
}

void C64URestClient::powerOffMachine()
{
}

void C64URestClient::pressMenuButton()
{
}

void C64URestClient::writeMem(const QString &address, const QByteArray &data)
{
    Q_UNUSED(address)
    Q_UNUSED(data)
}

void C64URestClient::typeText(const QString &text)
{
    Q_UNUSED(text)
}

void C64URestClient::getFileInfo(const QString &path)
{
    Q_UNUSED(path)
}

void C64URestClient::createD64(const QString &path, const QString &diskName, int tracks)
{
    Q_UNUSED(path)
    Q_UNUSED(diskName)
    Q_UNUSED(tracks)
}

void C64URestClient::createD81(const QString &path, const QString &diskName)
{
    Q_UNUSED(path)
    Q_UNUSED(diskName)
}

void C64URestClient::getConfigCategories()
{
}

void C64URestClient::getConfigCategoryItems(const QString &category)
{
    Q_UNUSED(category)
}

void C64URestClient::getConfigItem(const QString &category, const QString &item)
{
    Q_UNUSED(category)
    Q_UNUSED(item)
}

void C64URestClient::setConfigItem(const QString &category, const QString &item,
                                    const QVariant &value)
{
    Q_UNUSED(category)
    Q_UNUSED(item)
    Q_UNUSED(value)
}

void C64URestClient::updateConfigsBatch(const QJsonObject &configs)
{
    lastConfigsBatch_ = configs;
    // Don't auto-emit; let test control timing via mockEmitConfigsUpdated()
}

void C64URestClient::saveConfigToFlash()
{
}

void C64URestClient::loadConfigFromFlash()
{
}

void C64URestClient::resetConfigToDefaults()
{
}

// === Mock control methods ===

void C64URestClient::mockEmitConfigsUpdated()
{
    emit configsUpdated();
}

void C64URestClient::mockEmitOperationFailed(const QString &operation, const QString &error)
{
    emit operationFailed(operation, error);
}
