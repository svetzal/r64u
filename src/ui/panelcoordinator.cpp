#include "panelcoordinator.h"

#include "models/remotefilemodel.h"
#include "services/deviceconnection.h"
#include "services/errorhandler.h"
#include "services/irestclient.h"
#include "services/statusmessageservice.h"
#include "services/transferservice.h"
#include "ui/ipanel.h"

#include <QTabWidget>

PanelCoordinator::PanelCoordinator(IPanel *explore, IPanel *transfer, IPanel *view, IPanel *config,
                                   DeviceConnection *connection, RemoteFileModel *model,
                                   TransferService *transferService,
                                   StatusMessageService *statusService, ErrorHandler *errorHandler,
                                   QTabWidget *tabWidget, QObject *parent)
    : QObject(parent), explorePanel_(explore), transferPanel_(transfer), viewPanel_(view),
      configPanel_(config), deviceConnection_(connection), remoteFileModel_(model),
      transferService_(transferService), statusMessageService_(statusService),
      errorHandler_(errorHandler), modeTabWidget_(tabWidget)
{
    // Panel status message routing via string-based SIGNAL/SLOT (required for IPanel bridge)
    connect(explorePanel_->asQObject(), SIGNAL(statusMessage(QString, int)), this,
            SLOT(onAnyPanelStatusMessage(QString, int)));
    connect(transferPanel_->asQObject(), SIGNAL(statusMessage(QString, int)), this,
            SLOT(onAnyPanelStatusMessage(QString, int)));
    connect(transferPanel_->asQObject(), SIGNAL(clearStatusMessages()), this,
            SLOT(onTransferPanelClearMessages()));
    connect(viewPanel_->asQObject(), SIGNAL(statusMessage(QString, int)), this,
            SLOT(onAnyPanelStatusMessage(QString, int)));
    connect(configPanel_->asQObject(), SIGNAL(statusMessage(QString, int)), this,
            SLOT(onAnyPanelStatusMessage(QString, int)));

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

void PanelCoordinator::onAnyPanelStatusMessage(const QString &msg, int timeout)
{
    statusMessageService_->showInfo(msg, timeout);
}

void PanelCoordinator::onTransferPanelClearMessages()
{
    statusMessageService_->clearMessages();
}

void PanelCoordinator::onModeChanged(int index)
{
    emit windowTitleUpdateNeeded();
    emit actionsUpdateNeeded();

    // Don't sync model while transfer operations are in progress
    bool canSync = deviceConnection_->isConnected() && transferService_ &&
                   !transferService_->isProcessing() && !transferService_->isScanning() &&
                   !transferService_->isProcessingDelete() &&
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
