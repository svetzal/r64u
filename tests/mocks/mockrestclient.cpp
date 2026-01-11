#include "mockrestclient.h"

MockRestClient::MockRestClient(QObject *parent)
    : QObject(parent)
{
}

void MockRestClient::getInfo()
{
    getInfoCalls_++;
    // Don't do anything - tests will manually emit signals
}

void MockRestClient::getDrives()
{
    getDrivesCalls_++;
    // Don't do anything - tests will manually emit signals
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

void MockRestClient::mockReset()
{
    getInfoCalls_ = 0;
    getDrivesCalls_ = 0;
}
