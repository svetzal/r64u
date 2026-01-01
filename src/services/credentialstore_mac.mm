#include "credentialstore.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

bool CredentialStore::storePassword(const QString &service, const QString &account,
                                    const QString &password)
{
    if (password.isEmpty()) {
        // Delete the password if empty string is provided
        return deletePassword(service, account);
    }

    NSString *serviceStr = service.toNSString();
    NSString *accountStr = account.toNSString();
    NSData *passwordData = [password.toNSString() dataUsingEncoding:NSUTF8StringEncoding];

    // First try to delete any existing item
    NSDictionary *deleteQuery = @{
        (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: serviceStr,
        (__bridge id)kSecAttrAccount: accountStr
    };
    SecItemDelete((__bridge CFDictionaryRef)deleteQuery);

    // Add the new item
    NSDictionary *addQuery = @{
        (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: serviceStr,
        (__bridge id)kSecAttrAccount: accountStr,
        (__bridge id)kSecValueData: passwordData,
        (__bridge id)kSecAttrAccessible: (__bridge id)kSecAttrAccessibleWhenUnlocked
    };

    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)addQuery, NULL);
    return status == errSecSuccess;
}

QString CredentialStore::retrievePassword(const QString &service, const QString &account)
{
    NSString *serviceStr = service.toNSString();
    NSString *accountStr = account.toNSString();

    NSDictionary *query = @{
        (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: serviceStr,
        (__bridge id)kSecAttrAccount: accountStr,
        (__bridge id)kSecReturnData: @YES,
        (__bridge id)kSecMatchLimit: (__bridge id)kSecMatchLimitOne
    };

    CFTypeRef result = NULL;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);

    if (status == errSecSuccess && result != NULL) {
        NSData *passwordData = (__bridge NSData *)result;
        NSString *password = [[NSString alloc] initWithData:passwordData
                                                   encoding:NSUTF8StringEncoding];
        QString qPassword = QString::fromNSString(password);
        CFRelease(result);
        return qPassword;
    }

    return QString();
}

bool CredentialStore::deletePassword(const QString &service, const QString &account)
{
    NSString *serviceStr = service.toNSString();
    NSString *accountStr = account.toNSString();

    NSDictionary *query = @{
        (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: serviceStr,
        (__bridge id)kSecAttrAccount: accountStr
    };

    OSStatus status = SecItemDelete((__bridge CFDictionaryRef)query);
    return status == errSecSuccess || status == errSecItemNotFound;
}
