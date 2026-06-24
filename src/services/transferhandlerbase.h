#ifndef TRANSFERHANDLERBASE_H
#define TRANSFERHANDLERBASE_H

#include "core/transfercore.h"
#include "services/iftpclient.h"

#include <QObject>
#include <QPointer>

/**
 * @brief Shared base for transfer handler classes.
 *
 * Holds the common `state_` reference and `ftpClient_` pointer, provides a
 * virtual `setFtpClient()` that disconnects the old client before storing the
 * new one (via a `connectFtpSignals()` hook for subclass signal wiring), and
 * exposes shared helpers used across the handler layer.
 */
class TransferHandlerBase : public QObject
{
    Q_OBJECT

public:
    explicit TransferHandlerBase(transfer::State &state, QObject *parent = nullptr);

    /**
     * @brief Disconnects the previous FTP client, stores the new one, and calls
     *        connectFtpSignals() so subclasses can re-establish their signal wiring.
     */
    virtual void setFtpClient(IFtpClient *client);

signals:
    void operationFailed(const QString &fileName, const QString &error);
    void queueChanged();
    void scheduleProcessNextRequested();
    void itemDataChanged(int row);
    void batchProgressRequested(int batchId, bool isComplete, bool includeFailed);

protected:
    /**
     * @brief Called by setFtpClient() after storing the new client.
     *
     * Override to connect subclass-specific FTP client signals.
     * Default implementation is a no-op.
     */
    virtual void connectFtpSignals() {}

    /**
     * @brief Returns true if the FTP client is set and reports connected.
     */
    [[nodiscard]] bool ftpConnected() const;

    /**
     * @brief Emits queueChanged() followed by scheduleProcessNextRequested().
     */
    void notifyQueueChanged();

    /**
     * @brief Marks the item at `state_.currentIndex` as complete and emits
     *        itemDataChanged and (if applicable) batchProgressRequested.
     */
    void markCurrentComplete(transfer::TransferItem::Status status);

    /**
     * @brief Calls transfer::completeTransferOperation on state_ then notifyQueueChanged().
     */
    void completeAndNotify();

    transfer::State &state_;
    QPointer<IFtpClient> ftpClient_;
};

#endif  // TRANSFERHANDLERBASE_H
