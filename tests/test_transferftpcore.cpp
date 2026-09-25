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

    // --- Ownership of FTP client events ---

    static transfer::State stateWithInFlight(transfer::OperationType type)
    {
        transfer::State state;
        transfer::TransferItem item;
        item.operationType = type;
        item.remotePath = "/remote/a.prg";
        item.localPath = type == transfer::OperationType::Delete ? QString() : "/local/a.prg";
        item.status = transfer::TransferItem::Status::InProgress;
        state.items.append(item);
        state.currentIndex = 0;
        state.queueState = transfer::QueueState::Transferring;
        return state;
    }

    void testIsInFlightItem_matchesDispatchedDownload()
    {
        auto state = stateWithInFlight(transfer::OperationType::Download);

        QVERIFY(transfer::isInFlightItem(state, transfer::OperationType::Download, "/remote/a.prg",
                                         "/local/a.prg"));
        QVERIFY(!transfer::isInFlightItem(state, transfer::OperationType::Download, "/remote/a.prg",
                                          "/elsewhere/a.prg"));
        QVERIFY(!transfer::isInFlightItem(state, transfer::OperationType::Upload, "/remote/a.prg",
                                          "/local/a.prg"));
    }

    void testIsInFlightItem_ignoresLocalPathForDeletes()
    {
        auto state = stateWithInFlight(transfer::OperationType::Delete);

        QVERIFY(transfer::isInFlightItem(state, transfer::OperationType::Delete, "/remote/a.prg",
                                         QString()));
    }

    void testIsInFlightItem_falseWhenItemNoLongerInProgress()
    {
        auto state = stateWithInFlight(transfer::OperationType::Download);
        state.items[0].status = transfer::TransferItem::Status::Failed;

        QVERIFY(!transfer::isInFlightItem(state, transfer::OperationType::Download, "/remote/a.prg",
                                          "/local/a.prg"));
    }

    void testInFlightDownloadIndex_matchesRemotePathOnly()
    {
        auto state = stateWithInFlight(transfer::OperationType::Download);

        QCOMPARE(transfer::inFlightDownloadIndex(state, "/remote/a.prg"), 0);
        QCOMPARE(transfer::inFlightDownloadIndex(state, "/preview.sid"), -1);
        QCOMPARE(transfer::inFlightUploadIndex(state, "/local/a.prg"), -1);
    }

    void testInFlightUploadIndex_matchesLocalPathOnly()
    {
        auto state = stateWithInFlight(transfer::OperationType::Upload);

        QCOMPARE(transfer::inFlightUploadIndex(state, "/local/a.prg"), 0);
        QCOMPARE(transfer::inFlightUploadIndex(state, "/local/b.prg"), -1);
    }

    void testIsAwaitedListing_coversEveryListingTheQueueRequests()
    {
        transfer::State state;
        state.requestedListings.insert("/scan");
        state.requestedDeleteListings.insert("/delete");
        state.requestedFolderCheckListings.insert("/folder");
        state.requestedUploadFileCheckListings.insert("/upload");

        QVERIFY(transfer::isAwaitedListing(state, "/scan"));
        QVERIFY(transfer::isAwaitedListing(state, "/delete"));
        QVERIFY(transfer::isAwaitedListing(state, "/folder"));
        QVERIFY(transfer::isAwaitedListing(state, "/upload"));
        QVERIFY(!transfer::isAwaitedListing(state, "/browsed"));
    }

    void testIsAwaitedMkdir_onlyTheDirectoryBeingCreated()
    {
        transfer::State state;
        transfer::PendingMkdir first;
        first.remotePath = "/remote/a";
        transfer::PendingMkdir second;
        second.remotePath = "/remote/a/b";
        state.pendingMkdirs.enqueue(first);
        state.pendingMkdirs.enqueue(second);

        QVERIFY(!transfer::isAwaitedMkdir(state, "/remote/a"));  // not creating yet
        state.queueState = transfer::QueueState::CreatingDirectories;
        QVERIFY(transfer::isAwaitedMkdir(state, "/remote/a"));
        QVERIFY(!transfer::isAwaitedMkdir(state, "/remote/a/b"));
    }

    void testIsAwaitedRecursiveDelete_onlyTheEntryBeingRemoved()
    {
        transfer::State state;
        state.queueState = transfer::QueueState::Deleting;
        transfer::DeleteItem first;
        first.path = "/remote/dir/a";
        transfer::DeleteItem second;
        second.path = "/remote/dir";
        state.deleteQueue = {first, second};

        QVERIFY(transfer::isAwaitedRecursiveDelete(state, "/remote/dir/a"));
        QVERIFY(!transfer::isAwaitedRecursiveDelete(state, "/remote/dir"));
        state.deletedCount = 2;
        QVERIFY(!transfer::isAwaitedRecursiveDelete(state, "/remote/dir"));
    }
};

QTEST_MAIN(TestTransferFtpCore)
#include "test_transferftpcore.moc"
