#ifndef INETWORKINTERFACEPROVIDER_H
#define INETWORKINTERFACEPROVIDER_H

#include <QList>
#include <QNetworkInterface>

/**
 * @brief Abstract interface for enumerating local network interfaces.
 *
 * Production implementation delegates to QNetworkInterface::allInterfaces().
 * Test implementation returns a configured list of interfaces.
 */
class INetworkInterfaceProvider
{
public:
    virtual ~INetworkInterfaceProvider() = default;

    /**
     * @brief Returns all active network interfaces.
     * @return List of QNetworkInterface objects.
     */
    [[nodiscard]] virtual QList<QNetworkInterface> allInterfaces() const = 0;
};

#endif  // INETWORKINTERFACEPROVIDER_H
