#ifndef SERVICEFACTORY_H
#define SERVICEFACTORY_H

#include "metadataservicebundle.h"

#include <QObject>
#include <QWidget>

class DeviceConnection;
class RemoteFileModel;
class TransferQueue;
class ConfigFileLoader;
class FilePreviewService;
class TransferService;
class ErrorHandler;
class StatusMessageService;
class FavoritesManager;
class PlaylistManager;
class HttpFileDownloader;
class SonglengthsDatabase;
class HVSCMetadataService;
class GameBase64Service;

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
     * @param owner The QWidget used as parent for dialogs (e.g. ErrorHandler).
     * @param parent Qt object parent for this factory.
     */
    explicit ServiceFactory(QWidget *owner, QObject *parent = nullptr);

    /// @brief Returns the device connection service.
    [[nodiscard]] DeviceConnection *deviceConnection() const;
    /// @brief Returns the remote file model.
    [[nodiscard]] RemoteFileModel *remoteFileModel() const;
    /// @brief Returns the transfer queue.
    [[nodiscard]] TransferQueue *transferQueue() const;
    /// @brief Returns the config file loader.
    [[nodiscard]] ConfigFileLoader *configFileLoader() const;
    /// @brief Returns the file preview service.
    [[nodiscard]] FilePreviewService *filePreviewService() const;
    /// @brief Returns the transfer service.
    [[nodiscard]] TransferService *transferService() const;
    /// @brief Returns the error handler.
    [[nodiscard]] ErrorHandler *errorHandler() const;
    /// @brief Returns the status message service.
    [[nodiscard]] StatusMessageService *statusMessageService() const;
    /// @brief Returns the favorites manager.
    [[nodiscard]] FavoritesManager *favoritesManager() const;
    /// @brief Returns the playlist manager.
    [[nodiscard]] PlaylistManager *playlistManager() const;
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

    /// @brief Returns all metadata download services bundled together.
    [[nodiscard]] MetadataServiceBundle metadataBundle() const;

private:
    DeviceConnection *deviceConnection_ = nullptr;
    RemoteFileModel *remoteFileModel_ = nullptr;
    TransferQueue *transferQueue_ = nullptr;
    ConfigFileLoader *configFileLoader_ = nullptr;
    FilePreviewService *filePreviewService_ = nullptr;
    TransferService *transferService_ = nullptr;
    ErrorHandler *errorHandler_ = nullptr;
    StatusMessageService *statusMessageService_ = nullptr;
    FavoritesManager *favoritesManager_ = nullptr;
    PlaylistManager *playlistManager_ = nullptr;
    HttpFileDownloader *songlengthsDownloader_ = nullptr;
    SonglengthsDatabase *songlengthsDatabase_ = nullptr;
    HttpFileDownloader *stilDownloader_ = nullptr;
    HttpFileDownloader *buglistDownloader_ = nullptr;
    HVSCMetadataService *hvscMetadataService_ = nullptr;
    HttpFileDownloader *gameBase64Downloader_ = nullptr;
    GameBase64Service *gameBase64Service_ = nullptr;
};

#endif  // SERVICEFACTORY_H
