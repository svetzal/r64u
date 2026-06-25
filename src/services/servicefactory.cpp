#include "servicefactory.h"

#include "models/remotefilemodel.h"
#include "models/transferqueue.h"
#include "services/configfileloaderservice.h"
#include "services/configurationservice.h"
#include "services/deviceactionservice.h"
#include "services/deviceconnectionmanager.h"
#include "services/errorhandler.h"
#include "services/favoritesservice.h"
#include "services/filepreviewservice.h"
#include "services/gamebase64service.h"
#include "services/httpfiledownloaderservice.h"
#include "services/hvscmetadataservice.h"
#include "services/ierrorpresenter.h"
#include "services/playlistservice.h"
#include "services/remotefileoperationsservice.h"
#include "services/songlengthsdatabaseservice.h"
#include "services/statusmessageservice.h"
#include "services/transferservice.h"
#include "ui/systemcommandcontroller.h"

ServiceFactory::ServiceFactory(QObject *parent) : QObject(parent)
{
    deviceConnection_ = new DeviceConnectionManager(this);
    remoteFileModel_ = new RemoteFileModel(this);
    transferQueue_ = new TransferQueue(this);
    configFileLoader_ = new ConfigFileLoaderService(this);

    // Connect the FTP client to the model and queue
    remoteFileModel_->setFtpClient(deviceConnection_->ftpClient());
    transferQueue_->setFtpClient(deviceConnection_->ftpClient());

    filePreviewService_ = new FilePreviewService(deviceConnection_->ftpClient(), this);
    transferService_ = new TransferService(deviceConnection_, transferQueue_, this);
    errorHandler_ = new ErrorHandler(nullptr, this);
    statusMessageService_ = new StatusMessageService(this);
    favoritesService_ = new FavoritesService(this);
    playlistService_ = new PlaylistService(deviceConnection_, this);
    songlengthsDownloader_ = new HttpFileDownloaderService(this);
    songlengthsDatabase_ = new SonglengthsDatabaseService(songlengthsDownloader_, this);
    stilDownloader_ = new HttpFileDownloaderService(this);
    buglistDownloader_ = new HttpFileDownloaderService(this);
    hvscMetadataService_ = new HVSCMetadataService(stilDownloader_, buglistDownloader_, this);
    gameBase64Downloader_ = new HttpFileDownloaderService(this);
    gameBase64Service_ = new GameBase64Service(gameBase64Downloader_, this);
    playlistService_->setSonglengthsDatabase(songlengthsDatabase_);
    configFileLoader_->setFtpClient(deviceConnection_->ftpClient());
    configFileLoader_->setRestClient(deviceConnection_->restClient());

    configurationService_ = new ConfigurationService(deviceConnection_, this);
    deviceActionService_ = new DeviceActionService(deviceConnection_, this);
    remoteFileOperations_ = new RemoteFileOperationsService(deviceConnection_->ftpClient(), this);
    systemCommandController_ =
        new SystemCommandController(deviceConnection_->restClient(), statusMessageService_, this);
}

void ServiceFactory::setErrorPresenter(IErrorPresenter *presenter)
{
    errorHandler_->setPresenter(presenter);
}

DeviceConnectionManager *ServiceFactory::deviceConnection() const
{
    return deviceConnection_;
}

RemoteFileModel *ServiceFactory::remoteFileModel() const
{
    return remoteFileModel_;
}

TransferQueue *ServiceFactory::transferQueue() const
{
    return transferQueue_;
}

ConfigFileLoaderService *ServiceFactory::configFileLoader() const
{
    return configFileLoader_;
}

FilePreviewService *ServiceFactory::filePreviewService() const
{
    return filePreviewService_;
}

TransferService *ServiceFactory::transferService() const
{
    return transferService_;
}

ErrorHandler *ServiceFactory::errorHandler() const
{
    return errorHandler_;
}

StatusMessageService *ServiceFactory::statusMessageService() const
{
    return statusMessageService_;
}

FavoritesService *ServiceFactory::favoritesService() const
{
    return favoritesService_;
}

PlaylistService *ServiceFactory::playlistService() const
{
    return playlistService_;
}

HttpFileDownloaderService *ServiceFactory::songlengthsDownloader() const
{
    return songlengthsDownloader_;
}

SonglengthsDatabaseService *ServiceFactory::songlengthsDatabase() const
{
    return songlengthsDatabase_;
}

HttpFileDownloaderService *ServiceFactory::stilDownloader() const
{
    return stilDownloader_;
}

HttpFileDownloaderService *ServiceFactory::buglistDownloader() const
{
    return buglistDownloader_;
}

HVSCMetadataService *ServiceFactory::hvscMetadataService() const
{
    return hvscMetadataService_;
}

HttpFileDownloaderService *ServiceFactory::gameBase64Downloader() const
{
    return gameBase64Downloader_;
}

GameBase64Service *ServiceFactory::gameBase64Service() const
{
    return gameBase64Service_;
}

ConfigurationService *ServiceFactory::configurationService() const
{
    return configurationService_;
}

DeviceActionService *ServiceFactory::deviceActionService() const
{
    return deviceActionService_;
}

RemoteFileOperationsService *ServiceFactory::remoteFileOperations() const
{
    return remoteFileOperations_;
}

SystemCommandController *ServiceFactory::systemCommandController() const
{
    return systemCommandController_;
}

MetadataServiceBundle ServiceFactory::metadataBundle() const
{
    return {songlengthsDownloader_, songlengthsDatabase_,  stilDownloader_,   buglistDownloader_,
            hvscMetadataService_,   gameBase64Downloader_, gameBase64Service_};
}
