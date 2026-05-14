#include "configfileloaderservice.h"
#include "deviceconnection.h"
#include "errorhandler.h"
#include "filepreviewservice.h"
#include "gamebase64service.h"
#include "hvscmetadataservice.h"
#include "iaudioplaybackservice.h"
#include "iftpclient.h"
#include "irestclient.h"
#include "keyboardinputservice.h"
#include "remotefileoperationsservice.h"
#include "songlengthsdatabase.h"
#include "transferservice.h"
#include "videorecordingservice.h"

#include "models/remotefilemodel.h"

#include <QFileInfo>

void ErrorHandler::connectSources(DeviceConnection *dc, IRestClient *restClient,
                                  RemoteFileModel *rfm, IFtpClient *ftpClient,
                                  FilePreviewService *fps, ConfigFileLoaderService *cfl,
                                  TransferService *ts, SonglengthsDatabase *sld,
                                  HVSCMetadataService *hvsc, GameBase64Service *gb64,
                                  RemoteFileOperationsService *rfo, StreamingService *ss,
                                  IAudioPlaybackService *apb, VideoRecordingService *vrs,
                                  KeyboardInputService *kis)
{
    connectDeviceSources(dc, restClient, cfl);
    connectTransferSources(ftpClient, ts);
    connectModelSources(rfm, fps);
    connectMetadataSources(sld, hvsc, gb64);
    if (rfo) {
        connect(rfo, &RemoteFileOperationsService::operationFailed, this,
                &ErrorHandler::handleOperationFailed);
    }
    if (ss) {
        connectStreamingServiceSources(ss);
    }
    if (apb) {
        connect(apb, &IAudioPlaybackService::errorOccurred, this,
                &ErrorHandler::handleStreamingError);
    }
    if (vrs) {
        connect(vrs, &VideoRecordingService::error, this, &ErrorHandler::handleStreamingError);
    }
    if (kis) {
        connect(kis, &KeyboardInputService::errorOccurred, this, &ErrorHandler::handleDataError);
    }
}

void ErrorHandler::connectDeviceSources(DeviceConnection *dc, IRestClient *restClient,
                                        ConfigFileLoaderService *cfl)
{
    if (dc) {
        connect(dc, &DeviceConnection::connectionError, this, &ErrorHandler::handleConnectionError);
    }
    if (restClient) {
        connect(restClient, &IRestClient::operationFailed, this,
                &ErrorHandler::handleOperationFailed);
    }
    if (cfl) {
        connect(cfl, &ConfigFileLoaderService::loadFailed, this,
                [this](const QString &path, const QString &error) {
                    handleOperationFailed(tr("Loading %1").arg(QFileInfo(path).fileName()), error);
                });
    }
}

void ErrorHandler::connectTransferSources(IFtpClient *ftpClient, TransferService *ts)
{
    if (ftpClient) {
        connect(ftpClient, &IFtpClient::error, this, &ErrorHandler::handleDataError);
    }
    if (ts) {
        connect(ts, &TransferService::operationFailed, this, &ErrorHandler::handleOperationFailed);
    }
}

void ErrorHandler::connectModelSources(RemoteFileModel *rfm, FilePreviewService *fps)
{
    if (rfm) {
        connect(rfm, &RemoteFileModel::errorOccurred, this, &ErrorHandler::handleDataError);
    }
    if (fps) {
        connect(fps, &FilePreviewService::previewFailed, this,
                [this](const QString &path, const QString &error) {
                    handleOperationFailed(tr("Preview of %1").arg(path), error);
                });
    }
}

void ErrorHandler::connectMetadataSources(SonglengthsDatabase *sld, HVSCMetadataService *hvsc,
                                          GameBase64Service *gb64)
{
    if (sld) {
        connect(sld, &SonglengthsDatabase::downloadFailed, this, [this](const QString &error) {
            handleDownloadError(tr("Song lengths database"), error);
        });
    }
    if (hvsc) {
        connect(hvsc, &HVSCMetadataService::stilDownloadFailed, this,
                [this](const QString &error) { handleDownloadError(tr("HVSC STIL"), error); });
        connect(hvsc, &HVSCMetadataService::buglistDownloadFailed, this,
                [this](const QString &error) { handleDownloadError(tr("HVSC BUGlist"), error); });
    }
    if (gb64) {
        connect(gb64, &GameBase64Service::downloadFailed, this,
                [this](const QString &error) { handleDownloadError(tr("GameBase64"), error); });
    }
}
