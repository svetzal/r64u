#ifndef SYSTEMCREDENTIALSTORE_H
#define SYSTEMCREDENTIALSTORE_H

#include "icredentialstore.h"
#include "platformkeychain.h"

/**
 * @brief Production credential store backed by the platform keychain.
 *
 * Delegates all operations to PlatformKeychain static methods.
 */
class SystemCredentialStore : public ICredentialStore
{
public:
    SystemCredentialStore() = default;

    [[nodiscard]] bool storePassword(const QString &service, const QString &account,
                                     const QString &password) override
    {
        return PlatformKeychain::storePassword(service, account, password);
    }

    [[nodiscard]] QString retrievePassword(const QString &service, const QString &account) override
    {
        return PlatformKeychain::retrievePassword(service, account);
    }

    [[nodiscard]] bool deletePassword(const QString &service, const QString &account) override
    {
        return PlatformKeychain::deletePassword(service, account);
    }
};

#endif  // SYSTEMCREDENTIALSTORE_H
