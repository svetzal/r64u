#ifndef REMOTELISTINGCOORDINATOR_H
#define REMOTELISTINGCOORDINATOR_H

#include "ftp/ftpentry.h"
#include "services/iftpclient.h"

#include <QObject>
#include <QPointer>
#include <QSet>

/**
 * @brief Orchestrates lazy-load FTP directory listing requests.
 *
 * Owns the FTP client reference, deduplicates concurrent listing
 * requests for the same path, and filters out listings (and failures)
 * that were not initiated by this coordinator: the client is shared with
 * the transfer queue and previews.  The owning view-model connects to
 * listingReady() / listingFailed() / listingsAborted() to drive tree mutations.
 */
class RemoteListingCoordinator : public QObject
{
    Q_OBJECT

public:
    explicit RemoteListingCoordinator(QObject *parent = nullptr);

    /**
     * @brief Set or replace the FTP client.
     *
     * Disconnects signal connections to any previously-held client
     * and connects to @p client.  Pass nullptr to disconnect only.
     */
    void setFtpClient(IFtpClient *client);

    /**
     * @brief Returns true if an FTP client is currently set.
     */
    [[nodiscard]] bool hasFtpClient() const { return ftpClient_ != nullptr; }

    /**
     * @brief Returns true if the FTP client is set and logged in, so a listing
     *        requested now can succeed.
     */
    [[nodiscard]] bool isClientLoggedIn() const
    {
        return ftpClient_ != nullptr && ftpClient_->isLoggedIn();
    }

    /**
     * @brief Request a directory listing for @p path.
     *
     * If a listing for @p path is already in-flight this call is a
     * no-op and returns false.  Otherwise the request is issued to
     * the FTP client and true is returned.
     *
     * @return true if the request was issued; false if already pending
     *         (caller should treat its own state as already in-flight).
     */
    [[nodiscard]] bool requestListing(const QString &path);

    /**
     * @brief Cancel all in-flight listing requests.
     *
     * Clears the pending-paths and requested-listings sets.  The
     * owning model must reset its own node state after calling this.
     */
    void cancelPending();

    /**
     * @brief Cancel the in-flight listing for a single @p path.
     *
     * Used when refreshing a specific subtree: removes only the paths
     * belonging to that subtree rather than cancelling all requests.
     */
    void cancelPath(const QString &path);

signals:
    /**
     * @brief Emitted when a requested directory listing completes.
     * @param path    The directory path whose listing arrived.
     * @param entries The parsed directory entries.
     */
    void listingReady(const QString &path, const QList<FtpEntry> &entries);

    /**
     * @brief Emitted when a listing this coordinator requested fails.
     * @param path    The directory whose listing failed.
     * @param message The error message from the FTP client.
     */
    void listingFailed(const QString &path, const QString &message);

    /**
     * @brief Emitted when the connection is lost: every pending listing is dropped.
     *
     * The client reports the connection problem itself; this only tells the
     * owner that none of the pending listings will arrive.
     */
    void listingsAborted();

    /**
     * @brief Emitted when the client (re)connects: listings that failed before
     *        are worth requesting again.
     */
    void connectionEstablished();

private slots:
    void onDirectoryListed(const QString &path, const QList<FtpEntry> &entries);
    void onFtpOperationFailed(IFtpClient::Operation operation, const QString &remotePath,
                              const QString &localPath, const QString &message);
    void onFtpError(const QString &message);
    void onFtpDisconnected();

private:
    void abortPendingListings();

    QPointer<IFtpClient> ftpClient_;
    QSet<QString> pendingPaths_;
    QSet<QString> requestedListings_;
};

#endif  // REMOTELISTINGCOORDINATOR_H
