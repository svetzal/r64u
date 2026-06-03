/**
 * @file transferdispatchhandler.h
 * @brief Handler that owns the processNext() dispatch loop for the transfer queue.
 *
 * TransferDispatchHandler reads the current transfer::State, calls
 * transfer::decideNextAction, and dispatches to the appropriate collaborator
 * (FTP client, folder coordinator).  It emits signals for every side-effect so
 * that TransferManager can remain the single owner of shared resources while
 * keeping the dispatch logic isolated and independently testable.
 */

#ifndef TRANSFERDISPATCHHANDLER_H
#define TRANSFERDISPATCHHANDLER_H

#include "core/transfercore.h"

#include <QObject>
#include <QPointer>

class IFtpClient;
class ILocalFileSystemService;
class TransferTimeoutManager;
class FolderOperationCoordinator;

using transfer::OperationType;
using transfer::QueueState;

/**
 * @brief Owns the processNext() dispatch loop and transitionTo() state setter.
 *
 * The handler holds a non-owning reference to transfer::State (owned by
 * TransferManager) and must never outlive it.  All side-effects are emitted as
 * signals so TransferManager can wire them to the appropriate collaborators.
 */
class TransferDispatchHandler : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the handler.
     * @param state Non-owning reference to the shared transfer state.
     * @param parent Qt parent for memory management.
     */
    explicit TransferDispatchHandler(transfer::State &state, QObject *parent = nullptr);

    void setFtpClient(IFtpClient *client);
    void setLocalFileSystem(ILocalFileSystemService *fs);
    void setTimeoutManager(TransferTimeoutManager *manager);
    void setFolderCoordinator(FolderOperationCoordinator *coordinator);

public slots:
    /// Inspect state, decide the next action, and dispatch to the appropriate collaborator.
    void processNext();

    /// Transition the state machine to @p newState (no-op if already in that state).
    void transitionTo(QueueState newState);

signals:
    void operationStarted(const QString &fileName, OperationType type);
    void itemDataChanged(int row);
    /// @param timeout Status-bar timeout in milliseconds; 0 means permanent.
    void statusMessage(const QString &message, int timeout = 0);
    void operationFailed(const QString &fileName, const QString &error);
    void overwriteConfirmationNeeded(const QString &fileName, OperationType type);
    void allOperationsCompleted();

    /// Request TransferManager to start the operation timeout timer.
    void startTimeoutRequested();
    /// Request TransferManager to stop the operation timeout timer.
    void stopTimeoutRequested();
    /// Request TransferManager to schedule a processNext() call via the event queue.
    void scheduleProcessNextRequested();

private:
    transfer::State &state_;
    QPointer<IFtpClient> ftpClient_;
    ILocalFileSystemService *localFs_ = nullptr;
    FolderOperationCoordinator *folderCoordinator_ = nullptr;
};

#endif  // TRANSFERDISPATCHHANDLER_H
