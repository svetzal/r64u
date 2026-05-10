#include "services/errorhandler.h"
#include "services/iftpclient.h"

class ConfigFileLoaderService;
class DeviceConnection;
class FilePreviewService;
class GameBase64Service;
class HVSCMetadataService;
class IRestClient;
class RemoteFileModel;
class RemoteFileOperationsService;
class SonglengthsDatabase;
class TransferService;

void ErrorHandler::connectSources(DeviceConnection * /*dc*/, IRestClient * /*restClient*/,
                                  RemoteFileModel * /*rfm*/, IFtpClient *ftpClient,
                                  FilePreviewService * /*fps*/, ConfigFileLoaderService * /*cfl*/,
                                  TransferService * /*ts*/, SonglengthsDatabase * /*sld*/,
                                  HVSCMetadataService * /*hvsc*/, GameBase64Service * /*gb64*/,
                                  RemoteFileOperationsService * /*rfo*/)
{
    if (ftpClient) {
        connect(ftpClient, &IFtpClient::error, this, &ErrorHandler::handleDataError);
    }
}
