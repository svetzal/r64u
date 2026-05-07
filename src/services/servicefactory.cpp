#include "servicefactory.h"

#include "models/remotefilemodel.h"
#include "models/transferqueue.h"
#include "services/configfileloader.h"
#include "services/configurationservice.h"
#include "services/deviceactionservice.h"
#include "services/deviceconnection.h"
#include "services/errorhandler.h"
#include "services/favoritesservice.h"
#include "services/filepreviewservice.h"
#include "services/gamebase64service.h"
#include "services/httpfiledownloader.h"
#include "services/hvscmetadataservice.h"
#include "services/playlistservice.h"
#include "services/remotefileoperations.h"
#include "services/songlengthsdatabase.h"
#include "services/statusmessageservice.h"
#include "services/systemcommandcontroller.h"
#include "services/transferservice.h"

ServiceFactory::ServiceFactory(QWidget *owner, QObject *parent) : QObject(parent)
{
    deviceConnection_ = new DeviceConnection(this);
    remoteFileModel_ = new RemoteFileModel(this);
    transferQueue_ = new TransferQueue(this);
    configFileLoader_ = new ConfigFileLoader(this);

    // Connect the FTP client to the model and queue
    remoteFileModel_->setFtpClient(deviceConnection_->ftpClient());
    transferQueue_->setFtpClient(deviceConnection_->ftpClient());

    filePreviewService_ = new FilePreviewService(deviceConnection_->ftpClient(), this);
    transferService_ = new TransferService(deviceConnection_, transferQueue_, this);
    errorHandler_ = new ErrorHandler(owner, this);
    statusMessageService_ = new StatusMessageService(this);
    favoritesService_ = new FavoritesService(this);
    playlistService_ = new PlaylistService(deviceConnection_, this);
    songlengthsDownloader_ = new HttpFileDownloader(this);
    songlengthsDatabase_ = new SonglengthsDatabase(songlengthsDownloader_, this);
    stilDownloader_ = new HttpFileDownloader(this);
    buglistDownloader_ = new HttpFileDownloader(this);
    hvscMetadataService_ = new HVSCMetadataService(stilDownloader_, buglistDownloader_, this);
    gameBase64Downloader_ = new HttpFileDownloader(this);
    gameBase64Service_ = new GameBase64Service(gameBase64Downloader_, this);
    playlistService_->setSonglengthsDatabase(songlengthsDatabase_);
    configFileLoader_->setFtpClient(deviceConnection_->ftpClient());
    configFileLoader_->setRestClient(deviceConnection_->restClient());

    configurationService_ = new ConfigurationService(deviceConnection_, this);
    deviceActionService_ = new DeviceActionService(deviceConnection_, this);
    remoteFileOperations_ = new RemoteFileOperations(deviceConnection_->ftpClient(), this);
    systemCommandController_ =
        new SystemCommandController(deviceConnection_->restClient(), statusMessageService_, this);
}

DeviceConnection *ServiceFactory::deviceConnection() const
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

ConfigFileLoader *ServiceFactory::configFileLoader() const
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

HttpFileDownloader *ServiceFactory::songlengthsDownloader() const
{
    return songlengthsDownloader_;
}

SonglengthsDatabase *ServiceFactory::songlengthsDatabase() const
{
    return songlengthsDatabase_;
}

HttpFileDownloader *ServiceFactory::stilDownloader() const
{
    return stilDownloader_;
}

HttpFileDownloader *ServiceFactory::buglistDownloader() const
{
    return buglistDownloader_;
}

HVSCMetadataService *ServiceFactory::hvscMetadataService() const
{
    return hvscMetadataService_;
}

HttpFileDownloader *ServiceFactory::gameBase64Downloader() const
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

RemoteFileOperations *ServiceFactory::remoteFileOperations() const
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
