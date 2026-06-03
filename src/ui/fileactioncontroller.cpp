#include "fileactioncontroller.h"

#include "core/filebrowsercore.h"
#include "core/filetypecore.h"
#include "models/remotefilemodel.h"
#include "services/configfileloaderservice.h"
#include "services/deviceactionservice.h"
#include "services/deviceconnectionmanager.h"
#include "services/diskbootsequenceservice.h"
#include "services/errorhandler.h"
#include "services/irestclient.h"
#include "services/playlistservice.h"
#include "services/streamingservice.h"
#include "utils/logging.h"

#include <QAction>
#include <QTreeView>

FileActionController::FileActionController(DeviceActionService *deviceActionService,
                                           DeviceConnectionManager *connection,
                                           ConfigFileLoaderService *configLoader,
                                           ErrorHandler *errorHandler, QObject *parent)
    : QObject(parent), deviceActionService_(deviceActionService), deviceConnection_(connection),
      configFileLoader_(configLoader), errorHandler_(errorHandler)
{
    Q_ASSERT(errorHandler_ && "ErrorHandler is required");
    bootService_ = new DiskBootSequenceService(this);
    if (deviceConnection_) {
        bootService_->setRestClient(deviceConnection_->restClient());
    }

    connect(bootService_, &DiskBootSequenceService::statusMessage, this,
            [this](const QString &message, int /*timeout*/) {
                errorHandler_->info(ErrorCategory::FileOperation, message);
            });
    connect(bootService_, &DiskBootSequenceService::errorOccurred, errorHandler_,
            &ErrorHandler::handleDataError);
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

void FileActionController::setSelectionSource(QTreeView *view, RemoteFileModel *model)
{
    selectionView_ = view;
    selectionModel_ = model;
}

bool FileActionController::hasSelectionSource() const
{
    return selectionView_ != nullptr && selectionModel_ != nullptr;
}

QString FileActionController::selectionPath() const
{
    if (!hasSelectionSource()) {
        return {};
    }
    QModelIndex index = selectionView_->currentIndex();
    if (index.isValid()) {
        return selectionModel_->filePath(index);
    }
    return {};
}

filetype::FileType FileActionController::selectionFileType() const
{
    if (!hasSelectionSource()) {
        return filetype::FileType::Unknown;
    }
    QModelIndex index = selectionView_->currentIndex();
    if (index.isValid()) {
        return selectionModel_->fileType(index);
    }
    return filetype::FileType::Unknown;
}

void FileActionController::playSelection()
{
    if (!hasSelectionSource()) {
        emit statusMessage(tr("File browser not ready"));
        return;
    }
    play(selectionPath(), selectionFileType());
}

void FileActionController::runSelection()
{
    if (!hasSelectionSource()) {
        emit statusMessage(tr("File browser not ready"));
        return;
    }
    run(selectionPath(), selectionFileType());
}

void FileActionController::loadConfigSelection()
{
    if (!hasSelectionSource()) {
        emit statusMessage(tr("File browser not ready"));
        return;
    }
    loadConfig(selectionPath(), selectionFileType());
}

void FileActionController::mountToDriveSelection(const QString &drive)
{
    if (!hasSelectionSource()) {
        emit statusMessage(tr("File browser not ready"));
        return;
    }
    mountToDrive(selectionPath(), drive);
}

void FileActionController::downloadSelection()
{
    if (!hasSelectionSource()) {
        emit statusMessage(tr("File browser not ready"));
        return;
    }
    download(selectionPath());
}

void FileActionController::addToPlaylistSelection()
{
    if (!hasSelectionSource()) {
        emit statusMessage(tr("File browser not ready"));
        return;
    }

    QModelIndexList selectedIndices = selectionView_->selectionModel()->selectedRows();
    if (selectedIndices.isEmpty()) {
        emit statusMessage(tr("No files selected"));
        return;
    }

    QList<QPair<QString, filetype::FileType>> items;
    items.reserve(selectedIndices.size());
    for (const QModelIndex &idx : selectedIndices) {
        items.append({selectionModel_->filePath(idx), selectionModel_->fileType(idx)});
    }
    addToPlaylist(items);
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
    if (!validateFileOperation(path))
        return;
    ensureStreamingStarted();
    if (type == filetype::FileType::SidMusic) {
        deviceActionService_->playSid(path);
        errorHandler_->info(ErrorCategory::FileOperation, tr("Playing SID: %1").arg(path));
    } else if (type == filetype::FileType::ModMusic) {
        deviceActionService_->playMod(path);
        errorHandler_->info(ErrorCategory::FileOperation, tr("Playing MOD: %1").arg(path));
    }
}

void FileActionController::run(const QString &path, filetype::FileType type)
{
    if (!validateFileOperation(path))
        return;
    ensureStreamingStarted();
    if (type == filetype::FileType::Program) {
        deviceActionService_->runPrg(path);
        errorHandler_->info(ErrorCategory::FileOperation, tr("Running PRG: %1").arg(path));
    } else if (type == filetype::FileType::Cartridge) {
        deviceActionService_->runCrt(path);
        errorHandler_->info(ErrorCategory::FileOperation, tr("Running CRT: %1").arg(path));
    } else if (type == filetype::FileType::DiskImage) {
        runDiskImage(path);
    }
}

void FileActionController::mountToDrive(const QString &path, const QString &drive)
{
    if (!validateFileOperation(path))
        return;
    deviceActionService_->mountImage(drive, path);
    errorHandler_->info(ErrorCategory::FileOperation,
                        tr("Mounting to Drive %1: %2").arg(drive.toUpper()).arg(path));
}

void FileActionController::loadConfig(const QString &path, filetype::FileType type)
{
    if (path.isEmpty()) {
        errorHandler_->handleError(ErrorCategory::Validation, ErrorSeverity::Warning,
                                   tr("No file selected"));
        return;
    }
    if (type != filetype::FileType::Config) {
        errorHandler_->handleError(ErrorCategory::Validation, ErrorSeverity::Warning,
                                   tr("Selected file is not a configuration file"));
        return;
    }
    if (!deviceConnection_ || !deviceConnection_->canPerformOperations()) {
        errorHandler_->handleError(ErrorCategory::Connection, ErrorSeverity::Warning,
                                   tr("Not connected to device"));
        return;
    }
    if (configFileLoader_) {
        configFileLoader_->loadConfigFile(path);
    } else {
        qCWarning(LogUi) << "loadConfig: configFileLoader_ is null, cannot load" << path;
        emit statusMessage(tr("Configuration loader not available"));
    }
}

void FileActionController::download(const QString &path)
{
    errorHandler_->info(ErrorCategory::FileOperation, tr("Download requested for: %1").arg(path));
}

void FileActionController::addToPlaylist(const QList<QPair<QString, filetype::FileType>> &items)
{
    if (!playlistService_) {
        errorHandler_->handleError(ErrorCategory::Validation, ErrorSeverity::Warning,
                                   tr("Playlist not available"));
        return;
    }

    QList<filebrowser::PlaylistCandidate> candidates = filebrowser::filterPlaylistCandidates(items);
    if (candidates.isEmpty()) {
        errorHandler_->handleError(ErrorCategory::Validation, ErrorSeverity::Warning,
                                   tr("No SID music files in selection"));
        return;
    }

    for (const auto &candidate : candidates) {
        playlistService_->addItem(candidate.path);
    }
    errorHandler_->info(ErrorCategory::FileOperation,
                        tr("Added %1 item(s) to playlist").arg(candidates.size()));
}

bool FileActionController::validateFileOperation(const QString &path)
{
    if (path.isEmpty()) {
        errorHandler_->handleError(ErrorCategory::Validation, ErrorSeverity::Warning,
                                   tr("No file selected"));
        return false;
    }
    if (!deviceActionService_ || !deviceActionService_->canPerformOperations()) {
        errorHandler_->handleError(ErrorCategory::Connection, ErrorSeverity::Warning,
                                   tr("Not connected to device"));
        return false;
    }
    return true;
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
