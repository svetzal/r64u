#ifndef CREDENTIALSTORE_H
#define CREDENTIALSTORE_H

#include <QString>

class CredentialStore
{
public:
    static bool storePassword(const QString &service, const QString &account,
                              const QString &password);
    static QString retrievePassword(const QString &service, const QString &account);
    static bool deletePassword(const QString &service, const QString &account);

private:
    CredentialStore() = default;
};

#endif // CREDENTIALSTORE_H
