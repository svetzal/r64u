#include "services/transferservice.h"

// Minimal stubs for symbols referenced by panelcoordinator.cpp.
// These methods are never called in tests: the DeviceConnection is always
// disconnected, so the transferService_ null-check short-circuits before
// any of these methods are reached.

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
