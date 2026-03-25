/**
 * @file mockcredentialstore.h
 * @brief In-memory mock credential store for testing.
 */

#ifndef MOCKCREDENTIALSTORE_H
#define MOCKCREDENTIALSTORE_H

#include "services/icredentialstore.h"

#include <QHash>
#include <QString>

/**
 * @brief In-memory credential store mock.
 *
 * Stores credentials in a QHash keyed by "service:account". No platform
 * calls are made, enabling deterministic testing without a keychain.
 */
class MockCredentialStore : public ICredentialStore
{
public:
    MockCredentialStore() = default;

    [[nodiscard]] bool storePassword(const QString &service, const QString &account,
                                     const QString &password) override
    {
        store_[key(service, account)] = password;
        return true;
    }

    [[nodiscard]] QString retrievePassword(const QString &service, const QString &account) override
    {
        return store_.value(key(service, account));
    }

    [[nodiscard]] bool deletePassword(const QString &service, const QString &account) override
    {
        return store_.remove(key(service, account)) > 0;
    }

    /**
     * @brief Clears all stored credentials.
     */
    void mockClear() { store_.clear(); }

    /**
     * @brief Returns the number of stored credentials.
     */
    [[nodiscard]] int mockCount() const { return store_.size(); }

private:
    static QString key(const QString &service, const QString &account)
    {
        return service + QLatin1Char(':') + account;
    }

    QHash<QString, QString> store_;
};

#endif  // MOCKCREDENTIALSTORE_H
