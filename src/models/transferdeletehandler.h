#ifndef TRANSFERDELETEHANDLER_H
#define TRANSFERDELETEHANDLER_H

#include "services/iftpclient.h"
#include "services/transfercore.h"

#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>

class RecursiveScanCoordinator;
class RemoteDirectoryCoordinator;

using transfer::OperationType;
using transfer::QueueState;

/**
 * @brief Handles enqueue and dispatch of delete operations.
 *
 * Encapsulates enqueueDelete(), enqueueRecursiveDelete(), and processNextDelete()
 * that were previously part of TransferManager. Operates on the shared
 * transfer::State by reference, and signals back to the orchestrator for
 * model notifications, state transitions, and scheduling.
 */
class TransferDeleteHandler : public QObject
{
    Q_OBJECT

public:
    using CreateBatchFn =
        std::function<int(OperationType, const QString &, const QString &, const QString &)>;
    using NotifyInsertFn = std::function<void(int, int)>;
    using VoidFn = std::function<void()>;

    explicit TransferDeleteHandler(transfer::State &state, QObject *parent = nullptr);

    void setFtpClient(IFtpClient *client);
    void setScanCoordinator(RecursiveScanCoordinator *coordinator);
    void setDirCreator(RemoteDirectoryCoordinator *creator);

    /// Callbacks for orchestrator operations that cannot be signalled.
    void setCreateBatchCallback(CreateBatchFn fn);
    void setBeginInsertCallback(NotifyInsertFn fn);
    void setEndInsertCallback(VoidFn fn);
    void setTransitionToCallback(std::function<void(QueueState)> fn);
    void setScheduleProcessNextCallback(VoidFn fn);

    void enqueueDelete(const QString &remotePath, bool isDirectory);
    void enqueueRecursiveDelete(const QString &remotePath);
    void processNextDelete();

signals:
    void operationFailed(const QString &fileName, const QString &error);
    void operationCompleted(const QString &message);
    void allOperationsCompleted();
    void statusMessage(const QString &message, int timeout = 0);
    void queueChanged();
    void batchStarted(int batchId);
    void startDirectoryCreationAfterDeleteRequested();

private:
    transfer::State &state_;
    QPointer<IFtpClient> ftpClient_;
    RecursiveScanCoordinator *scanCoordinator_ = nullptr;
    RemoteDirectoryCoordinator *dirCreator_ = nullptr;

    CreateBatchFn createBatchCb_;
    NotifyInsertFn beginInsertCb_;
    VoidFn endInsertCb_;
    std::function<void(QueueState)> transitionToCb_;
    VoidFn scheduleProcessNextCb_;
};

#endif  // TRANSFERDELETEHANDLER_H
