#ifndef CONNECTIONSTATUSWIDGET_H
#define CONNECTIONSTATUSWIDGET_H

#include "iconnectionstatusview.h"

#include <QLabel>
#include <QWidget>

class ConnectionStatusWidget : public QWidget, public IConnectionStatusView
{
    Q_OBJECT

public:
    explicit ConnectionStatusWidget(QWidget *parent = nullptr);

    void setConnected(bool connected) override;
    void setHostname(const QString &hostname) override;
    void setFirmwareVersion(const QString &version) override;

private:
    QLabel *statusLabel_ = nullptr;
    QLabel *hostnameLabel_ = nullptr;
    QLabel *firmwareLabel_ = nullptr;
    QLabel *indicator_ = nullptr;
    bool connected_ = false;

    void updateDisplay();
};

#endif  // CONNECTIONSTATUSWIDGET_H
