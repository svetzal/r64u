#ifndef CONNECTIONSTATUSWIDGET_H
#define CONNECTIONSTATUSWIDGET_H

#include "iconnectionstatusview.h"

#include <QLabel>
#include <QWidget>

/**
 * @brief Status-bar LED plus label showing the device connection state.
 *
 * The single LED (visual.md 2.2) sits to the left of the text so the reading
 * order is LED, state, hostname, firmware.
 */
class ConnectionStatusWidget : public QWidget, public IConnectionStatusView
{
    Q_OBJECT

public:
    explicit ConnectionStatusWidget(QWidget *parent = nullptr);

    void setConnected(bool connected) override;
    void setConnecting(bool connecting) override;
    void setHostname(const QString &hostname) override;
    void setFirmwareVersion(const QString &version) override;

private:
    QLabel *indicator_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QLabel *hostnameLabel_ = nullptr;
    QLabel *firmwareLabel_ = nullptr;
    bool connected_ = false;
    bool connecting_ = false;

    void updateDisplay();
};

#endif  // CONNECTIONSTATUSWIDGET_H
