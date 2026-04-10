#include "menubarbuilder.h"

#include "../services/systemcommandcontroller.h"

#include <QKeySequence>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QTabWidget>

QAction *MenuBarBuilder::build(QMainWindow *window, SystemCommandController *sysCtrl,
                               QTabWidget *modeTabWidget)
{
    // File menu
    auto *fileMenu = window->menuBar()->addMenu(QObject::tr("&File"));

    auto *connectAction = fileMenu->addAction(QObject::tr("&Connect"));
    QObject::connect(connectAction, &QAction::triggered, window,
                     [window]() { QMetaObject::invokeMethod(window, "onConnect"); });

    auto *disconnectAction = fileMenu->addAction(QObject::tr("&Disconnect"));
    QObject::connect(disconnectAction, &QAction::triggered, window,
                     [window]() { QMetaObject::invokeMethod(window, "onDisconnect"); });

    fileMenu->addSeparator();

    auto *prefsAction = fileMenu->addAction(QObject::tr("&Preferences..."));
    prefsAction->setShortcut(QKeySequence::Preferences);
    QObject::connect(prefsAction, &QAction::triggered, window,
                     [window]() { QMetaObject::invokeMethod(window, "onPreferences"); });

    fileMenu->addSeparator();

    auto *quitAction = fileMenu->addAction(QObject::tr("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    QObject::connect(quitAction, &QAction::triggered, window, &QMainWindow::close);

    // View menu
    auto *viewMenu = window->menuBar()->addMenu(QObject::tr("&View"));

    auto *exploreAction = viewMenu->addAction(QObject::tr("&Explore/Run Mode"));
    exploreAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_1));
    QObject::connect(exploreAction, &QAction::triggered, window,
                     [modeTabWidget]() { modeTabWidget->setCurrentIndex(0); });

    auto *transferAction = viewMenu->addAction(QObject::tr("&Transfer Mode"));
    transferAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_2));
    QObject::connect(transferAction, &QAction::triggered, window,
                     [modeTabWidget]() { modeTabWidget->setCurrentIndex(1); });

    auto *viewModeAction = viewMenu->addAction(QObject::tr("&View Mode"));
    viewModeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_3));
    QObject::connect(viewModeAction, &QAction::triggered, window,
                     [modeTabWidget]() { modeTabWidget->setCurrentIndex(2); });

    auto *configModeAction = viewMenu->addAction(QObject::tr("&Config Mode"));
    configModeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_4));
    QObject::connect(configModeAction, &QAction::triggered, window,
                     [modeTabWidget]() { modeTabWidget->setCurrentIndex(3); });

    viewMenu->addSeparator();

    auto *refreshAction = viewMenu->addAction(QObject::tr("&Refresh"));
    refreshAction->setShortcut(QKeySequence::Refresh);
    QObject::connect(refreshAction, &QAction::triggered, window,
                     [window]() { QMetaObject::invokeMethod(window, "onRefresh"); });

    // Machine menu
    auto *machineMenu = window->menuBar()->addMenu(QObject::tr("&Machine"));

    auto *resetMachineAction = machineMenu->addAction(QObject::tr("&Reset"));
    QObject::connect(resetMachineAction, &QAction::triggered, sysCtrl,
                     &SystemCommandController::onReset);

    machineMenu->addSeparator();

    auto *ejectDriveAAction = machineMenu->addAction(QObject::tr("Eject Drive &A"));
    QObject::connect(ejectDriveAAction, &QAction::triggered, sysCtrl,
                     &SystemCommandController::onEjectDriveA);

    auto *ejectDriveBAction = machineMenu->addAction(QObject::tr("Eject Drive &B"));
    QObject::connect(ejectDriveBAction, &QAction::triggered, sysCtrl,
                     &SystemCommandController::onEjectDriveB);

    // Help menu
    auto *helpMenu = window->menuBar()->addMenu(QObject::tr("&Help"));

    auto *aboutAction = helpMenu->addAction(QObject::tr("&About r64u"));
    QObject::connect(aboutAction, &QAction::triggered, window, [window]() {
        QMessageBox::about(window, QObject::tr("About r64u"),
                           QObject::tr("<h3>r64u</h3>"
                                       "<p>Version 0.1.0</p>"
                                       "<p>Remote access tool for Commodore 64 Ultimate "
                                       "devices.</p>"));
    });

    return refreshAction;
}
