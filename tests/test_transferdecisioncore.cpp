#include "core/transferdecisioncore.h"

#include <QTest>

namespace {

transfer::State makeStateWithItem(transfer::OperationType opType,
                                  transfer::TransferItem::Status status, int batchId = 1)
{
    transfer::State state;
    transfer::TransferBatch batch;
    batch.batchId = batchId;
    batch.operationType = opType;
    batch.scanned = true;
    batch.folderConfirmed = true;
    transfer::TransferItem item;
    item.status = status;
    item.batchId = batchId;
    item.operationType = opType;
    batch.items.append(item);
    state.items.append(item);
    state.batches.append(batch);
    return state;
}

}  // namespace

class TestTransferDecisionCore : public QObject
{
    Q_OBJECT

private slots:
    void testDecideNextAction_blockedWhenTransferring()
    {
        auto state = makeStateWithItem(transfer::OperationType::Download,
                                       transfer::TransferItem::Status::Pending);
        state.queueState = transfer::QueueState::Transferring;

        auto decision =
            transfer::decideNextAction(state, true, [](const QString &) { return false; });

        QCOMPARE(decision.action, transfer::ProcessNextAction::Blocked);
    }

    void testDecideNextAction_noFtpClientWhenDisconnected()
    {
        auto state = makeStateWithItem(transfer::OperationType::Download,
                                       transfer::TransferItem::Status::Pending);
        state.queueState = transfer::QueueState::Idle;

        auto decision =
            transfer::decideNextAction(state, false, [](const QString &) { return false; });

        QCOMPARE(decision.action, transfer::ProcessNextAction::NoFtpClient);
    }

    void testDecideNextAction_startFolderOpWhenPendingFolderOps()
    {
        transfer::State state;
        state.queueState = transfer::QueueState::Idle;
        state.currentFolderOp.batchId = -1;
        transfer::PendingFolderOp op;
        op.batchId = 2;
        op.operationType = transfer::OperationType::Download;
        state.pendingFolderOps.enqueue(op);

        auto decision =
            transfer::decideNextAction(state, true, [](const QString &) { return false; });

        QCOMPARE(decision.action, transfer::ProcessNextAction::StartFolderOp);
        QCOMPARE(decision.folderOpToStart.batchId, 2);
    }

    void testDecideNextAction_needOverwriteCheckDownload_localFileExists()
    {
        auto state = makeStateWithItem(transfer::OperationType::Download,
                                       transfer::TransferItem::Status::Pending);
        state.queueState = transfer::QueueState::Idle;
        state.items[0].localPath = "/local/file.prg";
        state.overwriteAll = false;
        state.items[0].confirmed = false;

        auto decision =
            transfer::decideNextAction(state, true, [](const QString &) { return true; });

        QCOMPARE(decision.action, transfer::ProcessNextAction::NeedOverwriteCheck_Download);
    }

    void testDecideNextAction_needOverwriteCheckUpload_whenUploadPending()
    {
        auto state = makeStateWithItem(transfer::OperationType::Upload,
                                       transfer::TransferItem::Status::Pending);
        state.queueState = transfer::QueueState::Idle;
        state.items[0].remotePath = "/remote/dir/file.prg";
        state.overwriteAll = false;
        state.items[0].confirmed = false;

        auto decision =
            transfer::decideNextAction(state, true, [](const QString &) { return false; });

        QCOMPARE(decision.action, transfer::ProcessNextAction::NeedOverwriteCheck_Upload);
    }

    void testDecideNextAction_startTransferWhenConfirmed()
    {
        auto state = makeStateWithItem(transfer::OperationType::Download,
                                       transfer::TransferItem::Status::Pending);
        state.queueState = transfer::QueueState::Idle;
        state.items[0].localPath = "/local/file.prg";
        state.items[0].confirmed = true;

        auto decision =
            transfer::decideNextAction(state, true, [](const QString &) { return true; });

        QCOMPARE(decision.action, transfer::ProcessNextAction::StartTransfer);
    }

    void testDecideNextAction_noPendingWhenEmpty()
    {
        transfer::State state;
        state.queueState = transfer::QueueState::Idle;

        auto decision =
            transfer::decideNextAction(state, true, [](const QString &) { return false; });

        QCOMPARE(decision.action, transfer::ProcessNextAction::NoPending);
    }

    void testHandleFtpError_deleteError_incrementsDeletedCount()
    {
        auto state = makeStateWithItem(transfer::OperationType::Delete,
                                       transfer::TransferItem::Status::InProgress);
        state.queueState = transfer::QueueState::Deleting;
        transfer::DeleteItem di;
        di.path = "/remote/file.prg";
        di.isDirectory = false;
        state.deleteQueue.append(di);
        state.deletedCount = 0;

        auto result = transfer::handleFtpError(state, "delete failed");

        QVERIFY(result.isDeleteError);
        QVERIFY(result.shouldProcessNextDelete);
        QCOMPARE(result.newState.deletedCount, 1);
    }

    void testHandleFtpError_transferError_marksItemFailed()
    {
        auto state = makeStateWithItem(transfer::OperationType::Download,
                                       transfer::TransferItem::Status::InProgress);
        state.queueState = transfer::QueueState::Idle;
        state.currentIndex = 0;

        auto result = transfer::handleFtpError(state, "transfer failed");

        QVERIFY(!result.isDeleteError);
        QVERIFY(result.hasCurrentItem);
        QCOMPARE(result.newState.items[0].status, transfer::TransferItem::Status::Failed);
        QCOMPARE(result.newState.items[0].errorMessage, QString("transfer failed"));
        QVERIFY(result.shouldScheduleProcessNext);
    }

    void testHandleFtpError_clearsRequestedListings()
    {
        auto state = makeStateWithItem(transfer::OperationType::Download,
                                       transfer::TransferItem::Status::InProgress);
        state.requestedListings.insert("/some/path");
        state.requestedDeleteListings.insert("/delete/path");
        state.currentIndex = 0;

        auto result = transfer::handleFtpError(state, "error");

        QVERIFY(result.newState.requestedListings.isEmpty());
        QVERIFY(result.newState.requestedDeleteListings.isEmpty());
    }

    void testHandleOperationTimeout_findsInProgressItemAndMarksFailed()
    {
        auto state = makeStateWithItem(transfer::OperationType::Download,
                                       transfer::TransferItem::Status::InProgress);

        auto newState = transfer::handleOperationTimeout(state);

        QCOMPARE(newState.items[0].status, transfer::TransferItem::Status::Failed);
        QCOMPARE(newState.items[0].errorMessage, QString("Operation timed out"));
        QCOMPARE(newState.currentIndex, -1);
        QCOMPARE(newState.queueState, transfer::QueueState::Idle);
    }

    void testHandleOperationTimeout_noChangeWhenNoInProgress()
    {
        auto state = makeStateWithItem(transfer::OperationType::Download,
                                       transfer::TransferItem::Status::Pending);

        auto newState = transfer::handleOperationTimeout(state);

        QCOMPARE(newState.items[0].status, transfer::TransferItem::Status::Pending);
        QCOMPARE(newState.queueState, transfer::QueueState::Idle);
    }

    void testDecideNextDeleteAction_dispatchNextWhenQueueNotExhausted()
    {
        transfer::State state;
        transfer::DeleteItem di;
        di.path = "/remote/file.prg";
        di.isDirectory = false;
        state.deleteQueue.append(di);
        state.deletedCount = 0;

        auto decision = transfer::decideNextDeleteAction(state);

        QCOMPARE(decision.action, transfer::NextDeleteAction::DispatchNext);
        QCOMPARE(decision.nextItem.path, QString("/remote/file.prg"));
    }

    void testDecideNextDeleteAction_allDoneWhenExhaustedNoPendingUpload()
    {
        transfer::State state;
        state.deletedCount = 1;
        transfer::DeleteItem di;
        di.path = "/remote/file.prg";
        state.deleteQueue.append(di);
        state.pendingUploadAfterDelete = false;

        auto decision = transfer::decideNextDeleteAction(state);

        QCOMPARE(decision.action, transfer::NextDeleteAction::AllDone);
        QCOMPARE(decision.completedCount, 1);
    }

    void testDecideNextDeleteAction_pendingUploadReadyWhenExhaustedWithPendingUpload()
    {
        transfer::State state;
        state.deletedCount = 1;
        transfer::DeleteItem di;
        di.path = "/remote/file.prg";
        state.deleteQueue.append(di);
        state.pendingUploadAfterDelete = true;

        auto decision = transfer::decideNextDeleteAction(state);

        QCOMPARE(decision.action, transfer::NextDeleteAction::PendingUploadReady);
    }
};

QTEST_MAIN(TestTransferDecisionCore)
#include "test_transferdecisioncore.moc"
