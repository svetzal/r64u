#include "fileactioncontroller.h"

#include "services/configfileloader.h"
#include "services/deviceconnection.h"
#include "services/diskbootsequenceservice.h"
#include "services/errorhandler.h"
#include "services/filebrowsercore.h"
#include "services/filetypecore.h"
#include "services/playlistservice.h"
#include "services/streamingservice.h"
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
            [this](const QString &message, int /*timeout*/) {
                if (errorHandler_) {
                    errorHandler_->info(ErrorCategory::FileOperation, message);
                }
            });
}

void FileActionController::setErrorHandler(ErrorHandler *handler)
{
    errorHandler_ = handler;
}

void FileActionController::setStreamingService(StreamingService *manager)
{
    streamingService_ = manager;
}

void FileActionController::setPlaylistService(PlaylistService *manager)
{
    playlistService_ = manager;
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
    if (path.isEmpty()) {
        if (errorHandler_) {
            errorHandler_->handleError(ErrorCategory::Validation, ErrorSeverity::Warning,
                                       tr("No file selected"));
        }
        return;
    }
    if (!deviceConnection_ || !deviceConnection_->restClient()) {
        if (errorHandler_) {
            errorHandler_->handleError(ErrorCategory::Connection, ErrorSeverity::Warning,
                                       tr("Not connected to device"));
        }
        return;
    }
    ensureStreamingStarted();
    if (type == filetype::FileType::SidMusic) {
        deviceConnection_->restClient()->playSid(path);
        if (errorHandler_) {
            errorHandler_->info(ErrorCategory::FileOperation, tr("Playing SID: %1").arg(path));
        }
    } else if (type == filetype::FileType::ModMusic) {
        deviceConnection_->restClient()->playMod(path);
        if (errorHandler_) {
            errorHandler_->info(ErrorCategory::FileOperation, tr("Playing MOD: %1").arg(path));
        }
    }
}

void FileActionController::run(const QString &path, filetype::FileType type)
{
    if (path.isEmpty()) {
        if (errorHandler_) {
            errorHandler_->handleError(ErrorCategory::Validation, ErrorSeverity::Warning,
                                       tr("No file selected"));
        }
        return;
    }
    if (!deviceConnection_ || !deviceConnection_->restClient()) {
        if (errorHandler_) {
            errorHandler_->handleError(ErrorCategory::Connection, ErrorSeverity::Warning,
                                       tr("Not connected to device"));
        }
        return;
    }
    ensureStreamingStarted();
    if (type == filetype::FileType::Program) {
        deviceConnection_->restClient()->runPrg(path);
        if (errorHandler_) {
            errorHandler_->info(ErrorCategory::FileOperation, tr("Running PRG: %1").arg(path));
        }
    } else if (type == filetype::FileType::Cartridge) {
        deviceConnection_->restClient()->runCrt(path);
        if (errorHandler_) {
            errorHandler_->info(ErrorCategory::FileOperation, tr("Running CRT: %1").arg(path));
        }
    } else if (type == filetype::FileType::DiskImage) {
        runDiskImage(path);
    }
}

void FileActionController::mountToDrive(const QString &path, const QString &drive)
{
    if (path.isEmpty()) {
        if (errorHandler_) {
            errorHandler_->handleError(ErrorCategory::Validation, ErrorSeverity::Warning,
                                       tr("No file selected"));
        }
        return;
    }
    if (!deviceConnection_ || !deviceConnection_->restClient()) {
        if (errorHandler_) {
            errorHandler_->handleError(ErrorCategory::Connection, ErrorSeverity::Warning,
                                       tr("Not connected to device"));
        }
        return;
    }
    deviceConnection_->restClient()->mountImage(drive, path);
    if (errorHandler_) {
        errorHandler_->info(ErrorCategory::FileOperation,
                            tr("Mounting to Drive %1: %2").arg(drive.toUpper()).arg(path));
    }
}

void FileActionController::loadConfig(const QString &path, filetype::FileType type)
{
    if (path.isEmpty()) {
        if (errorHandler_) {
            errorHandler_->handleError(ErrorCategory::Validation, ErrorSeverity::Warning,
                                       tr("No file selected"));
        }
        return;
    }
    if (type != filetype::FileType::Config) {
        if (errorHandler_) {
            errorHandler_->handleError(ErrorCategory::Validation, ErrorSeverity::Warning,
                                       tr("Selected file is not a configuration file"));
        }
        return;
    }
    if (!deviceConnection_ || !deviceConnection_->canPerformOperations()) {
        if (errorHandler_) {
            errorHandler_->handleError(ErrorCategory::Connection, ErrorSeverity::Warning,
                                       tr("Not connected to device"));
        }
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
    if (errorHandler_) {
        errorHandler_->info(ErrorCategory::FileOperation,
                            tr("Download requested for: %1").arg(path));
    }
}

void FileActionController::addToPlaylist(const QList<QPair<QString, filetype::FileType>> &items)
{
    if (!playlistService_) {
        if (errorHandler_) {
            errorHandler_->handleError(ErrorCategory::Validation, ErrorSeverity::Warning,
                                       tr("Playlist not available"));
        }
        return;
    }

    QList<filebrowser::PlaylistCandidate> candidates = filebrowser::filterPlaylistCandidates(items);
    if (candidates.isEmpty()) {
        if (errorHandler_) {
            errorHandler_->handleError(ErrorCategory::Validation, ErrorSeverity::Warning,
                                       tr("No SID music files in selection"));
        }
        return;
    }

    for (const auto &candidate : candidates) {
        playlistService_->addItem(candidate.path);
    }
    if (errorHandler_) {
        errorHandler_->info(ErrorCategory::FileOperation,
                            tr("Added %1 item(s) to playlist").arg(candidates.size()));
    }
}

void FileActionController::runDiskImage(const QString &path)
{
    bootService_->startBootSequence(path);
}

void FileActionController::ensureStreamingStarted()
{
    if (streamingService_ != nullptr && !streamingService_->isStreaming()) {
        streamingService_->startStreaming();
    }
}
