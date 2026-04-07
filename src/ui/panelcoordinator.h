#ifndef PANELCOORDINATOR_H
#define PANELCOORDINATOR_H

#include <QObject>

class ExplorePanel;
class TransferPanel;
class ViewPanel;
class ConfigPanel;
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
    explicit PanelCoordinator(ExplorePanel *explore, TransferPanel *transfer, ViewPanel *view,
                              ConfigPanel *config, DeviceConnection *connection,
                              RemoteFileModel *model, TransferService *transferService,
                              StatusMessageService *statusService, ErrorHandler *errorHandler,
                              QTabWidget *tabWidget, QObject *parent = nullptr);

signals:
    void windowTitleUpdateNeeded();
    void actionsUpdateNeeded();

private slots:
    void onModeChanged(int index);
    void onOperationSucceeded(const QString &operation);

private:
    ExplorePanel *explorePanel_;
    TransferPanel *transferPanel_;
    ViewPanel *viewPanel_;
    ConfigPanel *configPanel_;
    DeviceConnection *deviceConnection_;
    RemoteFileModel *remoteFileModel_;
    TransferService *transferService_;
    StatusMessageService *statusMessageService_;
    ErrorHandler *errorHandler_;
    QTabWidget *modeTabWidget_;
};

#endif  // PANELCOORDINATOR_H
