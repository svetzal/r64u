#ifndef SYSTEMCREDENTIALSTORE_H
#define SYSTEMCREDENTIALSTORE_H

#include "credentialstore.h"
#include "icredentialstore.h"

/**
 * @brief Production credential store backed by the platform keychain.
 *
 * Delegates all operations to CredentialStore static methods.
 */
class SystemCredentialStore : public ICredentialStore
{
public:
    SystemCredentialStore() = default;

    [[nodiscard]] bool storePassword(const QString &service, const QString &account,
                                     const QString &password) override
    {
        return CredentialStore::storePassword(service, account, password);
    }

    [[nodiscard]] QString retrievePassword(const QString &service, const QString &account) override
    {
        return CredentialStore::retrievePassword(service, account);
    }

    [[nodiscard]] bool deletePassword(const QString &service, const QString &account) override
    {
        return CredentialStore::deletePassword(service, account);
    }
};

#endif  // SYSTEMCREDENTIALSTORE_H
