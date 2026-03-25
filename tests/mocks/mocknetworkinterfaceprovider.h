/**
 * @file mocknetworkinterfaceprovider.h
 * @brief Mock network interface provider for testing StreamingManager.
 */

#ifndef MOCKNETWORKINTERFACEPROVIDER_H
#define MOCKNETWORKINTERFACEPROVIDER_H

#include "services/inetworkinterfaceprovider.h"

#include <QList>
#include <QNetworkInterface>

/**
 * @brief Mock implementation of INetworkInterfaceProvider for testing.
 *
 * Returns a configured list of network interfaces for deterministic tests.
 */
class MockNetworkInterfaceProvider : public INetworkInterfaceProvider
{
public:
    MockNetworkInterfaceProvider() = default;

    [[nodiscard]] QList<QNetworkInterface> allInterfaces() const override { return interfaces_; }

    /**
     * @brief Sets the interfaces returned by allInterfaces().
     * @param interfaces List of interfaces to return.
     */
    void mockSetInterfaces(const QList<QNetworkInterface> &interfaces) { interfaces_ = interfaces; }

private:
    QList<QNetworkInterface> interfaces_;
};

#endif  // MOCKNETWORKINTERFACEPROVIDER_H
