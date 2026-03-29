/**
 * @file ftpclientmixin.h
 * @brief Free-function helpers for FTP client lifecycle management.
 *
 * Provides a reusable helper to disconnect all signals from an FTP client
 * to a given QObject receiver, used in destructors and setFtpClient() setters
 * to avoid signals being delivered after the receiver's members are destroyed.
 */

#ifndef FTPCLIENTMIXIN_H
#define FTPCLIENTMIXIN_H

#include "iftpclient.h"

#include <QObject>
#include <QPointer>

/**
 * @brief Disconnects all signals from @p client to @p receiver, if @p client is valid.
 *
 * Call this in destructors and at the start of setFtpClient() to safely remove
 * signal connections from the old FTP client before the receiver is destroyed
 * or before a new client is assigned.
 *
 * @param client The FTP client to disconnect from (may be null or already-deleted).
 * @param receiver The QObject whose slots should be disconnected.
 */
inline void disconnectFtpClient(const QPointer<IFtpClient> &client, QObject *receiver)
{
    if (client) {
        QObject::disconnect(client, nullptr, receiver, nullptr);
    }
}

#endif  // FTPCLIENTMIXIN_H
