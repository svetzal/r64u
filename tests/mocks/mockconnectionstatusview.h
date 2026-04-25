#ifndef MOCKCONNECTIONSTATUSVIEW_H
#define MOCKCONNECTIONSTATUSVIEW_H

#include "ui/iconnectionstatusview.h"

#include <QList>
#include <QString>

class MockConnectionStatusView : public IConnectionStatusView
{
public:
    void setConnected(bool connected) override { connectedHistory.append(connected); }
    void setHostname(const QString &h) override { hostnameHistory.append(h); }
    void setFirmwareVersion(const QString &v) override { firmwareHistory.append(v); }

    void reset()
    {
        connectedHistory.clear();
        hostnameHistory.clear();
        firmwareHistory.clear();
    }

    QList<bool> connectedHistory;
    QList<QString> hostnameHistory;
    QList<QString> firmwareHistory;
};

#endif  // MOCKCONNECTIONSTATUSVIEW_H
