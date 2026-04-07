#include "panelcoordinator.h"

#include "models/remotefilemodel.h"
#include "services/configfileloader.h"
#include "services/deviceconnection.h"
#include "services/errorhandler.h"
#include "services/irestclient.h"
#include "services/statusmessageservice.h"
#include "services/transferservice.h"
#include "ui/configpanel.h"
#include "ui/explorepanel.h"
#include "ui/transferpanel.h"
#include "ui/viewpanel.h"

#include <QFileInfo>
#include <QTabWidget>

PanelCoordinator::PanelCoordinator(ExplorePanel *explore, TransferPanel *transfer, ViewPanel *view,
                                   ConfigPanel *config, DeviceConnection *connection,
                                   RemoteFileModel *model, TransferService *transferService,
                                   StatusMessageService *statusService, ErrorHandler *errorHandler,
                                   QTabWidget *tabWidget, QObject *parent)
    : QObject(parent), explorePanel_(explore), transferPanel_(transfer), viewPanel_(view),
      configPanel_(config), deviceConnection_(connection), remoteFileModel_(model),
      transferService_(transferService), statusMessageService_(statusService),
      errorHandler_(errorHandler), modeTabWidget_(tabWidget)
{
    // Panel status message routing
    connect(
        explorePanel_, &ExplorePanel::statusMessage, this,
        [this](const QString &msg, int timeout) { statusMessageService_->showInfo(msg, timeout); });
    connect(
        transferPanel_, &TransferPanel::statusMessage, this,
        [this](const QString &msg, int timeout) { statusMessageService_->showInfo(msg, timeout); });
    connect(transferPanel_, &TransferPanel::clearStatusMessages, statusMessageService_,
            &StatusMessageService::clearMessages);
    connect(viewPanel_, &ViewPanel::statusMessage, this, [this](const QString &msg, int timeout) {
        statusMessageService_->showInfo(msg, timeout);
    });
    connect(
        configPanel_, &ConfigPanel::statusMessage, this,
        [this](const QString &msg, int timeout) { statusMessageService_->showInfo(msg, timeout); });

    // Error handler routing
    connect(errorHandler_, &ErrorHandler::statusMessage, this,
            [this](const QString &msg, int timeout) {
                statusMessageService_->showWarning(msg, timeout);
            });

    // REST client success signals
    connect(deviceConnection_->restClient(), &IRestClient::operationSucceeded, this,
            &PanelCoordinator::onOperationSucceeded);

    // Model loading signals
    connect(remoteFileModel_, &RemoteFileModel::loadingStarted, this, [this](const QString &path) {
        statusMessageService_->showInfo(tr("Loading %1...").arg(path));
    });
    connect(remoteFileModel_, &RemoteFileModel::loadingFinished, this, [](const QString &) {
        // Loading finished - no need to show a message, just let it clear naturally
    });

    // Tab widget mode switching
    connect(modeTabWidget_, &QTabWidget::currentChanged, this, &PanelCoordinator::onModeChanged);
}

void PanelCoordinator::onModeChanged(int index)
{
    emit windowTitleUpdateNeeded();
    emit actionsUpdateNeeded();

    // Don't sync model while transfer operations are in progress
    bool canSync = deviceConnection_->isConnected() && !transferService_->isProcessing() &&
                   !transferService_->isScanning() && !transferService_->isProcessingDelete() &&
                   !transferService_->isCreatingDirectories();

    switch (index) {
    case 0:
        if (canSync) {
            QString panelDir = explorePanel_->currentDirectory();
            if (!panelDir.isEmpty() && panelDir != remoteFileModel_->rootPath()) {
                explorePanel_->setCurrentDirectory(panelDir);
            }
        }
        break;
    case 1:
        if (canSync) {
            QString panelDir = transferPanel_->currentRemoteDir();
            if (!panelDir.isEmpty() && panelDir != remoteFileModel_->rootPath()) {
                transferPanel_->setCurrentRemoteDir(panelDir);
            }
        }
        break;
    case 2:
        break;
    case 3:
        configPanel_->refreshIfEmpty();
        break;
    default:
        break;
    }
}

void PanelCoordinator::onOperationSucceeded(const QString &operation)
{
    statusMessageService_->showInfo(tr("%1 succeeded").arg(operation), 3000);

    if (operation == "mount" || operation == "unmount") {
        deviceConnection_->refreshDriveInfo();
    }
}
