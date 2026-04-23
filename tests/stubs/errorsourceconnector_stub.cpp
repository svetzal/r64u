#include "services/errorhandler.h"
#include "services/iftpclient.h"

class ConfigFileLoader;
class DeviceConnection;
class FilePreviewService;
class GameBase64Service;
class HVSCMetadataService;
class IRestClient;
class RemoteFileModel;
class SonglengthsDatabase;
class TransferService;

void ErrorHandler::connectSources(DeviceConnection * /*dc*/, IRestClient * /*restClient*/,
                                  RemoteFileModel * /*rfm*/, IFtpClient *ftpClient,
                                  FilePreviewService * /*fps*/, ConfigFileLoader * /*cfl*/,
                                  TransferService * /*ts*/, SonglengthsDatabase * /*sld*/,
                                  HVSCMetadataService * /*hvsc*/, GameBase64Service * /*gb64*/)
{
    if (ftpClient) {
        connect(ftpClient, &IFtpClient::error, this, &ErrorHandler::handleDataError);
    }
}
