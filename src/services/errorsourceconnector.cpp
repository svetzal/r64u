#include "configfileloaderservice.h"
#include "deviceconnectionmanager.h"
#include "errorhandler.h"
#include "filepreviewservice.h"
#include "gamebase64service.h"
#include "hvscmetadataservice.h"
#include "iaudioplaybackservice.h"
#include "ierroremitter.h"
#include "iftpclient.h"
#include "irestclient.h"
#include "keyboardinputservice.h"
#include "playlistservice.h"
#include "remotefileoperationsservice.h"
#include "songlengthsdatabaseservice.h"
#include "transferservice.h"
#include "videorecordingservice.h"

#include "models/remotefilemodel.h"

void ErrorHandler::connectSources(DeviceConnectionManager *dc, IRestClient *restClient,
                                  RemoteFileModel *rfm, IFtpClient *ftpClient,
                                  FilePreviewService *fps, ConfigFileLoaderService *cfl,
                                  TransferService *ts, SonglengthsDatabaseService *sld,
                                  HVSCMetadataService *hvsc, GameBase64Service *gb64,
                                  RemoteFileOperationsService *rfo, StreamingService *ss,
                                  IAudioPlaybackService *apb, VideoRecordingService *vrs,
                                  KeyboardInputService *kis, PlaylistService *ps)
{
    // All IErrorEmitter sources use a single registerSource() call
    registerSource(dc);
    registerSource(restClient);
    registerSource(ftpClient);
    registerSource(fps);
    registerSource(cfl);
    registerSource(ts);
    registerSource(sld);
    registerSource(hvsc);
    registerSource(gb64);
    registerSource(rfo);
    registerSource(apb);
    registerSource(vrs);
    registerSource(kis);
    registerSource(ps);

    // RemoteFileModel inherits QAbstractItemModel, not IErrorEmitter — legacy path
    if (rfm) {
        connect(rfm, &RemoteFileModel::errorOccurred, this, [this](const QString &message) {
            handleError(ErrorCategory::FileOperation, ErrorSeverity::Warning, tr("Error"), message);
        });
    }

    // Streaming sub-services need separate wiring for socketError signals
    if (ss) {
        connectStreamingServiceSources(ss);
    }
}

void ErrorHandler::connectDeviceSources(DeviceConnectionManager * /*dc*/,
                                        IRestClient * /*restClient*/,
                                        ConfigFileLoaderService * /*cfl*/)
{
    // Legacy shim — wiring now done via registerSource() in connectSources()
}

void ErrorHandler::connectTransferSources(IFtpClient * /*ftpClient*/, TransferService * /*ts*/)
{
    // Legacy shim — wiring now done via registerSource() in connectSources()
}

void ErrorHandler::connectModelSources(RemoteFileModel * /*rfm*/, FilePreviewService * /*fps*/)
{
    // Legacy shim — wiring now done via registerSource() in connectSources()
}

void ErrorHandler::connectMetadataSources(SonglengthsDatabaseService * /*sld*/,
                                          HVSCMetadataService * /*hvsc*/,
                                          GameBase64Service * /*gb64*/)
{
    // Legacy shim — wiring now done via registerSource() in connectSources()
}
