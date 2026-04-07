#ifndef CONNECTIONUICONTROLLER_H
#define CONNECTIONUICONTROLLER_H

#include <QList>
#include <QObject>

class DeviceConnection;
class ConnectionStatusWidget;
class QAction;

class ConnectionUIController : public QObject
{
    Q_OBJECT

public:
    explicit ConnectionUIController(DeviceConnection *connection,
                                    ConnectionStatusWidget *statusWidget,
                                    QObject *parent = nullptr);

    void setManagedActions(const QList<QAction *> &systemActions, QAction *connectAction,
                           QAction *refreshAction);

    void updateAll();

signals:
    void windowTitleUpdateNeeded();

private slots:
    void onConnectionStateChanged();
    void onDeviceInfoUpdated();
    void onDriveInfoUpdated();

private:
    void updateStatusBar();
    void updateActions();

    DeviceConnection *deviceConnection_ = nullptr;
    ConnectionStatusWidget *statusWidget_ = nullptr;
    QAction *connectAction_ = nullptr;
    QAction *refreshAction_ = nullptr;
    QList<QAction *> systemActions_;
};

#endif  // CONNECTIONUICONTROLLER_H
