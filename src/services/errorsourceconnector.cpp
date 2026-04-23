#include "configfileloader.h"
#include "deviceconnection.h"
#include "errorhandler.h"
#include "filepreviewservice.h"
#include "gamebase64service.h"
#include "hvscmetadataservice.h"
#include "iftpclient.h"
#include "irestclient.h"
#include "songlengthsdatabase.h"
#include "transferservice.h"

#include "models/remotefilemodel.h"

#include <QFileInfo>

void ErrorHandler::connectSources(DeviceConnection *dc, IRestClient *restClient,
                                  RemoteFileModel *rfm, IFtpClient *ftpClient,
                                  FilePreviewService *fps, ConfigFileLoader *cfl,
                                  TransferService *ts, SonglengthsDatabase *sld,
                                  HVSCMetadataService *hvsc, GameBase64Service *gb64)
{
    if (dc) {
        connect(dc, &DeviceConnection::connectionError, this, &ErrorHandler::handleConnectionError);
    }
    if (restClient) {
        connect(restClient, &IRestClient::operationFailed, this,
                &ErrorHandler::handleOperationFailed);
    }
    if (rfm) {
        connect(rfm, &RemoteFileModel::errorOccurred, this, &ErrorHandler::handleDataError);
    }
    if (ftpClient) {
        connect(ftpClient, &IFtpClient::error, this, &ErrorHandler::handleDataError);
    }
    if (fps) {
        connect(fps, &FilePreviewService::previewFailed, this,
                [this](const QString &path, const QString &error) {
                    handleOperationFailed(tr("Preview of %1").arg(path), error);
                });
    }
    if (cfl) {
        connect(cfl, &ConfigFileLoader::loadFailed, this,
                [this](const QString &path, const QString &error) {
                    handleOperationFailed(tr("Loading %1").arg(QFileInfo(path).fileName()), error);
                });
    }
    if (ts) {
        connect(ts, &TransferService::operationFailed, this, &ErrorHandler::handleOperationFailed);
    }
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
