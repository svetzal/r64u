#ifndef PLATFORMKEYCHAIN_H
#define PLATFORMKEYCHAIN_H

#include <QString>

class PlatformKeychain
{
public:
    static bool storePassword(const QString &service, const QString &account,
                              const QString &password);
    static QString retrievePassword(const QString &service, const QString &account);
    static bool deletePassword(const QString &service, const QString &account);

private:
    PlatformKeychain() = default;
};

#endif  // PLATFORMKEYCHAIN_H
