/**
 * @file systemcommandcontroller.h
 * @brief Controller for system-level machine commands.
 *
 * Encapsulates the pass-through REST commands for machine control,
 * enabling unit testing without Qt widgets.
 */
#ifndef SYSTEMCOMMANDCONTROLLER_H
#define SYSTEMCOMMANDCONTROLLER_H

#include <QObject>

class IRestClient;
class StatusMessageService;

/**
 * @brief Executes machine control commands via the REST API.
 *
 * Each public slot sends a command to the device and posts an
 * informational status message.
 */
class SystemCommandController : public QObject
{
    Q_OBJECT

public:
    explicit SystemCommandController(IRestClient *restClient, StatusMessageService *statusService,
                                     QObject *parent = nullptr);

public slots:
    void onReset();
    void onReboot();
    void onPause();
    void onResume();
    void onMenuButton();
    void powerOff();  ///< Called by MainWindow after dialog confirmation
    void onEjectDriveA();
    void onEjectDriveB();

private:
    IRestClient *restClient_;
    StatusMessageService *statusService_;
};

#endif  // SYSTEMCOMMANDCONTROLLER_H
