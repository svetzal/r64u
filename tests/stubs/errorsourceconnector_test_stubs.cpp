#include "services/deviceconnection.h"
#include "services/transferservice.h"

// Minimal stub implementations to provide signal metadata for the linker.
// DeviceConnection and TransferService are QSKIP-ed in test_errorsourceconnector
// because they require heavy dependency chains or show blocking dialogs.
// These stubs supply just enough to satisfy link-time symbol resolution.

DeviceConnection::DeviceConnection(QObject *parent) : QObject(parent) {}

DeviceConnection::DeviceConnection(IRestClient * /*restClient*/, IFtpClient * /*ftpClient*/,
                                   QObject *parent)
    : QObject(parent)
{
}

DeviceConnection::~DeviceConnection() = default;

void DeviceConnection::setHost(const QString & /*host*/) {}
void DeviceConnection::setPassword(const QString & /*password*/) {}
void DeviceConnection::setAutoReconnect(bool /*enabled*/) {}
void DeviceConnection::connectToDevice() {}
void DeviceConnection::disconnectFromDevice() {}
void DeviceConnection::refreshDeviceInfo() {}
void DeviceConnection::refreshDriveInfo() {}
void DeviceConnection::onRestInfoReceived(const DeviceInfo & /*info*/) {}
void DeviceConnection::onRestDrivesReceived(const QList<DriveInfo> & /*drives*/) {}
void DeviceConnection::onRestConnectionError(const QString & /*error*/) {}
void DeviceConnection::onRestOperationFailed(const QString & /*operation*/,
                                             const QString & /*error*/)
{
}
void DeviceConnection::onFtpConnected() {}
void DeviceConnection::onFtpDisconnected() {}
void DeviceConnection::onFtpError(const QString & /*message*/) {}
void DeviceConnection::onReconnectTimer() {}
bool DeviceConnection::tryTransitionTo(ConnectionState /*newState*/)
{
    return false;
}
bool DeviceConnection::isValidTransition(ConnectionState /*from*/, ConnectionState /*to*/)
{
    return false;
}
void DeviceConnection::resetProtocolFlags() {}
void DeviceConnection::checkBothConnected() {}
void DeviceConnection::startReconnect() {}
void DeviceConnection::stopReconnect() {}

// ---------------------------------------------------------------------------

TransferService::TransferService(DeviceConnection * /*connection*/, TransferQueue * /*queue*/,
                                 QObject *parent)
    : QObject(parent)
{
}

TransferService::~TransferService() = default;

bool TransferService::uploadFile(const QString & /*localPath*/, const QString & /*remoteDir*/)
{
    return false;
}
bool TransferService::uploadDirectory(const QString & /*localDir*/, const QString & /*remoteDir*/)
{
    return false;
}
bool TransferService::downloadFile(const QString & /*remotePath*/, const QString & /*localDir*/)
{
    return false;
}
bool TransferService::downloadDirectory(const QString & /*remoteDir*/, const QString & /*localDir*/)
{
    return false;
}
bool TransferService::deleteRemote(const QString & /*remotePath*/, bool /*isDirectory*/)
{
    return false;
}
bool TransferService::deleteRecursive(const QString & /*remotePath*/)
{
    return false;
}
void TransferService::cancelAll() {}
void TransferService::cancelBatch(int /*batchId*/) {}
void TransferService::removeCompleted() {}
void TransferService::clear() {}
bool TransferService::isProcessing() const
{
    return false;
}
bool TransferService::isScanning() const
{
    return false;
}
bool TransferService::isProcessingDelete() const
{
    return false;
}
bool TransferService::isCreatingDirectories() const
{
    return false;
}
int TransferService::pendingCount() const
{
    return 0;
}
int TransferService::activeCount() const
{
    return 0;
}
int TransferService::activeAndPendingCount() const
{
    return 0;
}
int TransferService::totalCount() const
{
    return 0;
}
int TransferService::deleteProgress() const
{
    return 0;
}
int TransferService::deleteTotalCount() const
{
    return 0;
}
bool TransferService::isScanningForDelete() const
{
    return false;
}
BatchProgress TransferService::activeBatchProgress() const
{
    return {};
}
BatchProgress TransferService::batchProgress(int /*batchId*/) const
{
    return {};
}
QList<int> TransferService::allBatchIds() const
{
    return {};
}
bool TransferService::hasActiveBatch() const
{
    return false;
}
void TransferService::respondToOverwrite(OverwriteResponse /*response*/) {}
void TransferService::setAutoOverwrite(bool /*autoOverwrite*/) {}
void TransferService::respondToFolderExists(FolderExistsResponse /*response*/) {}
void TransferService::setAutoMerge(bool /*autoMerge*/) {}
