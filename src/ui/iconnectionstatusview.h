#pragma once

#include <QString>

class IConnectionStatusView
{
public:
    virtual ~IConnectionStatusView() = default;
    virtual void setConnected(bool connected) = 0;
    /// Shows an in-progress connection attempt (connecting or reconnecting).
    virtual void setConnecting(bool connecting) = 0;
    virtual void setHostname(const QString &hostname) = 0;
    virtual void setFirmwareVersion(const QString &version) = 0;
};
