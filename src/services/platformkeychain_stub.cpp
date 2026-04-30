#include "platformkeychain.h"

// Stub implementation for platforms without native credential storage
// Passwords will not be stored securely - falls back to no storage

bool PlatformKeychain::storePassword(const QString &service, const QString &account,
                                    const QString &password)
{
    Q_UNUSED(service)
    Q_UNUSED(account)
    Q_UNUSED(password)
    // Stub: passwords not stored on this platform yet
    return false;
}

QString PlatformKeychain::retrievePassword(const QString &service, const QString &account)
{
    Q_UNUSED(service)
    Q_UNUSED(account)
    // Stub: no password storage on this platform yet
    return QString();
}

bool PlatformKeychain::deletePassword(const QString &service, const QString &account)
{
    Q_UNUSED(service)
    Q_UNUSED(account)
    return true;
}
