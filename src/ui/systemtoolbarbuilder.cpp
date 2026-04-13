#include "systemtoolbarbuilder.h"

#include "connectionstatuswidget.h"
#include "connectionuicontroller.h"

#include "services/systemcommandcontroller.h"

#include <QAction>
#include <QMainWindow>
#include <QSizePolicy>
#include <QToolBar>
#include <QWidget>

SystemToolBarResult SystemToolBarBuilder::build(QMainWindow *window, QToolBar *toolBar,
                                                DeviceConnection *deviceConnection,
                                                SystemCommandController *sysCtrl,
                                                QAction *refreshAction)
{
    SystemToolBarResult result;

    // Connect action (placeholder — caller wires the trigger to private slots)
    result.connectAction = toolBar->addAction(QMainWindow::tr("Connect"));
    result.connectAction->setToolTip(QMainWindow::tr("Connect to C64U device"));

    toolBar->addSeparator();

    // Machine control actions
    result.resetAction = toolBar->addAction(QMainWindow::tr("Reset"));
    result.resetAction->setToolTip(QMainWindow::tr("Reset the C64"));
    QObject::connect(result.resetAction, &QAction::triggered, sysCtrl,
                     &SystemCommandController::onReset);

    result.rebootAction = toolBar->addAction(QMainWindow::tr("Reboot"));
    result.rebootAction->setToolTip(QMainWindow::tr("Reboot the Ultimate device"));
    QObject::connect(result.rebootAction, &QAction::triggered, sysCtrl,
                     &SystemCommandController::onReboot);

    result.pauseAction = toolBar->addAction(QMainWindow::tr("Pause"));
    result.pauseAction->setToolTip(QMainWindow::tr("Pause C64 execution"));
    QObject::connect(result.pauseAction, &QAction::triggered, sysCtrl,
                     &SystemCommandController::onPause);

    result.resumeAction = toolBar->addAction(QMainWindow::tr("Resume"));
    result.resumeAction->setToolTip(QMainWindow::tr("Resume C64 execution"));
    QObject::connect(result.resumeAction, &QAction::triggered, sysCtrl,
                     &SystemCommandController::onResume);

    result.menuAction = toolBar->addAction(QMainWindow::tr("Menu"));
    result.menuAction->setToolTip(QMainWindow::tr("Press Ultimate menu button"));
    QObject::connect(result.menuAction, &QAction::triggered, sysCtrl,
                     &SystemCommandController::onMenuButton);

    result.powerOffAction = toolBar->addAction(QMainWindow::tr("Power Off"));
    result.powerOffAction->setToolTip(QMainWindow::tr("Power off the Ultimate device"));

    toolBar->addSeparator();

    result.prefsAction = toolBar->addAction(QMainWindow::tr("Preferences"));
    result.prefsAction->setToolTip(QMainWindow::tr("Open preferences dialog"));

    // Spacer to push connection status to the right
    auto *spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolBar->addWidget(spacer);

    // Connection status on the right
    result.connectionStatus = new ConnectionStatusWidget();
    toolBar->addWidget(result.connectionStatus);

    // Create ConnectionUIController with all managed actions
    result.connectionUiController =
        new ConnectionUIController(deviceConnection, result.connectionStatus, window);
    result.connectionUiController->setManagedActions({result.resetAction, result.rebootAction,
                                                      result.pauseAction, result.resumeAction,
                                                      result.menuAction, result.powerOffAction},
                                                     result.connectAction, refreshAction);

    return result;
}
