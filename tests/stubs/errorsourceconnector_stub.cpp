#include "services/errorhandler.h"
#include "services/iftpclient.h"

class AudioPlaybackService;
class ConfigFileLoaderService;
class DeviceConnectionManager;
class FilePreviewService;
class GameBase64Service;
class HVSCMetadataService;
class IAudioPlaybackService;
class IRestClient;
class KeyboardInputService;
class PlaylistService;
class RemoteFileModel;
class RemoteFileOperationsService;
class SonglengthsDatabase;
class StreamingService;
class TransferService;
class VideoRecordingService;

void ErrorHandler::connectSources(DeviceConnectionManager * /*dc*/, IRestClient * /*restClient*/,
                                  RemoteFileModel * /*rfm*/, IFtpClient *ftpClient,
                                  FilePreviewService * /*fps*/, ConfigFileLoaderService * /*cfl*/,
                                  TransferService * /*ts*/, SonglengthsDatabase * /*sld*/,
                                  HVSCMetadataService * /*hvsc*/, GameBase64Service * /*gb64*/,
                                  RemoteFileOperationsService * /*rfo*/, StreamingService * /*ss*/,
                                  IAudioPlaybackService * /*apb*/, VideoRecordingService * /*vrs*/,
                                  KeyboardInputService * /*kis*/, PlaylistService * /*ps*/)
{
    connectTransferSources(ftpClient, nullptr);
}

void ErrorHandler::connectStreamingServiceSources(StreamingService * /*ss*/) {}

void ErrorHandler::connectDeviceSources(DeviceConnectionManager * /*dc*/,
                                        IRestClient * /*restClient*/,
                                        ConfigFileLoaderService * /*cfl*/)
{
}

void ErrorHandler::connectTransferSources(IFtpClient *ftpClient, TransferService * /*ts*/)
{
    if (ftpClient) {
        connect(ftpClient, &IFtpClient::error, this, &ErrorHandler::handleDataError);
    }
}

void ErrorHandler::connectModelSources(RemoteFileModel * /*rfm*/, FilePreviewService * /*fps*/) {}

void ErrorHandler::connectMetadataSources(SonglengthsDatabase * /*sld*/,
                                          HVSCMetadataService * /*hvsc*/,
                                          GameBase64Service * /*gb64*/)
{
}
