#include "services/errorhandler.h"
#include "services/iftpclient.h"

class AudioPlaybackService;
class ConfigFileLoaderService;
class DeviceConnection;
class FilePreviewService;
class GameBase64Service;
class HVSCMetadataService;
class IAudioPlaybackService;
class IRestClient;
class KeyboardInputService;
class RemoteFileModel;
class RemoteFileOperationsService;
class SonglengthsDatabase;
class StreamingService;
class TransferService;
class VideoRecordingService;

void ErrorHandler::connectSources(DeviceConnection * /*dc*/, IRestClient * /*restClient*/,
                                  RemoteFileModel * /*rfm*/, IFtpClient *ftpClient,
                                  FilePreviewService * /*fps*/, ConfigFileLoaderService * /*cfl*/,
                                  TransferService * /*ts*/, SonglengthsDatabase * /*sld*/,
                                  HVSCMetadataService * /*hvsc*/, GameBase64Service * /*gb64*/,
                                  RemoteFileOperationsService * /*rfo*/, StreamingService * /*ss*/,
                                  IAudioPlaybackService * /*apb*/, VideoRecordingService * /*vrs*/,
                                  KeyboardInputService * /*kis*/)
{
    if (ftpClient) {
        connect(ftpClient, &IFtpClient::error, this, &ErrorHandler::handleDataError);
    }
}

void ErrorHandler::connectStreamingServiceSources(StreamingService * /*ss*/) {}
