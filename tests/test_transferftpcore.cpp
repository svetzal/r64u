#include "core/transferftpcore.h"

#include <QTest>

class TestTransferFtpCore : public QObject
{
    Q_OBJECT

private slots:
    void testCompleteTransferOperation_transferring_transitionsToIdle()
    {
        transfer::State state;
        state.queueState = transfer::QueueState::Transferring;

        auto result = transfer::completeTransferOperation(state);

        QVERIFY(result.transitionedToIdle);
        QCOMPARE(result.newState.queueState, transfer::QueueState::Idle);
    }

    void testCompleteTransferOperation_alreadyIdle_noTransition()
    {
        transfer::State state;
        state.queueState = transfer::QueueState::Idle;

        auto result = transfer::completeTransferOperation(state);

        QVERIFY(!result.transitionedToIdle);
        QCOMPARE(result.newState.queueState, transfer::QueueState::Idle);
    }

    void testCompleteTransferOperation_deleting_noTransition()
    {
        transfer::State state;
        state.queueState = transfer::QueueState::Deleting;

        auto result = transfer::completeTransferOperation(state);

        QVERIFY(!result.transitionedToIdle);
        QCOMPARE(result.newState.queueState, transfer::QueueState::Deleting);
    }

    void testAdvanceDeleteProgress_matchingPath_advances()
    {
        transfer::State state;
        state.queueState = transfer::QueueState::Deleting;
        transfer::DeleteItem di;
        di.path = "/remote/file.prg";
        di.isDirectory = false;
        state.deleteQueue.append(di);
        state.deletedCount = 0;

        auto result = transfer::advanceDeleteProgress(state, "/remote/file.prg");

        QVERIFY(result.advanced);
        QCOMPARE(result.newState.deletedCount, 1);
        QCOMPARE(result.fileName, QString("file.prg"));
        QCOMPARE(result.currentCount, 1);
        QCOMPARE(result.totalCount, 1);
    }

    void testAdvanceDeleteProgress_wrongPath_doesNotAdvance()
    {
        transfer::State state;
        state.queueState = transfer::QueueState::Deleting;
        transfer::DeleteItem di;
        di.path = "/remote/file.prg";
        state.deleteQueue.append(di);
        state.deletedCount = 0;

        auto result = transfer::advanceDeleteProgress(state, "/remote/other.prg");

        QVERIFY(!result.advanced);
        QCOMPARE(result.newState.deletedCount, 0);
    }

    void testAdvanceDeleteProgress_notDeletingState_doesNotAdvance()
    {
        transfer::State state;
        state.queueState = transfer::QueueState::Idle;
        transfer::DeleteItem di;
        di.path = "/remote/file.prg";
        state.deleteQueue.append(di);
        state.deletedCount = 0;

        auto result = transfer::advanceDeleteProgress(state, "/remote/file.prg");

        QVERIFY(!result.advanced);
    }

    void testAdvanceDeleteProgress_deletedCountExhausted_doesNotAdvance()
    {
        transfer::State state;
        state.queueState = transfer::QueueState::Deleting;
        transfer::DeleteItem di;
        di.path = "/remote/file.prg";
        state.deleteQueue.append(di);
        state.deletedCount = 1;

        auto result = transfer::advanceDeleteProgress(state, "/remote/file.prg");

        QVERIFY(!result.advanced);
    }

    void testFindInProgressDeleteItem_found()
    {
        transfer::State state;
        transfer::TransferItem item;
        item.operationType = transfer::OperationType::Delete;
        item.remotePath = "/remote/file.prg";
        item.status = transfer::TransferItem::Status::InProgress;
        state.items.append(item);

        auto result = transfer::findInProgressDeleteItem(state, "/remote/file.prg");

        QVERIFY(result.found);
        QCOMPARE(result.itemIndex, 0);
        QCOMPARE(result.fileName, QString("file.prg"));
    }

    void testFindInProgressDeleteItem_notFoundWhenPending()
    {
        transfer::State state;
        transfer::TransferItem item;
        item.operationType = transfer::OperationType::Delete;
        item.remotePath = "/remote/file.prg";
        item.status = transfer::TransferItem::Status::Pending;
        state.items.append(item);

        auto result = transfer::findInProgressDeleteItem(state, "/remote/file.prg");

        QVERIFY(!result.found);
    }

    void testFindInProgressDeleteItem_notFoundWhenDifferentPath()
    {
        transfer::State state;
        transfer::TransferItem item;
        item.operationType = transfer::OperationType::Delete;
        item.remotePath = "/remote/other.prg";
        item.status = transfer::TransferItem::Status::InProgress;
        state.items.append(item);

        auto result = transfer::findInProgressDeleteItem(state, "/remote/file.prg");

        QVERIFY(!result.found);
    }

    void testFindInProgressDeleteItem_notFoundWhenWrongOperationType()
    {
        transfer::State state;
        transfer::TransferItem item;
        item.operationType = transfer::OperationType::Download;
        item.remotePath = "/remote/file.prg";
        item.status = transfer::TransferItem::Status::InProgress;
        state.items.append(item);

        auto result = transfer::findInProgressDeleteItem(state, "/remote/file.prg");

        QVERIFY(!result.found);
    }

    void testEnqueueItem_appendsToStateAndBatch()
    {
        transfer::State state;
        transfer::TransferBatch batch;
        batch.batchId = 1;
        state.batches.append(batch);

        transfer::TransferItem item;
        item.remotePath = "/remote/file.prg";
        item.operationType = transfer::OperationType::Download;

        int originalSize = state.items.size();
        auto result = transfer::enqueueItem(state, item, 0);

        QCOMPARE(result.newState.items.size(), originalSize + 1);
        QCOMPARE(result.newState.batches[0].items.size(), 1);
        QCOMPARE(result.insertedRow, originalSize);
    }
};

QTEST_MAIN(TestTransferFtpCore)
#include "test_transferftpcore.moc"
