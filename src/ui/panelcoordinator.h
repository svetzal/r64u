#ifndef PANELCOORDINATOR_H
#define PANELCOORDINATOR_H

#include <QObject>

class IPanel;
class StatusMessageService;
class RemoteFileModel;
class TransferService;
class DeviceConnection;
class ErrorHandler;
class QTabWidget;

class PanelCoordinator : public QObject
{
    Q_OBJECT

public:
    explicit PanelCoordinator(IPanel *explore, IPanel *transfer, IPanel *view, IPanel *config,
                              DeviceConnection *connection, RemoteFileModel *model,
                              TransferService *transferService, StatusMessageService *statusService,
                              ErrorHandler *errorHandler, QTabWidget *tabWidget,
                              QObject *parent = nullptr);

signals:
    void windowTitleUpdateNeeded();
    void actionsUpdateNeeded();

private slots:
    void onModeChanged(int index);
    void onOperationSucceeded(const QString &operation);
    void onAnyPanelStatusMessage(const QString &msg, int timeout);
    void onTransferPanelClearMessages();

private:
    IPanel *explorePanel_;
    IPanel *transferPanel_;
    IPanel *viewPanel_;
    IPanel *configPanel_;
    DeviceConnection *deviceConnection_;
    RemoteFileModel *remoteFileModel_;
    TransferService *transferService_;
    StatusMessageService *statusMessageService_;
    ErrorHandler *errorHandler_;
    QTabWidget *modeTabWidget_;
};

#endif  // PANELCOORDINATOR_H
