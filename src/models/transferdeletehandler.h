#ifndef TRANSFERDELETEHANDLER_H
#define TRANSFERDELETEHANDLER_H

#include "services/iftpclient.h"
#include "core/transfercore.h"

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
 * transfer::State by reference. Emits Qt signals for model notifications
 * (rowsAboutToBeInserted / rowsInserted) and orchestration callbacks
 * (transitionToRequested, scheduleProcessNextRequested) that TransferManager
 * connects to. The createBatch factory delegate is kept as std::function because
 * it returns an int and acts as a factory, not a notification.
 */
class TransferDeleteHandler : public QObject
{
    Q_OBJECT

public:
    using CreateBatchFn =
        std::function<int(OperationType, const QString &, const QString &, const QString &)>;

    explicit TransferDeleteHandler(transfer::State &state, QObject *parent = nullptr);

    void setFtpClient(IFtpClient *client);
    void setScanCoordinator(RecursiveScanCoordinator *coordinator);
    void setDirCreator(RemoteDirectoryCoordinator *creator);

    /// Factory callback: kept as std::function because it returns an int (factory pattern).
    void setCreateBatchCallback(CreateBatchFn fn);

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

    /// Model-notification signals for row insertions during enqueueDelete
    void rowsAboutToBeInserted(int first, int last);
    void rowsInserted();

    /// Orchestration signals — connect to TransferManager private slots
    void transitionToRequested(QueueState newState);
    void scheduleProcessNextRequested();

private:
    transfer::State &state_;
    QPointer<IFtpClient> ftpClient_;
    RecursiveScanCoordinator *scanCoordinator_ = nullptr;
    RemoteDirectoryCoordinator *dirCreator_ = nullptr;

    CreateBatchFn createBatchCb_;
};

#endif  // TRANSFERDELETEHANDLER_H
