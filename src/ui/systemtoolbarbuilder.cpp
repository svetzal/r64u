#include "systemtoolbarbuilder.h"

#include "connectionstatuswidget.h"
#include "connectionuicontroller.h"
#include "pixelicons.h"
#include "systemcommandcontroller.h"

#include <QAction>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QSize>
#include <QSizePolicy>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>

namespace {

/// A tool button for @p action that looks like the ones the toolbar makes itself.
QToolButton *toolButtonFor(QAction *action, const QToolBar *toolBar)
{
    auto *button = new QToolButton();
    button->setDefaultAction(action);
    button->setAutoRaise(true);
    button->setIconSize(toolBar->iconSize());
    button->setToolButtonStyle(toolBar->toolButtonStyle());
    QObject::connect(toolBar, &QToolBar::iconSizeChanged, button, &QToolButton::setIconSize);
    QObject::connect(toolBar, &QToolBar::toolButtonStyleChanged, button,
                     &QToolButton::setToolButtonStyle);
    return button;
}

/// Marks a toolbar action's button as guarded so the theme rings it in red.
void markAsDangerAction(QToolBar *toolBar, QAction *action)
{
    if (QWidget *button = toolBar->widgetForAction(action)) {
        button->setObjectName(QStringLiteral("DangerAction"));
    }
}

}  // namespace

SystemToolBarResult SystemToolBarBuilder::build(QMainWindow *window, QToolBar *toolBar,
                                                DeviceConnectionManager *deviceConnection,
                                                SystemCommandController *sysCtrl,
                                                QAction *refreshAction)
{
    using pixeliconcore::Icon;
    SystemToolBarResult result;
    toolBar->setIconSize(QSize(16, 16));

    // Connect action (placeholder — caller wires the trigger to private slots)
    result.connectAction =
        toolBar->addAction(pixelicons::icon(Icon::Connect), QMainWindow::tr("Connect"));
    result.connectAction->setToolTip(QMainWindow::tr("Connect to C64U device"));

    toolBar->addSeparator();

    // The C64 itself: Reset · Pause · Resume · Menu, set tight like keys on a front panel
    result.resetAction =
        new QAction(pixelicons::icon(Icon::Reset), QMainWindow::tr("Reset"), window);
    result.resetAction->setToolTip(QMainWindow::tr("Reset the C64"));
    QObject::connect(result.resetAction, &QAction::triggered, sysCtrl,
                     &SystemCommandController::onReset);

    result.pauseAction =
        new QAction(pixelicons::icon(Icon::Pause), QMainWindow::tr("Pause"), window);
    result.pauseAction->setToolTip(QMainWindow::tr("Pause C64 execution"));
    QObject::connect(result.pauseAction, &QAction::triggered, sysCtrl,
                     &SystemCommandController::onPause);

    result.resumeAction =
        new QAction(pixelicons::icon(Icon::Resume), QMainWindow::tr("Resume"), window);
    result.resumeAction->setToolTip(QMainWindow::tr("Resume C64 execution"));
    QObject::connect(result.resumeAction, &QAction::triggered, sysCtrl,
                     &SystemCommandController::onResume);

    result.menuAction = new QAction(pixelicons::icon(Icon::Menu), QMainWindow::tr("Menu"), window);
    result.menuAction->setToolTip(QMainWindow::tr("Press Ultimate menu button"));
    QObject::connect(result.menuAction, &QAction::triggered, sysCtrl,
                     &SystemCommandController::onMenuButton);

    auto *machineGroup = new QWidget();
    machineGroup->setObjectName(QStringLiteral("MachineControls"));
    auto *machineLayout = new QHBoxLayout(machineGroup);
    machineLayout->setContentsMargins(0, 0, 0, 0);
    machineLayout->setSpacing(0);
    for (QAction *action :
         {result.resetAction, result.pauseAction, result.resumeAction, result.menuAction}) {
        machineLayout->addWidget(toolButtonFor(action, toolBar));
    }
    toolBar->addWidget(machineGroup);

    toolBar->addSeparator();

    // The Ultimate device itself: guarded, since both interrupt whatever is running
    result.rebootAction =
        toolBar->addAction(pixelicons::icon(Icon::Reboot), QMainWindow::tr("Reboot"));
    result.rebootAction->setToolTip(QMainWindow::tr("Reboot the Ultimate device"));
    QObject::connect(result.rebootAction, &QAction::triggered, sysCtrl,
                     &SystemCommandController::onReboot);
    markAsDangerAction(toolBar, result.rebootAction);

    result.powerOffAction =
        toolBar->addAction(pixelicons::icon(Icon::PowerOff), QMainWindow::tr("Power Off"));
    result.powerOffAction->setToolTip(QMainWindow::tr("Power off the Ultimate device"));
    markAsDangerAction(toolBar, result.powerOffAction);

    // Preferences: the caller wires it, and on macOS the app menu already carries it
    result.prefsAction =
        new QAction(pixelicons::icon(Icon::Preferences), QMainWindow::tr("Preferences"), window);
    result.prefsAction->setToolTip(QMainWindow::tr("Open preferences dialog"));
#ifndef Q_OS_MACOS
    toolBar->addSeparator();
    toolBar->addAction(result.prefsAction);
#endif

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
