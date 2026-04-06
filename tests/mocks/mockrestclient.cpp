#include "mockrestclient.h"

MockRestClient::MockRestClient(QObject *parent) : IRestClient(parent) {}

void MockRestClient::getInfo()
{
    getInfoCalls_++;
    // Don't auto-emit - tests control timing via mockEmitInfoReceived()
}

void MockRestClient::getDrives()
{
    getDrivesCalls_++;
    // Don't auto-emit
}

void MockRestClient::writeMem(const QString &address, const QByteArray &data)
{
    writeMemCalls_++;
    lastWriteMemAddress_ = address;
    lastWriteMemData_ = data;
}

void MockRestClient::typeText(const QString &text)
{
    typeTextCalls_++;
    lastTypeText_ = text;
}

void MockRestClient::updateConfigsBatch(const QJsonObject &configs)
{
    lastConfigsBatch_ = configs;
    // Don't auto-emit; let test control timing via mockEmitConfigsUpdated()
}

void MockRestClient::mockEmitInfoReceived(const DeviceInfo &info)
{
    emit infoReceived(info);
}

void MockRestClient::mockEmitDrivesReceived(const QList<DriveInfo> &drives)
{
    emit drivesReceived(drives);
}

void MockRestClient::mockEmitConnectionError(const QString &error)
{
    emit connectionError(error);
}

void MockRestClient::mockEmitOperationFailed(const QString &operation, const QString &error)
{
    emit operationFailed(operation, error);
}

void MockRestClient::mockEmitConfigsUpdated()
{
    emit configsUpdated();
}

void MockRestClient::mockReset()
{
    getInfoCalls_ = 0;
    getDrivesCalls_ = 0;
    writeMemCalls_ = 0;
    typeTextCalls_ = 0;
    resetMachineCalls_ = 0;
    rebootMachineCalls_ = 0;
    pauseMachineCalls_ = 0;
    resumeMachineCalls_ = 0;
    powerOffMachineCalls_ = 0;
    pressMenuButtonCalls_ = 0;
    unmountImageCalls_ = 0;
    lastWriteMemAddress_.clear();
    lastWriteMemData_.clear();
    lastTypeText_.clear();
    lastConfigsBatch_ = QJsonObject();
    lastUnmountDrive_.clear();
}
