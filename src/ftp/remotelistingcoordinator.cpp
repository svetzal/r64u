/**
 * @file remotelistingcoordinator.cpp
 * @brief Orchestrates lazy-load FTP directory listing requests.
 */

#include "remotelistingcoordinator.h"

#include "core/ftpclientmixin.h"
#include "utils/logging.h"

RemoteListingCoordinator::RemoteListingCoordinator(QObject *parent) : QObject(parent) {}

void RemoteListingCoordinator::setFtpClient(IFtpClient *client)
{
    disconnectFtpClient(ftpClient_, this);

    ftpClient_ = client;

    if (ftpClient_) {
        connect(ftpClient_, &IFtpClient::directoryListed, this,
                &RemoteListingCoordinator::onDirectoryListed);
        connect(ftpClient_, &IFtpClient::operationFailed, this,
                &RemoteListingCoordinator::onFtpOperationFailed);
        connect(ftpClient_, &IFtpClient::error, this, &RemoteListingCoordinator::onFtpError);
        connect(ftpClient_, &IFtpClient::disconnected, this,
                &RemoteListingCoordinator::onFtpDisconnected);
        connect(ftpClient_, &IFtpClient::connected, this,
                &RemoteListingCoordinator::connectionEstablished);
    }
}

bool RemoteListingCoordinator::requestListing(const QString &path)
{
    if (pendingPaths_.contains(path)) {
        qCDebug(LogFileOps) << "Coordinator: requestListing already pending for" << path;
        return false;
    }
    if (!ftpClient_) {
        qCWarning(LogFileOps) << "Coordinator: requestListing called with no FTP client";
        return false;
    }
    pendingPaths_.insert(path);
    requestedListings_.insert(path);
    ftpClient_->list(path);
    return true;
}

void RemoteListingCoordinator::cancelPending()
{
    pendingPaths_.clear();
    requestedListings_.clear();
}

void RemoteListingCoordinator::cancelPath(const QString &path)
{
    pendingPaths_.remove(path);
    requestedListings_.remove(path);
}

void RemoteListingCoordinator::onDirectoryListed(const QString &path,
                                                 const QList<FtpEntry> &entries)
{
    qCDebug(LogFileOps) << "Coordinator: onDirectoryListed path:" << path;

    if (!requestedListings_.contains(path)) {
        qCDebug(LogFileOps) << "Coordinator: Ignoring listing for path we didn't request:" << path;
        return;
    }
    requestedListings_.remove(path);
    pendingPaths_.remove(path);

    emit listingReady(path, entries);
}

void RemoteListingCoordinator::onFtpOperationFailed(IFtpClient::Operation operation,
                                                    const QString &remotePath,
                                                    const QString & /*localPath*/,
                                                    const QString &message)
{
    if (operation != IFtpClient::Operation::List || !requestedListings_.contains(remotePath)) {
        return;  // Another component's request on the shared client
    }
    qCDebug(LogFileOps) << "Coordinator: listing failed for" << remotePath << ":" << message;
    requestedListings_.remove(remotePath);
    pendingPaths_.remove(remotePath);
    emit listingFailed(remotePath, message);
}

void RemoteListingCoordinator::onFtpError(const QString &message)
{
    // Failures of single requests are matched through operationFailed(); an error
    // that leaves the client disconnected (socket error, timeout) drops everything
    if (ftpClient_ && ftpClient_->isConnected()) {
        return;
    }
    qCDebug(LogFileOps) << "Coordinator: connection-level error:" << message;
    abortPendingListings();
}

void RemoteListingCoordinator::onFtpDisconnected()
{
    abortPendingListings();
}

void RemoteListingCoordinator::abortPendingListings()
{
    if (requestedListings_.isEmpty() && pendingPaths_.isEmpty()) {
        return;
    }
    pendingPaths_.clear();
    requestedListings_.clear();
    emit listingsAborted();
}
