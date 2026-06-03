#include "deviceactionservice.h"

#include "services/deviceconnectionmanager.h"
#include "services/irestclient.h"

DeviceActionService::DeviceActionService(DeviceConnectionManager *connection, QObject *parent)
    : QObject(parent), deviceConnection_(connection)
{
}

bool DeviceActionService::canPerformOperations() const
{
    return deviceConnection_ && deviceConnection_->restClient();
}

bool DeviceActionService::ensureCanPerformOperations(const QString &operation)
{
    if (!canPerformOperations()) {
        emit operationNotAvailable(operation);
        return false;
    }
    return true;
}

void DeviceActionService::playSid(const QString &path, int songNumber)
{
    if (!ensureCanPerformOperations(tr("Play SID")))
        return;
    deviceConnection_->restClient()->playSid(path, songNumber);
}

void DeviceActionService::playMod(const QString &path)
{
    if (!ensureCanPerformOperations(tr("Play MOD")))
        return;
    deviceConnection_->restClient()->playMod(path);
}

void DeviceActionService::runPrg(const QString &path)
{
    if (!ensureCanPerformOperations(tr("Run PRG")))
        return;
    deviceConnection_->restClient()->runPrg(path);
}

void DeviceActionService::runCrt(const QString &path)
{
    if (!ensureCanPerformOperations(tr("Run CRT")))
        return;
    deviceConnection_->restClient()->runCrt(path);
}

void DeviceActionService::mountImage(const QString &drive, const QString &path)
{
    if (!ensureCanPerformOperations(tr("Mount image")))
        return;
    deviceConnection_->restClient()->mountImage(drive, path);
}
