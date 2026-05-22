#ifndef SERVICEFACTORY_H
#define SERVICEFACTORY_H

#include "metadataservicebundle.h"

#include <QObject>

class IErrorPresenter;
class DeviceConnectionManager;
class RemoteFileModel;
class TransferQueue;
class ConfigFileLoaderService;
class FilePreviewService;
class TransferService;
class ErrorHandler;
class StatusMessageService;
class FavoritesService;
class PlaylistService;
class HttpFileDownloader;
class SonglengthsDatabase;
class HVSCMetadataService;
class GameBase64Service;
class ConfigurationService;
class DeviceActionService;
class RemoteFileOperationsService;
class SystemCommandController;

/**
 * @brief Creates and owns all application services.
 *
 * Instantiated in MainWindow's constructor. All returned pointers are
 * non-owning — the factory (and its Qt parent chain) manages lifetimes.
 */
class ServiceFactory : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs and wires all services.
     * @param parent Qt object parent for this factory.
     */
    explicit ServiceFactory(QObject *parent = nullptr);

    /**
     * @brief Sets the error presenter used to show dialogs.
     *
     * Call this after construction to wire a UI-layer presenter into the
     * ErrorHandler. The presenter is not owned by the factory.
     * @param presenter Presenter to use (not owned, may be null).
     */
    void setErrorPresenter(IErrorPresenter *presenter);

    /// @brief Returns the device connection service.
    [[nodiscard]] DeviceConnectionManager *deviceConnection() const;
    /// @brief Returns the remote file model.
    [[nodiscard]] RemoteFileModel *remoteFileModel() const;
    /// @brief Returns the transfer queue.
    [[nodiscard]] TransferQueue *transferQueue() const;
    /// @brief Returns the config file loader service.
    [[nodiscard]] ConfigFileLoaderService *configFileLoader() const;
    /// @brief Returns the file preview service.
    [[nodiscard]] FilePreviewService *filePreviewService() const;
    /// @brief Returns the transfer service.
    [[nodiscard]] TransferService *transferService() const;
    /// @brief Returns the error handler.
    [[nodiscard]] ErrorHandler *errorHandler() const;
    /// @brief Returns the status message service.
    [[nodiscard]] StatusMessageService *statusMessageService() const;
    /// @brief Returns the favorites service.
    [[nodiscard]] FavoritesService *favoritesService() const;
    /// @brief Returns the playlist service.
    [[nodiscard]] PlaylistService *playlistService() const;
    /// @brief Returns the HTTP downloader used for HVSC song lengths.
    [[nodiscard]] HttpFileDownloader *songlengthsDownloader() const;
    /// @brief Returns the song lengths database.
    [[nodiscard]] SonglengthsDatabase *songlengthsDatabase() const;
    /// @brief Returns the HTTP downloader used for the HVSC STIL file.
    [[nodiscard]] HttpFileDownloader *stilDownloader() const;
    /// @brief Returns the HTTP downloader used for the HVSC bug list.
    [[nodiscard]] HttpFileDownloader *buglistDownloader() const;
    /// @brief Returns the HVSC metadata service.
    [[nodiscard]] HVSCMetadataService *hvscMetadataService() const;
    /// @brief Returns the HTTP downloader used for GameBase64 data.
    [[nodiscard]] HttpFileDownloader *gameBase64Downloader() const;
    /// @brief Returns the GameBase64 service.
    [[nodiscard]] GameBase64Service *gameBase64Service() const;

    /// @brief Returns the configuration service.
    [[nodiscard]] ConfigurationService *configurationService() const;
    /// @brief Returns the device action service (play/run/mount).
    [[nodiscard]] DeviceActionService *deviceActionService() const;
    /// @brief Returns the remote file operations service.
    [[nodiscard]] RemoteFileOperationsService *remoteFileOperations() const;
    /// @brief Returns the system command controller.
    [[nodiscard]] SystemCommandController *systemCommandController() const;

    /// @brief Returns all metadata download services bundled together.
    [[nodiscard]] MetadataServiceBundle metadataBundle() const;

private:
    DeviceConnectionManager *deviceConnection_ = nullptr;
    RemoteFileModel *remoteFileModel_ = nullptr;
    TransferQueue *transferQueue_ = nullptr;
    ConfigFileLoaderService *configFileLoader_ = nullptr;
    FilePreviewService *filePreviewService_ = nullptr;
    TransferService *transferService_ = nullptr;
    ErrorHandler *errorHandler_ = nullptr;
    StatusMessageService *statusMessageService_ = nullptr;
    FavoritesService *favoritesService_ = nullptr;
    PlaylistService *playlistService_ = nullptr;
    HttpFileDownloader *songlengthsDownloader_ = nullptr;
    SonglengthsDatabase *songlengthsDatabase_ = nullptr;
    HttpFileDownloader *stilDownloader_ = nullptr;
    HttpFileDownloader *buglistDownloader_ = nullptr;
    HVSCMetadataService *hvscMetadataService_ = nullptr;
    HttpFileDownloader *gameBase64Downloader_ = nullptr;
    GameBase64Service *gameBase64Service_ = nullptr;
    ConfigurationService *configurationService_ = nullptr;
    DeviceActionService *deviceActionService_ = nullptr;
    RemoteFileOperationsService *remoteFileOperations_ = nullptr;
    SystemCommandController *systemCommandController_ = nullptr;
};

#endif  // SERVICEFACTORY_H
