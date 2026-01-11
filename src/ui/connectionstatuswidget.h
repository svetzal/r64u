#ifndef CONNECTIONSTATUSWIDGET_H
#define CONNECTIONSTATUSWIDGET_H

#include <QWidget>
#include <QLabel>

class ConnectionStatusWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ConnectionStatusWidget(QWidget *parent = nullptr);

    void setConnected(bool connected);
    void setHostname(const QString &hostname);
    void setFirmwareVersion(const QString &version);

private:
    QLabel *statusLabel_ = nullptr;
    QLabel *hostnameLabel_ = nullptr;
    QLabel *firmwareLabel_ = nullptr;
    QLabel *indicator_ = nullptr;
    bool connected_ = false;

    void updateDisplay();
};

#endif // CONNECTIONSTATUSWIDGET_H
