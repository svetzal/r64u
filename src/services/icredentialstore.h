/**
 * @file icredentialstore.h
 * @brief Gateway interface for credential storage implementations.
 *
 * Abstracts platform-specific secure storage so that callers can be
 * tested with an in-memory mock instead of touching the system keychain.
 */

#ifndef ICREDENTIALSTORE_H
#define ICREDENTIALSTORE_H

#include <QString>

/**
 * @brief Abstract interface for secure credential storage.
 *
 * Production implementation delegates to the platform keychain
 * (Keychain on macOS, stub on other platforms). Test implementation
 * uses an in-memory QHash.
 */
class ICredentialStore
{
public:
    virtual ~ICredentialStore() = default;

    /**
     * @brief Stores a password for the given service and account.
     * @param service Service identifier (e.g. "r64u").
     * @param account Account identifier (e.g. device hostname).
     * @param password The password to store.
     * @return True if the password was stored successfully.
     */
    [[nodiscard]] virtual bool storePassword(const QString &service, const QString &account,
                                             const QString &password) = 0;

    /**
     * @brief Retrieves a password for the given service and account.
     * @param service Service identifier.
     * @param account Account identifier.
     * @return The stored password, or an empty string if not found.
     */
    [[nodiscard]] virtual QString retrievePassword(const QString &service,
                                                   const QString &account) = 0;

    /**
     * @brief Deletes a stored password.
     * @param service Service identifier.
     * @param account Account identifier.
     * @return True if the password was deleted successfully.
     */
    [[nodiscard]] virtual bool deletePassword(const QString &service, const QString &account) = 0;
};

#endif  // ICREDENTIALSTORE_H
