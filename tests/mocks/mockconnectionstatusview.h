#ifndef MOCKCONNECTIONSTATUSVIEW_H
#define MOCKCONNECTIONSTATUSVIEW_H

#include "ui/iconnectionstatusview.h"

#include <QList>
#include <QString>

class MockConnectionStatusView : public IConnectionStatusView
{
public:
    void setConnected(bool connected) override { connectedHistory.append(connected); }
    void setConnecting(bool connecting) override { connectingHistory.append(connecting); }
    void setHostname(const QString &h) override { hostnameHistory.append(h); }
    void setFirmwareVersion(const QString &v) override { firmwareHistory.append(v); }

    void reset()
    {
        connectedHistory.clear();
        connectingHistory.clear();
        hostnameHistory.clear();
        firmwareHistory.clear();
    }

    QList<bool> connectedHistory;
    QList<bool> connectingHistory;
    QList<QString> hostnameHistory;
    QList<QString> firmwareHistory;
};

#endif  // MOCKCONNECTIONSTATUSVIEW_H
