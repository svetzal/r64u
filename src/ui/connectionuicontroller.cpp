#include "connectionuicontroller.h"

#include "services/deviceconnection.h"
#include "ui/connectionstatuswidget.h"

#include <QAction>

ConnectionUIController::ConnectionUIController(DeviceConnection *connection,
                                               ConnectionStatusWidget *statusWidget,
                                               QObject *parent)
    : QObject(parent), deviceConnection_(connection), statusWidget_(statusWidget)
{
    connect(deviceConnection_, &DeviceConnection::stateChanged, this,
            &ConnectionUIController::onConnectionStateChanged);
    connect(deviceConnection_, &DeviceConnection::deviceInfoUpdated, this,
            &ConnectionUIController::onDeviceInfoUpdated);
    connect(deviceConnection_, &DeviceConnection::driveInfoUpdated, this,
            &ConnectionUIController::onDriveInfoUpdated);
}

void ConnectionUIController::setManagedActions(const QList<QAction *> &systemActions,
                                               QAction *connectAction, QAction *refreshAction)
{
    systemActions_ = systemActions;
    connectAction_ = connectAction;
    refreshAction_ = refreshAction;
}

void ConnectionUIController::updateAll()
{
    updateStatusBar();
    updateActions();
}

void ConnectionUIController::onConnectionStateChanged()
{
    updateStatusBar();
    updateActions();
    emit windowTitleUpdateNeeded();
}

void ConnectionUIController::onDeviceInfoUpdated()
{
    updateStatusBar();
    emit windowTitleUpdateNeeded();
}

void ConnectionUIController::onDriveInfoUpdated()
{
    updateStatusBar();
    emit windowTitleUpdateNeeded();
}

void ConnectionUIController::updateStatusBar()
{
    if (!statusWidget_ || !deviceConnection_) {
        return;
    }

    if (deviceConnection_->isConnected()) {
        DeviceInfo info = deviceConnection_->deviceInfo();
        statusWidget_->setConnected(true);
        statusWidget_->setHostname(info.hostname.isEmpty() ? deviceConnection_->host()
                                                           : info.hostname);
        statusWidget_->setFirmwareVersion(info.firmwareVersion);
    } else {
        statusWidget_->setConnected(false);
    }
}

void ConnectionUIController::updateActions()
{
    if (!deviceConnection_) {
        return;
    }

    if (connectAction_) {
        DeviceConnection::ConnectionState state = deviceConnection_->state();
        switch (state) {
        case DeviceConnection::ConnectionState::Disconnected:
            connectAction_->setText(tr("Connect"));
            break;
        case DeviceConnection::ConnectionState::Connecting:
            connectAction_->setText(tr("Cancel"));
            break;
        case DeviceConnection::ConnectionState::Connected:
            connectAction_->setText(tr("Disconnect"));
            break;
        case DeviceConnection::ConnectionState::Reconnecting:
            connectAction_->setText(tr("Cancel"));
            break;
        }
    }

    bool restConnected = deviceConnection_->isRestConnected();
    for (QAction *action : systemActions_) {
        action->setEnabled(restConnected);
    }

    if (refreshAction_) {
        refreshAction_->setEnabled(deviceConnection_->isConnected());
    }
}
