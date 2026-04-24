#include "fileactioncontroller.h"

#include "services/configfileloader.h"
#include "services/deviceconnection.h"
#include "services/diskbootsequenceservice.h"
#include "services/filebrowsercore.h"
#include "services/filetypecore.h"
#include "services/playlistmanager.h"
#include "services/streamingmanager.h"
#include "utils/logging.h"

#include <QAction>

FileActionController::FileActionController(DeviceConnection *connection,
                                           ConfigFileLoader *configLoader, QObject *parent)
    : QObject(parent), deviceConnection_(connection), configFileLoader_(configLoader)
{
    bootService_ = new DiskBootSequenceService(this);
    if (deviceConnection_) {
        bootService_->setRestClient(deviceConnection_->restClient());
    }

    connect(bootService_, &DiskBootSequenceService::statusMessage, this,
            &FileActionController::statusMessage);
}

void FileActionController::setStreamingManager(StreamingManager *manager)
{
    streamingManager_ = manager;
}

void FileActionController::setPlaylistManager(PlaylistManager *manager)
{
    playlistManager_ = manager;
}

void FileActionController::setActions(QAction *play, QAction *run, QAction *mount)
{
    playAction_ = play;
    runAction_ = run;
    mountAction_ = mount;
}

void FileActionController::updateActionStates(filetype::FileType type, bool canOperate)
{
    auto caps = filetype::capabilities(type);
    if (playAction_) {
        playAction_->setEnabled(canOperate && caps.canPlay);
    }
    if (runAction_) {
        runAction_->setEnabled(canOperate && caps.canRun);
    }
    if (mountAction_) {
        mountAction_->setEnabled(canOperate && caps.canMount);
    }
}

void FileActionController::play(const QString &path, filetype::FileType type)
{
    if (path.isEmpty() || !deviceConnection_ || !deviceConnection_->restClient()) {
        return;
    }
    ensureStreamingStarted();
    if (type == filetype::FileType::SidMusic) {
        deviceConnection_->restClient()->playSid(path);
        emit statusMessage(tr("Playing SID: %1").arg(path), 3000);
    } else if (type == filetype::FileType::ModMusic) {
        deviceConnection_->restClient()->playMod(path);
        emit statusMessage(tr("Playing MOD: %1").arg(path), 3000);
    }
}

void FileActionController::run(const QString &path, filetype::FileType type)
{
    if (path.isEmpty() || !deviceConnection_ || !deviceConnection_->restClient()) {
        return;
    }
    ensureStreamingStarted();
    if (type == filetype::FileType::Program) {
        deviceConnection_->restClient()->runPrg(path);
        emit statusMessage(tr("Running PRG: %1").arg(path), 3000);
    } else if (type == filetype::FileType::Cartridge) {
        deviceConnection_->restClient()->runCrt(path);
        emit statusMessage(tr("Running CRT: %1").arg(path), 3000);
    } else if (type == filetype::FileType::DiskImage) {
        runDiskImage(path);
    }
}

void FileActionController::mountToDrive(const QString &path, const QString &drive)
{
    if (path.isEmpty() || !deviceConnection_ || !deviceConnection_->restClient()) {
        return;
    }
    deviceConnection_->restClient()->mountImage(drive, path);
    emit statusMessage(tr("Mounting to Drive %1: %2").arg(drive.toUpper()).arg(path), 3000);
}

void FileActionController::loadConfig(const QString &path, filetype::FileType type)
{
    if (path.isEmpty()) {
        return;
    }
    if (type != filetype::FileType::Config) {
        emit statusMessage(tr("Selected file is not a configuration file"), 3000);
        return;
    }
    if (!deviceConnection_ || !deviceConnection_->canPerformOperations()) {
        emit statusMessage(tr("Not connected"), 3000);
        return;
    }
    if (configFileLoader_) {
        configFileLoader_->loadConfigFile(path);
    } else {
        qCDebug(LogUi) << "loadConfig: configFileLoader_ is null, skipping load for" << path;
    }
}

void FileActionController::download(const QString &path)
{
    emit statusMessage(tr("Download requested for: %1").arg(path), 3000);
}

void FileActionController::addToPlaylist(const QList<QPair<QString, filetype::FileType>> &items)
{
    if (!playlistManager_) {
        qCDebug(LogUi) << "addToPlaylist: playlistManager_ is null, skipping";
        return;
    }

    QList<filebrowser::PlaylistCandidate> candidates = filebrowser::filterPlaylistCandidates(items);
    if (candidates.isEmpty()) {
        emit statusMessage(tr("No SID music files in selection"), 3000);
        return;
    }

    for (const auto &candidate : candidates) {
        playlistManager_->addItem(candidate.path);
    }
    emit statusMessage(tr("Added %1 item(s) to playlist").arg(candidates.size()), 3000);
}

void FileActionController::runDiskImage(const QString &path)
{
    bootService_->startBootSequence(path);
}

void FileActionController::ensureStreamingStarted()
{
    if (streamingManager_ != nullptr && !streamingManager_->isStreaming()) {
        streamingManager_->startStreaming();
    }
}
