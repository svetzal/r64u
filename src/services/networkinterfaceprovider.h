/**
 * @file networkinterfaceprovider.h
 * @brief Production implementation of INetworkInterfaceProvider.
 *
 * Thin wrapper around QNetworkInterface::allInterfaces(). Contains no
 * logic worth testing independently.
 */

#ifndef NETWORKINTERFACEPROVIDER_H
#define NETWORKINTERFACEPROVIDER_H

#include "inetworkinterfaceprovider.h"

#include <QNetworkInterface>

/**
 * @brief Production network interface provider.
 *
 * Delegates directly to QNetworkInterface::allInterfaces().
 */
class NetworkInterfaceProvider : public INetworkInterfaceProvider
{
public:
    NetworkInterfaceProvider() = default;

    [[nodiscard]] QList<QNetworkInterface> allInterfaces() const override
    {
        return QNetworkInterface::allInterfaces();
    }
};

#endif  // NETWORKINTERFACEPROVIDER_H
