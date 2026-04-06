#include "systemcommandcontroller.h"

#include "services/irestclient.h"
#include "services/statusmessageservice.h"

SystemCommandController::SystemCommandController(IRestClient *restClient,
                                                 StatusMessageService *statusService,
                                                 QObject *parent)
    : QObject(parent), restClient_(restClient), statusService_(statusService)
{
}

void SystemCommandController::onReset()
{
    restClient_->resetMachine();
    statusService_->showInfo(tr("Reset sent"), 3000);
}

void SystemCommandController::onReboot()
{
    restClient_->rebootMachine();
    statusService_->showInfo(tr("Reboot sent"), 3000);
}

void SystemCommandController::onPause()
{
    restClient_->pauseMachine();
    statusService_->showInfo(tr("Pause sent"), 3000);
}

void SystemCommandController::onResume()
{
    restClient_->resumeMachine();
    statusService_->showInfo(tr("Resume sent"), 3000);
}

void SystemCommandController::onMenuButton()
{
    restClient_->pressMenuButton();
    statusService_->showInfo(tr("Menu button pressed"), 3000);
}

void SystemCommandController::powerOff()
{
    restClient_->powerOffMachine();
    statusService_->showInfo(tr("Power off sent"), 3000);
}

void SystemCommandController::onEjectDriveA()
{
    restClient_->unmountImage("a");
    statusService_->showInfo(tr("Ejecting Drive A"), 3000);
}

void SystemCommandController::onEjectDriveB()
{
    restClient_->unmountImage("b");
    statusService_->showInfo(tr("Ejecting Drive B"), 3000);
}
