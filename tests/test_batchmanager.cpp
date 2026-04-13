#include "models/batchmanager.h"
#include "services/transfercore.h"

#include <QSignalSpy>
#include <QTest>

namespace {

/// Build a state with one batch (batchId = @p batchId) that has one item
/// with the given status.  The batch is scanned + folderConfirmed so that
/// isComplete() returns true when completedCount == 1.
transfer::State makeCompletedBatchState(int batchId)
{
    transfer::State state;

    transfer::TransferBatch batch;
    batch.batchId = batchId;
    batch.scanned = true;
    batch.folderConfirmed = true;
    batch.completedCount = 1;

    transfer::TransferItem item;
    item.batchId = batchId;
    item.status = transfer::TransferItem::Status::Completed;
    batch.items.append(item);

    state.batches.append(batch);
    state.items.append(item);
    state.activeBatchIndex = 0;
    state.queueState = transfer::QueueState::Transferring;
    state.currentIndex = 0;

    return state;
}

}  // namespace

class TestBatchManager : public QObject
{
    Q_OBJECT

private slots:
    // completeBatch tests
    void testCompleteBatch_resetsState();
    void testCompleteBatch_callsStopTimeoutCallback();
    void testCompleteBatch_emitsBatchCompleted();
    void testCompleteBatch_emitsAllOperationsCompleted_whenNoBatchesRemain();
    void testCompleteBatch_unknownBatchId_noEffect();
    void testCompleteBatch_folderOperation_callsFolderOpCompleteCallback();
    void testCompleteBatch_folderOperation_doesNotEmitAllOperationsCompleted();
    void testCompleteBatch_withNextBatch_emitsBatchStarted();
    void testCompleteBatch_withNextBatch_callsScheduleNextCallback();
    void testCompleteBatch_withNextBatch_doesNotEmitAllOperationsCompleted();

    // purgeBatch tests
    void testPurgeBatch_removesItemsAndCallsModelSignals();
    void testPurgeBatch_adjustsCurrentIndex();
    void testPurgeBatch_adjustsActiveBatchIndex_whenActiveBatchPurged();
    void testPurgeBatch_nonExistentBatch_noEffect();
    void testPurgeBatch_emitsQueueChanged();
};

// ============================================================================
// completeBatch tests
// ============================================================================

void TestBatchManager::testCompleteBatch_resetsState()
{
    transfer::State state = makeCompletedBatchState(1);
    BatchManager bm(state);

    bm.completeBatch(1);

    QCOMPARE(state.activeBatchIndex, -1);
    QCOMPARE(state.currentIndex, -1);
    QCOMPARE(state.queueState, transfer::QueueState::Idle);
}

void TestBatchManager::testCompleteBatch_callsStopTimeoutCallback()
{
    transfer::State state = makeCompletedBatchState(1);
    BatchManager bm(state);

    bool called = false;
    bm.setStopTimeoutCallback([&called]() { called = true; });

    bm.completeBatch(1);

    QVERIFY(called);
}

void TestBatchManager::testCompleteBatch_emitsBatchCompleted()
{
    transfer::State state = makeCompletedBatchState(1);
    BatchManager bm(state);

    QSignalSpy spy(&bm, &BatchManager::batchCompleted);
    bm.completeBatch(1);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy[0][0].toInt(), 1);
}

void TestBatchManager::testCompleteBatch_emitsAllOperationsCompleted_whenNoBatchesRemain()
{
    transfer::State state = makeCompletedBatchState(1);
    BatchManager bm(state);

    QSignalSpy spy(&bm, &BatchManager::allOperationsCompleted);
    bm.completeBatch(1);

    QCOMPARE(spy.count(), 1);
}

void TestBatchManager::testCompleteBatch_unknownBatchId_noEffect()
{
    transfer::State state = makeCompletedBatchState(1);
    BatchManager bm(state);

    QSignalSpy completedSpy(&bm, &BatchManager::batchCompleted);
    QSignalSpy allDoneSpy(&bm, &BatchManager::allOperationsCompleted);

    bm.completeBatch(999);

    QCOMPARE(completedSpy.count(), 0);
    QCOMPARE(allDoneSpy.count(), 0);
    // State should be unchanged
    QCOMPARE(state.activeBatchIndex, 0);
}

void TestBatchManager::testCompleteBatch_folderOperation_callsFolderOpCompleteCallback()
{
    transfer::State state = makeCompletedBatchState(5);
    state.currentFolderOp.batchId = 5;
    BatchManager bm(state);

    bool folderOpCalled = false;
    bm.setFolderOpCompleteCallback([&folderOpCalled]() { folderOpCalled = true; });

    bm.completeBatch(5);

    QVERIFY(folderOpCalled);
}

void TestBatchManager::testCompleteBatch_folderOperation_doesNotEmitAllOperationsCompleted()
{
    transfer::State state = makeCompletedBatchState(5);
    state.currentFolderOp.batchId = 5;
    BatchManager bm(state);

    QSignalSpy spy(&bm, &BatchManager::allOperationsCompleted);
    bm.completeBatch(5);

    QCOMPARE(spy.count(), 0);
}

void TestBatchManager::testCompleteBatch_withNextBatch_emitsBatchStarted()
{
    transfer::State state;

    // Batch 1: complete (being completed now)
    transfer::TransferBatch b1;
    b1.batchId = 1;
    b1.scanned = true;
    b1.folderConfirmed = true;
    b1.completedCount = 1;
    transfer::TransferItem item1;
    item1.batchId = 1;
    item1.status = transfer::TransferItem::Status::Completed;
    b1.items.append(item1);

    // Batch 2: pending (should be activated next)
    transfer::TransferBatch b2;
    b2.batchId = 2;
    b2.scanned = true;
    b2.folderConfirmed = true;
    transfer::TransferItem item2;
    item2.batchId = 2;
    item2.status = transfer::TransferItem::Status::Pending;
    b2.items.append(item2);

    state.batches = {b1, b2};
    state.items = {item1, item2};
    state.activeBatchIndex = 0;
    state.queueState = transfer::QueueState::Transferring;

    BatchManager bm(state);
    QSignalSpy startedSpy(&bm, &BatchManager::batchStarted);
    bm.completeBatch(1);

    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(startedSpy[0][0].toInt(), 2);
}

void TestBatchManager::testCompleteBatch_withNextBatch_callsScheduleNextCallback()
{
    transfer::State state;

    transfer::TransferBatch b1;
    b1.batchId = 1;
    b1.scanned = true;
    b1.folderConfirmed = true;
    b1.completedCount = 1;
    transfer::TransferItem item1;
    item1.batchId = 1;
    item1.status = transfer::TransferItem::Status::Completed;
    b1.items.append(item1);

    transfer::TransferBatch b2;
    b2.batchId = 2;
    b2.scanned = true;
    b2.folderConfirmed = true;
    transfer::TransferItem item2;
    item2.batchId = 2;
    item2.status = transfer::TransferItem::Status::Pending;
    b2.items.append(item2);

    state.batches = {b1, b2};
    state.items = {item1, item2};
    state.activeBatchIndex = 0;
    state.queueState = transfer::QueueState::Transferring;

    BatchManager bm(state);

    bool scheduleCalled = false;
    bm.setScheduleProcessNextCallback([&scheduleCalled]() { scheduleCalled = true; });

    bm.completeBatch(1);

    QVERIFY(scheduleCalled);
}

void TestBatchManager::testCompleteBatch_withNextBatch_doesNotEmitAllOperationsCompleted()
{
    transfer::State state;

    transfer::TransferBatch b1;
    b1.batchId = 1;
    b1.scanned = true;
    b1.folderConfirmed = true;
    b1.completedCount = 1;
    transfer::TransferItem item1;
    item1.batchId = 1;
    item1.status = transfer::TransferItem::Status::Completed;
    b1.items.append(item1);

    transfer::TransferBatch b2;
    b2.batchId = 2;
    b2.scanned = true;
    b2.folderConfirmed = true;
    transfer::TransferItem item2;
    item2.batchId = 2;
    item2.status = transfer::TransferItem::Status::Pending;
    b2.items.append(item2);

    state.batches = {b1, b2};
    state.items = {item1, item2};
    state.activeBatchIndex = 0;
    state.queueState = transfer::QueueState::Transferring;

    BatchManager bm(state);
    QSignalSpy spy(&bm, &BatchManager::allOperationsCompleted);
    bm.completeBatch(1);

    QCOMPARE(spy.count(), 0);
}

// ============================================================================
// purgeBatch tests
// ============================================================================

void TestBatchManager::testPurgeBatch_removesItemsAndCallsModelSignals()
{
    transfer::State state = makeCompletedBatchState(1);
    BatchManager bm(state);

    QList<QPair<int, int>> beginCalls;
    int endCallCount = 0;
    bm.setRowRemovalCallbacks(
        [&beginCalls](int first, int last) { beginCalls.append({first, last}); },
        [&endCallCount]() { ++endCallCount; });

    bm.purgeBatch(1);

    QCOMPARE(state.items.size(), 0);
    QCOMPARE(state.batches.size(), 0);
    QCOMPARE(beginCalls.size(), 1);
    QCOMPARE(endCallCount, 1);
    // Signal was for row 0
    QCOMPARE(beginCalls[0].first, 0);
    QCOMPARE(beginCalls[0].second, 0);
}

void TestBatchManager::testPurgeBatch_adjustsCurrentIndex()
{
    transfer::State state = makeCompletedBatchState(1);
    state.currentIndex = 0;
    BatchManager bm(state);
    bm.setRowRemovalCallbacks([](int, int) {}, []() {});

    bm.purgeBatch(1);

    QCOMPARE(state.currentIndex, -1);
}

void TestBatchManager::testPurgeBatch_adjustsActiveBatchIndex_whenActiveBatchPurged()
{
    transfer::State state = makeCompletedBatchState(1);
    state.activeBatchIndex = 0;
    BatchManager bm(state);
    bm.setRowRemovalCallbacks([](int, int) {}, []() {});

    bm.purgeBatch(1);

    QCOMPARE(state.activeBatchIndex, -1);
}

void TestBatchManager::testPurgeBatch_nonExistentBatch_noEffect()
{
    transfer::State state = makeCompletedBatchState(1);
    BatchManager bm(state);

    int beginCallCount = 0;
    bm.setRowRemovalCallbacks([&beginCallCount](int, int) { ++beginCallCount; }, []() {});

    bm.purgeBatch(999);

    QCOMPARE(state.items.size(), 1);
    QCOMPARE(state.batches.size(), 1);
    QCOMPARE(beginCallCount, 0);
}

void TestBatchManager::testPurgeBatch_emitsQueueChanged()
{
    transfer::State state = makeCompletedBatchState(1);
    BatchManager bm(state);
    bm.setRowRemovalCallbacks([](int, int) {}, []() {});

    QSignalSpy spy(&bm, &BatchManager::queueChanged);
    bm.purgeBatch(1);

    QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(TestBatchManager)
#include "test_batchmanager.moc"
