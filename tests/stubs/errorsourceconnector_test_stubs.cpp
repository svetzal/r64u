#include "services/deviceconnectionmanager.h"
#include "services/errorhandler.h"
#include "services/ierroremitter.h"
#include "services/ivideostreamreceiverservice.h"
#include "services/playlistservice.h"
#include "services/transferservice.h"
#include "services/videorecordingservice.h"

// Minimal stub implementations to provide signal metadata for the linker.
// DeviceConnectionManager and TransferService are QSKIP-ed in test_errorsourceconnector
// because they require heavy dependency chains or show blocking dialogs.
// VideoRecordingService is stubbed here to avoid pulling in StreamingService and
// AudioStreamReceiverService/VideoStreamReceiverService chains.
// These stubs supply just enough to satisfy link-time symbol resolution.

class StreamingService;

void ErrorHandler::connectStreamingServiceSources(StreamingService * /*ss*/) {}

VideoRecordingService::VideoRecordingService(QObject *parent) : IErrorEmitter(parent) {}
VideoRecordingService::~VideoRecordingService() = default;
void VideoRecordingService::connectToStreaming(StreamingService * /*manager*/) {}
bool VideoRecordingService::startRecording(const QString & /*filePath*/)
{
    return false;
}
bool VideoRecordingService::stopRecording()
{
    return false;
}
void VideoRecordingService::addFrame(const QImage & /*frame*/) {}
void VideoRecordingService::addAudioSamples(const QByteArray & /*samples*/, int /*sampleCount*/) {}
void VideoRecordingService::onRawFrameReady(const QByteArray & /*frameData*/,
                                            quint16 /*frameNumber*/,
                                            IVideoStreamReceiverService::VideoFormat /*format*/)
{
}

DeviceConnectionManager::DeviceConnectionManager(QObject *parent) : IErrorEmitter(parent) {}

DeviceConnectionManager::DeviceConnectionManager(IRestClient * /*restClient*/,
                                                 IFtpClient * /*ftpClient*/, QObject *parent)
    : IErrorEmitter(parent)
{
}

DeviceConnectionManager::~DeviceConnectionManager() = default;

void DeviceConnectionManager::setHost(const QString & /*host*/) {}
void DeviceConnectionManager::setPassword(const QString & /*password*/) {}
void DeviceConnectionManager::setAutoReconnect(bool /*enabled*/) {}
void DeviceConnectionManager::connectToDevice() {}
void DeviceConnectionManager::disconnectFromDevice() {}
void DeviceConnectionManager::refreshDeviceInfo() {}
void DeviceConnectionManager::refreshDriveInfo() {}
void DeviceConnectionManager::onRestInfoReceived(const DeviceInfo & /*info*/) {}
void DeviceConnectionManager::onRestDrivesReceived(const QList<DriveInfo> & /*drives*/) {}
void DeviceConnectionManager::onRestConnectionError(const QString & /*error*/) {}
void DeviceConnectionManager::onRestOperationFailed(const QString & /*operation*/,
                                                    const QString & /*error*/)
{
}
void DeviceConnectionManager::onFtpConnected() {}
void DeviceConnectionManager::onFtpDisconnected() {}
void DeviceConnectionManager::onFtpError(const QString & /*message*/) {}
void DeviceConnectionManager::onReconnectTimer() {}
bool DeviceConnectionManager::tryTransitionTo(ConnectionState /*newState*/)
{
    return false;
}
bool DeviceConnectionManager::isValidTransition(ConnectionState /*from*/, ConnectionState /*to*/)
{
    return false;
}
void DeviceConnectionManager::resetProtocolFlags() {}
void DeviceConnectionManager::checkBothConnected() {}
void DeviceConnectionManager::startReconnect() {}
void DeviceConnectionManager::stopReconnect() {}

// ---------------------------------------------------------------------------
// PlaylistService stubs — provides Q_OBJECT/signal metadata without the real implementation

PlaylistService::PlaylistService(DeviceConnectionManager * /*connection*/, QObject *parent)
    : IErrorEmitter(parent)
{
}

void PlaylistService::setSonglengthsDatabase(SonglengthsDatabaseService * /*database*/) {}
void PlaylistService::setStreamingService(StreamingService * /*manager*/) {}
void PlaylistService::addItem(const QString & /*path*/, int /*subsong*/) {}
void PlaylistService::addItem(const PlaylistItem & /*item*/) {}
void PlaylistService::removeItem(int /*index*/) {}
void PlaylistService::moveItem(int /*from*/, int /*to*/) {}
void PlaylistService::clear() {}
PlaylistService::PlaylistItem PlaylistService::itemAt(int /*index*/) const
{
    return {};
}
void PlaylistService::play(int /*index*/) {}
void PlaylistService::stop() {}
void PlaylistService::next() {}
void PlaylistService::previous() {}
void PlaylistService::setShuffle(bool /*enabled*/) {}
void PlaylistService::setRepeatMode(RepeatMode /*mode*/) {}
void PlaylistService::setDefaultDuration(int /*seconds*/) {}
void PlaylistService::setItemDuration(int /*index*/, int /*seconds*/) {}
void PlaylistService::updateDurationFromData(const QString & /*path*/,
                                             const QByteArray & /*sidData*/)
{
}
bool PlaylistService::savePlaylist(const QString & /*filePath*/) const
{
    return false;
}
bool PlaylistService::loadPlaylist(const QString & /*filePath*/)
{
    return false;
}
void PlaylistService::saveSettings() const {}
void PlaylistService::loadSettings() {}
void PlaylistService::onAdvanceTimer() {}
void PlaylistService::onSidDataReceived(const QString & /*remotePath*/, const QByteArray & /*data*/)
{
}
void PlaylistService::startTimer() {}
void PlaylistService::stopTimer() {}
void PlaylistService::playCurrentItem() {}
void PlaylistService::ensureStreamingStarted() {}

// ---------------------------------------------------------------------------

TransferService::TransferService(DeviceConnectionManager * /*connection*/,
                                 TransferQueue * /*queue*/, QObject *parent)
    : IErrorEmitter(parent)
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
