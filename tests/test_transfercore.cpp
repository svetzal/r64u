#include "services/transfercore.h"

#include <QTest>

// FtpEntry is included transitively via transfercore.h -> ftpentry.h

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

class TestTransferCore : public QObject
{
    Q_OBJECT

private slots:
    void testQueueStateToString_idle();
    void testQueueStateToString_collectingItems();
    void testQueueStateToString_awaitingFolderConfirm();
    void testQueueStateToString_scanning();
    void testQueueStateToString_awaitingFileConfirm();
    void testQueueStateToString_creatingDirectories();
    void testQueueStateToString_transferring();
    void testQueueStateToString_deleting();
    void testQueueStateToString_batchComplete();

    void testTransferBatchIsComplete_emptyScanned();
    void testTransferBatchIsComplete_hasPendingItem();
    void testTransferBatchIsComplete_notScanned();
    void testTransferBatchPendingCount();

    void testBatchProgressIsValid_validId();
    void testBatchProgressIsValid_invalidId();
    void testBatchProgressPendingItems();

    // normalizePath tests
    void testNormalizePath_noTrailingSlash();
    void testNormalizePath_singleTrailingSlash();
    void testNormalizePath_multipleTrailingSlashes();
    void testNormalizePath_rootPath();
    void testNormalizePath_emptyString();

    // pendingCount tests
    void testPendingCount_empty();
    void testPendingCount_mixedStatuses();

    // activeCount tests
    void testActiveCount_oneInProgress();

    // activeAndPendingCount tests
    void testActiveAndPendingCount_mixed();

    // canProcessNext tests
    void testCanProcessNext_idle();
    void testCanProcessNext_blocked_collectingItems();
    void testCanProcessNext_blocked_scanning();
    void testCanProcessNext_blocked_transferring();

    // findBatchIndex tests
    void testFindBatchIndex_found();
    void testFindBatchIndex_notFound();

    // findItemIndex tests
    void testFindItemIndex_found();
    void testFindItemIndex_notFound();

    // findNextPendingItem tests
    void testFindNextPendingItem_found();
    void testFindNextPendingItem_skipNonPending();
    void testFindNextPendingItem_empty();

    // isPathBeingTransferred tests
    void testIsPathBeingTransferred_inActiveBatch();
    void testIsPathBeingTransferred_inCompleteBatch();
    void testIsPathBeingTransferred_trailingSlash();
    void testIsPathBeingTransferred_wrongType();
    void testIsPathBeingTransferred_notFound();

    // hasActiveBatch tests
    void testHasActiveBatch_noActive();
    void testHasActiveBatch_withActive();

    // queuedBatchCount tests
    void testQueuedBatchCount_empty();
    void testQueuedBatchCount_someIncomplete();

    // BatchProgress computation tests
    void testComputeActiveBatchProgress_noActiveBatch();
    void testComputeActiveBatchProgress_withActiveBatch();
    void testComputeBatchProgress_found();
    void testComputeBatchProgress_notFound();

    // itemData tests
    void testItemData_displayRole_upload();
    void testItemData_displayRole_download();
    void testItemData_statusRole();
    void testItemData_progressRole_withTotal();
    void testItemData_progressRole_zeroTotal();

    // purgeBatch tests
    void testPurgeBatch_removesItems();
    void testPurgeBatch_adjustsCurrentIndex();
    void testPurgeBatch_adjustsActiveBatchIndex();
    void testPurgeBatch_nonExistentBatch_unchanged();

    // createBatch tests
    void testCreateBatch_assignsIncrementingId();
    void testCreateBatch_appendsToBatchList();
    void testCreateBatch_purgesCompletedBatches();
    void testCreateBatch_doesNotPurgeScanning();

    // activateNextBatch tests
    void testActivateNextBatch_findsFirstIncomplete();
    void testActivateNextBatch_skipsCompleteBatches();
    void testActivateNextBatch_noneAvailable();

    // completeBatch tests
    void testCompleteBatch_resetsActiveIndex();
    void testCompleteBatch_transitionsToIdle();
    void testCompleteBatch_detectsFolderOperation();
    void testCompleteBatch_activatesNext_whenAvailable();

    // markItemComplete tests
    void testMarkItemComplete_setsStatus();
    void testMarkItemComplete_completedSetsBytes();
    void testMarkItemComplete_incrementsCompletedCount();
    void testMarkItemComplete_incrementsFailedCount();
    void testMarkItemComplete_detectsBatchComplete();
    void testMarkItemComplete_resetsCurrentIndex();
    void testMarkItemComplete_invalidIndex_unchanged();

    // clearAll tests
    void testClearAll_emptiesEverything();
    void testClearAll_transitionsToIdle();

    // removeCompleted tests
    void testRemoveCompleted_keepsOnlyPending();
    void testRemoveCompleted_adjustsCurrentIndex();

    // cancelAllItems tests
    void testCancelAllItems_marksPendingAsFailed();
    void testCancelAllItems_marksInProgressAsFailed();
    void testCancelAllItems_leavesCompletedAlone();
    void testCancelAllItems_clearsBatches();

    // cancelBatch tests
    void testCancelBatch_marksItemsAsFailed();
    void testCancelBatch_detectsActiveBatch();
    void testCancelBatch_nonExistentBatch_unchanged();
    void testCancelBatch_activatesNextBatchAfterCancel();

    // respondToOverwrite tests
    void testRespondToOverwrite_overwrite_confirmsItem();
    void testRespondToOverwrite_overwriteAll_setsFlag();
    void testRespondToOverwrite_skip_marksSkipped();
    void testRespondToOverwrite_skip_updatesBatchCount();
    void testRespondToOverwrite_cancel_flagsCancelAll();
    void testRespondToOverwrite_wrongState_unchanged();

    // respondToFolderExists tests
    void testRespondToFolderExists_merge_dequeuesOp();
    void testRespondToFolderExists_replace_setsFlag();
    void testRespondToFolderExists_cancel_clearsFolderOps();
    void testRespondToFolderExists_wrongState_unchanged();

    // checkFolderConfirmation tests
    void testCheckFolderConfirmation_noExisting_startsOp();
    void testCheckFolderConfirmation_someExisting_needsConfirm();
    void testCheckFolderConfirmation_empty_goesIdle();

    // sortDeleteQueue tests
    void testSortDeleteQueue_filesBeforeDirectories();
    void testSortDeleteQueue_deepestDirectoriesFirst();
    void testSortDeleteQueue_empty();

    // processDirectoryListingForDownload tests
    void testProcessDirListingDownload_filesOnly();
    void testProcessDirListingDownload_directoriesOnly();
    void testProcessDirListingDownload_mixed();
    void testProcessDirListingDownload_correctPathConstruction();

    // processDirectoryListingForDelete tests
    void testProcessDirListingDelete_filesAddedToQueue();
    void testProcessDirListingDelete_subdirectoriesQueued();
    void testProcessDirListingDelete_parentDirectoryIncluded();

    // updateFolderExistence tests
    void testUpdateFolderExistence_marksExisting();
    void testUpdateFolderExistence_marksNonExisting();

    // checkUploadFileExists tests
    void testCheckUploadFileExists_fileExists();
    void testCheckUploadFileExists_fileDoesNotExist();
    void testCheckUploadFileExists_invalidCurrentIndex();

    // decideNextAction tests
    void testDecideNext_blocked_whenNotIdle();
    void testDecideNext_noFtpClient();
    void testDecideNext_startFolderOp_whenPending();
    void testDecideNext_overwriteCheck_download_fileExists();
    void testDecideNext_startTransfer_download();
    void testDecideNext_startTransfer_upload_confirmed();
    void testDecideNext_overwriteCheck_upload_notConfirmed();
    void testDecideNext_noPending();

    // handleFtpError tests
    void testHandleFtpError_duringDelete_skipsAndContinues();
    void testHandleFtpError_duringDirCreation();
    void testHandleFtpError_duringTransfer_marksItemFailed();
    void testHandleFtpError_clearsAllPendingRequests();

    // handleOperationTimeout tests
    void testHandleOperationTimeout_marksInProgressAsFailed();
    void testHandleOperationTimeout_resetsCurrentIndex();
    void testHandleOperationTimeout_transitionsToIdle();
};

void TestTransferCore::testQueueStateToString_idle()
{
    QCOMPARE(QString(transfer::queueStateToString(transfer::QueueState::Idle)), QString("Idle"));
}

void TestTransferCore::testQueueStateToString_collectingItems()
{
    QCOMPARE(QString(transfer::queueStateToString(transfer::QueueState::CollectingItems)),
             QString("CollectingItems"));
}

void TestTransferCore::testQueueStateToString_awaitingFolderConfirm()
{
    QCOMPARE(QString(transfer::queueStateToString(transfer::QueueState::AwaitingFolderConfirm)),
             QString("AwaitingFolderConfirm"));
}

void TestTransferCore::testQueueStateToString_scanning()
{
    QCOMPARE(QString(transfer::queueStateToString(transfer::QueueState::Scanning)),
             QString("Scanning"));
}

void TestTransferCore::testQueueStateToString_awaitingFileConfirm()
{
    QCOMPARE(QString(transfer::queueStateToString(transfer::QueueState::AwaitingFileConfirm)),
             QString("AwaitingFileConfirm"));
}

void TestTransferCore::testQueueStateToString_creatingDirectories()
{
    QCOMPARE(QString(transfer::queueStateToString(transfer::QueueState::CreatingDirectories)),
             QString("CreatingDirectories"));
}

void TestTransferCore::testQueueStateToString_transferring()
{
    QCOMPARE(QString(transfer::queueStateToString(transfer::QueueState::Transferring)),
             QString("Transferring"));
}

void TestTransferCore::testQueueStateToString_deleting()
{
    QCOMPARE(QString(transfer::queueStateToString(transfer::QueueState::Deleting)),
             QString("Deleting"));
}

void TestTransferCore::testQueueStateToString_batchComplete()
{
    QCOMPARE(QString(transfer::queueStateToString(transfer::QueueState::BatchComplete)),
             QString("BatchComplete"));
}

void TestTransferCore::testTransferBatchIsComplete_emptyScanned()
{
    transfer::TransferBatch batch;
    batch.scanned = true;
    batch.folderConfirmed = true;
    // No items → pendingCount() == 0 → isComplete() == true
    QVERIFY(batch.isComplete());
}

void TestTransferCore::testTransferBatchIsComplete_hasPendingItem()
{
    transfer::TransferBatch batch;
    batch.scanned = true;
    batch.folderConfirmed = true;
    transfer::TransferItem item;
    item.status = transfer::TransferItem::Status::Pending;
    batch.items.append(item);
    // 1 pending item → isComplete() == false
    QVERIFY(!batch.isComplete());
}

void TestTransferCore::testTransferBatchIsComplete_notScanned()
{
    transfer::TransferBatch batch;
    batch.scanned = false;  // Not scanned yet
    batch.folderConfirmed = true;
    // Not scanned → isComplete() == false even with no items
    QVERIFY(!batch.isComplete());
}

void TestTransferCore::testTransferBatchPendingCount()
{
    transfer::TransferBatch batch;
    batch.scanned = true;
    batch.folderConfirmed = true;

    transfer::TransferItem pending;
    pending.status = transfer::TransferItem::Status::Pending;

    transfer::TransferItem completed;
    completed.status = transfer::TransferItem::Status::Completed;

    batch.items.append(pending);
    batch.items.append(completed);
    batch.completedCount = 1;
    batch.failedCount = 0;

    QCOMPARE(batch.pendingCount(), 1);
}

void TestTransferCore::testBatchProgressIsValid_validId()
{
    transfer::BatchProgress p;
    p.batchId = 42;
    QVERIFY(p.isValid());
}

void TestTransferCore::testBatchProgressIsValid_invalidId()
{
    transfer::BatchProgress p;  // batchId defaults to -1
    QVERIFY(!p.isValid());
}

void TestTransferCore::testBatchProgressPendingItems()
{
    transfer::BatchProgress p;
    p.batchId = 1;
    p.totalItems = 10;
    p.completedItems = 3;
    p.failedItems = 2;
    QCOMPARE(p.pendingItems(), 5);
}

// ---------------------------------------------------------------------------
// normalizePath tests
// ---------------------------------------------------------------------------

void TestTransferCore::testNormalizePath_noTrailingSlash()
{
    QCOMPARE(transfer::normalizePath("/SD/folder"), QString("/SD/folder"));
}

void TestTransferCore::testNormalizePath_singleTrailingSlash()
{
    QCOMPARE(transfer::normalizePath("/SD/folder/"), QString("/SD/folder"));
}

void TestTransferCore::testNormalizePath_multipleTrailingSlashes()
{
    QCOMPARE(transfer::normalizePath("/SD/folder///"), QString("/SD/folder"));
}

void TestTransferCore::testNormalizePath_rootPath()
{
    QCOMPARE(transfer::normalizePath("/"), QString("/"));
}

void TestTransferCore::testNormalizePath_emptyString()
{
    QCOMPARE(transfer::normalizePath(""), QString(""));
}

// ---------------------------------------------------------------------------
// pendingCount tests
// ---------------------------------------------------------------------------

void TestTransferCore::testPendingCount_empty()
{
    transfer::State state;
    QCOMPARE(transfer::pendingCount(state), 0);
}

void TestTransferCore::testPendingCount_mixedStatuses()
{
    transfer::State state;
    transfer::TransferItem pending;
    transfer::TransferItem inProgress;
    transfer::TransferItem completed;
    pending.status = transfer::TransferItem::Status::Pending;
    inProgress.status = transfer::TransferItem::Status::InProgress;
    completed.status = transfer::TransferItem::Status::Completed;
    state.items = {pending, inProgress, completed};
    QCOMPARE(transfer::pendingCount(state), 1);
}

// ---------------------------------------------------------------------------
// activeCount tests
// ---------------------------------------------------------------------------

void TestTransferCore::testActiveCount_oneInProgress()
{
    transfer::State state;
    transfer::TransferItem item;
    item.status = transfer::TransferItem::Status::InProgress;
    state.items.append(item);
    QCOMPARE(transfer::activeCount(state), 1);
}

// ---------------------------------------------------------------------------
// activeAndPendingCount tests
// ---------------------------------------------------------------------------

void TestTransferCore::testActiveAndPendingCount_mixed()
{
    transfer::State state;
    transfer::TransferItem pending;
    transfer::TransferItem inProgress;
    transfer::TransferItem completed;
    pending.status = transfer::TransferItem::Status::Pending;
    inProgress.status = transfer::TransferItem::Status::InProgress;
    completed.status = transfer::TransferItem::Status::Completed;
    state.items = {pending, inProgress, completed};
    QCOMPARE(transfer::activeAndPendingCount(state), 2);
}

// ---------------------------------------------------------------------------
// canProcessNext tests
// ---------------------------------------------------------------------------

void TestTransferCore::testCanProcessNext_idle()
{
    QVERIFY(transfer::canProcessNext(transfer::QueueState::Idle));
}

void TestTransferCore::testCanProcessNext_blocked_collectingItems()
{
    QVERIFY(!transfer::canProcessNext(transfer::QueueState::CollectingItems));
}

void TestTransferCore::testCanProcessNext_blocked_scanning()
{
    QVERIFY(!transfer::canProcessNext(transfer::QueueState::Scanning));
}

void TestTransferCore::testCanProcessNext_blocked_transferring()
{
    QVERIFY(!transfer::canProcessNext(transfer::QueueState::Transferring));
}

// ---------------------------------------------------------------------------
// findBatchIndex tests
// ---------------------------------------------------------------------------

void TestTransferCore::testFindBatchIndex_found()
{
    transfer::State state;
    transfer::TransferBatch b;
    b.batchId = 42;
    state.batches.append(b);
    QCOMPARE(transfer::findBatchIndex(state, 42), 0);
}

void TestTransferCore::testFindBatchIndex_notFound()
{
    transfer::State state;
    QCOMPARE(transfer::findBatchIndex(state, 99), -1);
}

// ---------------------------------------------------------------------------
// findItemIndex tests
// ---------------------------------------------------------------------------

void TestTransferCore::testFindItemIndex_found()
{
    transfer::State state;
    transfer::TransferItem item;
    item.localPath = "/local/file.txt";
    item.remotePath = "/remote/file.txt";
    state.items.append(item);
    QCOMPARE(transfer::findItemIndex(state, "/local/file.txt", "/remote/file.txt"), 0);
}

void TestTransferCore::testFindItemIndex_notFound()
{
    transfer::State state;
    QCOMPARE(transfer::findItemIndex(state, "/local/x.txt", "/remote/x.txt"), -1);
}

// ---------------------------------------------------------------------------
// findNextPendingItem tests
// ---------------------------------------------------------------------------

void TestTransferCore::testFindNextPendingItem_found()
{
    transfer::State state;
    transfer::TransferItem completed;
    transfer::TransferItem pending;
    completed.status = transfer::TransferItem::Status::Completed;
    pending.status = transfer::TransferItem::Status::Pending;
    state.items = {completed, pending};
    QCOMPARE(transfer::findNextPendingItem(state), 1);
}

void TestTransferCore::testFindNextPendingItem_skipNonPending()
{
    transfer::State state;
    transfer::TransferItem inProgress;
    inProgress.status = transfer::TransferItem::Status::InProgress;
    state.items.append(inProgress);
    QCOMPARE(transfer::findNextPendingItem(state), -1);
}

void TestTransferCore::testFindNextPendingItem_empty()
{
    transfer::State state;
    QCOMPARE(transfer::findNextPendingItem(state), -1);
}

// ---------------------------------------------------------------------------
// isPathBeingTransferred tests
// ---------------------------------------------------------------------------

void TestTransferCore::testIsPathBeingTransferred_inActiveBatch()
{
    transfer::State state;
    transfer::TransferBatch batch;
    batch.batchId = 1;
    batch.operationType = transfer::OperationType::Download;
    batch.sourcePath = "/remote/folder";
    batch.scanned = false;
    batch.folderConfirmed = false;
    state.batches.append(batch);
    QVERIFY(transfer::isPathBeingTransferred(state, "/remote/folder",
                                             transfer::OperationType::Download));
}

void TestTransferCore::testIsPathBeingTransferred_inCompleteBatch()
{
    transfer::State state;
    transfer::TransferBatch batch;
    batch.batchId = 1;
    batch.operationType = transfer::OperationType::Download;
    batch.sourcePath = "/remote/folder";
    batch.scanned = true;
    batch.folderConfirmed = true;
    // No pending items, so isComplete() == true
    state.batches.append(batch);
    QVERIFY(!transfer::isPathBeingTransferred(state, "/remote/folder",
                                              transfer::OperationType::Download));
}

void TestTransferCore::testIsPathBeingTransferred_trailingSlash()
{
    transfer::State state;
    transfer::TransferBatch batch;
    batch.batchId = 1;
    batch.operationType = transfer::OperationType::Upload;
    batch.sourcePath = "/local/folder";
    batch.scanned = false;
    batch.folderConfirmed = false;
    state.batches.append(batch);
    // Path with trailing slash should still match
    QVERIFY(
        transfer::isPathBeingTransferred(state, "/local/folder/", transfer::OperationType::Upload));
}

void TestTransferCore::testIsPathBeingTransferred_wrongType()
{
    transfer::State state;
    transfer::TransferBatch batch;
    batch.batchId = 1;
    batch.operationType = transfer::OperationType::Upload;
    batch.sourcePath = "/local/folder";
    batch.scanned = false;
    batch.folderConfirmed = false;
    state.batches.append(batch);
    QVERIFY(!transfer::isPathBeingTransferred(state, "/local/folder",
                                              transfer::OperationType::Download));
}

void TestTransferCore::testIsPathBeingTransferred_notFound()
{
    transfer::State state;
    QVERIFY(
        !transfer::isPathBeingTransferred(state, "/some/path", transfer::OperationType::Download));
}

// ---------------------------------------------------------------------------
// hasActiveBatch tests
// ---------------------------------------------------------------------------

void TestTransferCore::testHasActiveBatch_noActive()
{
    transfer::State state;
    state.activeBatchIndex = -1;
    QVERIFY(!transfer::hasActiveBatch(state));
}

void TestTransferCore::testHasActiveBatch_withActive()
{
    transfer::State state;
    transfer::TransferBatch b;
    b.batchId = 1;
    state.batches.append(b);
    state.activeBatchIndex = 0;
    QVERIFY(transfer::hasActiveBatch(state));
}

// ---------------------------------------------------------------------------
// queuedBatchCount tests
// ---------------------------------------------------------------------------

void TestTransferCore::testQueuedBatchCount_empty()
{
    transfer::State state;
    QCOMPARE(transfer::queuedBatchCount(state), 0);
}

void TestTransferCore::testQueuedBatchCount_someIncomplete()
{
    transfer::State state;
    transfer::TransferBatch incomplete;
    transfer::TransferBatch complete;
    incomplete.scanned = false;
    incomplete.folderConfirmed = false;
    complete.scanned = true;
    complete.folderConfirmed = true;
    // complete has no items so isComplete() == true
    state.batches = {incomplete, complete};
    QCOMPARE(transfer::queuedBatchCount(state), 1);
}

// ---------------------------------------------------------------------------
// BatchProgress computation tests
// ---------------------------------------------------------------------------

void TestTransferCore::testComputeActiveBatchProgress_noActiveBatch()
{
    transfer::State state;
    state.activeBatchIndex = -1;
    transfer::BatchProgress p = transfer::computeActiveBatchProgress(state);
    QVERIFY(!p.isValid());
}

void TestTransferCore::testComputeActiveBatchProgress_withActiveBatch()
{
    transfer::State state;
    transfer::TransferBatch b;
    b.batchId = 5;
    b.description = "Downloading foo.prg";
    b.operationType = transfer::OperationType::Download;
    b.completedCount = 2;
    b.failedCount = 1;
    for (int i = 0; i < 3; ++i) {
        transfer::TransferItem item;
        item.status = transfer::TransferItem::Status::Completed;
        item.batchId = 5;
        b.items.append(item);
    }
    state.batches.append(b);
    state.activeBatchIndex = 0;
    state.queueState = transfer::QueueState::Idle;

    transfer::BatchProgress p = transfer::computeActiveBatchProgress(state);
    QVERIFY(p.isValid());
    QCOMPARE(p.batchId, 5);
    QCOMPARE(p.completedItems, 2);
    QCOMPARE(p.failedItems, 1);
    QCOMPARE(p.totalItems, 3);
    QVERIFY(!p.isScanning);
}

void TestTransferCore::testComputeBatchProgress_found()
{
    transfer::State state;
    transfer::TransferBatch b;
    b.batchId = 7;
    b.completedCount = 3;
    state.batches.append(b);
    state.activeBatchIndex = -1;  // not the active batch

    transfer::BatchProgress p = transfer::computeBatchProgress(state, 7);
    QVERIFY(p.isValid());
    QCOMPARE(p.batchId, 7);
    QCOMPARE(p.completedItems, 3);
}

void TestTransferCore::testComputeBatchProgress_notFound()
{
    transfer::State state;
    transfer::BatchProgress p = transfer::computeBatchProgress(state, 999);
    QVERIFY(!p.isValid());
}

// ---------------------------------------------------------------------------
// itemData tests
// ---------------------------------------------------------------------------

void TestTransferCore::testItemData_displayRole_upload()
{
    transfer::TransferItem item;
    item.operationType = transfer::OperationType::Upload;
    item.localPath = "/home/user/Games/Turrican.prg";
    item.remotePath = "/SD/Games/Turrican.prg";
    QVariant v = transfer::itemData(item, Qt::DisplayRole);
    QCOMPARE(v.toString(), QString("Turrican.prg"));
}

void TestTransferCore::testItemData_displayRole_download()
{
    transfer::TransferItem item;
    item.operationType = transfer::OperationType::Download;
    item.localPath = "/home/user/Games/Turrican.prg";
    item.remotePath = "/SD/Games/Turrican.prg";
    QVariant v = transfer::itemData(item, Qt::DisplayRole);
    QCOMPARE(v.toString(), QString("Turrican.prg"));
}

void TestTransferCore::testItemData_statusRole()
{
    transfer::TransferItem item;
    item.status = transfer::TransferItem::Status::InProgress;
    QVariant v = transfer::itemData(item, static_cast<int>(transfer::ItemRole::Status));
    QCOMPARE(v.toInt(), static_cast<int>(transfer::TransferItem::Status::InProgress));
}

void TestTransferCore::testItemData_progressRole_withTotal()
{
    transfer::TransferItem item;
    item.bytesTransferred = 50;
    item.totalBytes = 100;
    QVariant v = transfer::itemData(item, static_cast<int>(transfer::ItemRole::Progress));
    QCOMPARE(v.toInt(), 50);
}

void TestTransferCore::testItemData_progressRole_zeroTotal()
{
    transfer::TransferItem item;
    item.bytesTransferred = 0;
    item.totalBytes = 0;
    QVariant v = transfer::itemData(item, static_cast<int>(transfer::ItemRole::Progress));
    QCOMPARE(v.toInt(), 0);
}

// ---------------------------------------------------------------------------
// purgeBatch tests
// ---------------------------------------------------------------------------

void TestTransferCore::testPurgeBatch_removesItems()
{
    transfer::State state = makeStateWithItem(transfer::OperationType::Download,
                                              transfer::TransferItem::Status::Completed, 1);
    QCOMPARE(state.items.size(), 1);
    transfer::State result = transfer::purgeBatch(state, 1);
    QCOMPARE(result.items.size(), 0);
    QCOMPARE(result.batches.size(), 0);
}

void TestTransferCore::testPurgeBatch_adjustsCurrentIndex()
{
    transfer::State state = makeStateWithItem(transfer::OperationType::Download,
                                              transfer::TransferItem::Status::InProgress, 1);
    state.currentIndex = 0;
    transfer::State result = transfer::purgeBatch(state, 1);
    QCOMPARE(result.currentIndex, -1);
}

void TestTransferCore::testPurgeBatch_adjustsActiveBatchIndex()
{
    transfer::State state = makeStateWithItem(transfer::OperationType::Download,
                                              transfer::TransferItem::Status::Completed, 1);
    state.activeBatchIndex = 0;
    transfer::State result = transfer::purgeBatch(state, 1);
    QCOMPARE(result.activeBatchIndex, -1);
}

void TestTransferCore::testPurgeBatch_nonExistentBatch_unchanged()
{
    transfer::State state = makeStateWithItem(transfer::OperationType::Download,
                                              transfer::TransferItem::Status::Pending, 1);
    transfer::State result = transfer::purgeBatch(state, 999);
    QCOMPARE(result.items.size(), 1);
    QCOMPARE(result.batches.size(), 1);
}

// ---------------------------------------------------------------------------
// createBatch tests
// ---------------------------------------------------------------------------

void TestTransferCore::testCreateBatch_assignsIncrementingId()
{
    transfer::State state;
    state.nextBatchId = 5;
    auto r1 = transfer::createBatch(state, transfer::OperationType::Download, "D1", "f1");
    QCOMPARE(r1.batchId, 5);
    auto r2 = transfer::createBatch(r1.newState, transfer::OperationType::Upload, "U1", "f2");
    QCOMPARE(r2.batchId, 6);
}

void TestTransferCore::testCreateBatch_appendsToBatchList()
{
    transfer::State state;
    auto result = transfer::createBatch(state, transfer::OperationType::Download, "Test", "folder");
    QCOMPARE(result.newState.batches.size(), 1);
    QCOMPARE(result.newState.batches[0].description, QString("Test"));
}

void TestTransferCore::testCreateBatch_purgesCompletedBatches()
{
    // Create a completed batch (scanned + folderConfirmed, no items → isComplete() == true)
    transfer::State state;
    transfer::TransferBatch completed;
    completed.batchId = 1;
    completed.scanned = true;
    completed.folderConfirmed = true;
    state.batches.append(completed);
    state.nextBatchId = 2;

    auto result = transfer::createBatch(state, transfer::OperationType::Download, "New", "f");
    // The completed batch should be purged, leaving only the new one
    QCOMPARE(result.newState.batches.size(), 1);
    QCOMPARE(result.newState.batches[0].batchId, 2);
}

void TestTransferCore::testCreateBatch_doesNotPurgeScanning()
{
    // A completed batch that still has a pending scan should NOT be purged
    transfer::State state;
    transfer::TransferBatch completed;
    completed.batchId = 1;
    completed.scanned = true;
    completed.folderConfirmed = true;
    state.batches.append(completed);
    state.nextBatchId = 2;

    transfer::PendingScan scan;
    scan.batchId = 1;
    state.pendingScans.enqueue(scan);

    auto result = transfer::createBatch(state, transfer::OperationType::Download, "New", "f");
    QCOMPARE(result.newState.batches.size(), 2);
}

// ---------------------------------------------------------------------------
// activateNextBatch tests
// ---------------------------------------------------------------------------

void TestTransferCore::testActivateNextBatch_findsFirstIncomplete()
{
    transfer::State state;
    transfer::TransferBatch b;
    b.batchId = 10;
    b.scanned = false;
    b.folderConfirmed = false;
    transfer::TransferItem item;
    item.status = transfer::TransferItem::Status::Pending;
    item.batchId = 10;
    b.items.append(item);
    state.batches.append(b);
    state.activeBatchIndex = -1;

    transfer::State result = transfer::activateNextBatch(state);
    QCOMPARE(result.activeBatchIndex, 0);
}

void TestTransferCore::testActivateNextBatch_skipsCompleteBatches()
{
    transfer::State state;
    transfer::TransferBatch complete;
    complete.batchId = 1;
    complete.scanned = true;
    complete.folderConfirmed = true;

    transfer::TransferBatch incomplete;
    incomplete.batchId = 2;
    incomplete.scanned = false;
    incomplete.folderConfirmed = false;
    transfer::TransferItem item;
    item.status = transfer::TransferItem::Status::Pending;
    item.batchId = 2;
    incomplete.items.append(item);

    state.batches = {complete, incomplete};
    transfer::State result = transfer::activateNextBatch(state);
    QCOMPARE(result.activeBatchIndex, 1);
}

void TestTransferCore::testActivateNextBatch_noneAvailable()
{
    transfer::State state;
    transfer::State result = transfer::activateNextBatch(state);
    QCOMPARE(result.activeBatchIndex, -1);
}

// ---------------------------------------------------------------------------
// completeBatch tests
// ---------------------------------------------------------------------------

void TestTransferCore::testCompleteBatch_resetsActiveIndex()
{
    transfer::State state = makeStateWithItem(transfer::OperationType::Download,
                                              transfer::TransferItem::Status::Completed, 3);
    state.activeBatchIndex = 0;
    state.batches[0].completedCount = 1;

    auto result = transfer::completeBatch(state, 3);
    QCOMPARE(result.newState.activeBatchIndex, -1);
}

void TestTransferCore::testCompleteBatch_transitionsToIdle()
{
    transfer::State state = makeStateWithItem(transfer::OperationType::Download,
                                              transfer::TransferItem::Status::Completed, 3);
    state.queueState = transfer::QueueState::Transferring;
    state.batches[0].completedCount = 1;

    auto result = transfer::completeBatch(state, 3);
    QCOMPARE(result.newState.queueState, transfer::QueueState::Idle);
}

void TestTransferCore::testCompleteBatch_detectsFolderOperation()
{
    transfer::State state = makeStateWithItem(transfer::OperationType::Download,
                                              transfer::TransferItem::Status::Completed, 5);
    state.batches[0].completedCount = 1;
    state.currentFolderOp.batchId = 5;

    auto result = transfer::completeBatch(state, 5);
    QVERIFY(result.isFolderOperation);
}

void TestTransferCore::testCompleteBatch_activatesNext_whenAvailable()
{
    transfer::State state;

    transfer::TransferBatch b1;
    b1.batchId = 1;
    b1.scanned = true;
    b1.folderConfirmed = true;
    b1.completedCount = 1;
    transfer::TransferItem item1;
    item1.status = transfer::TransferItem::Status::Completed;
    item1.batchId = 1;
    b1.items.append(item1);

    transfer::TransferBatch b2;
    b2.batchId = 2;
    b2.scanned = false;
    b2.folderConfirmed = true;
    transfer::TransferItem item2;
    item2.status = transfer::TransferItem::Status::Pending;
    item2.batchId = 2;
    b2.items.append(item2);

    state.batches = {b1, b2};
    state.items = {item1, item2};
    state.activeBatchIndex = 0;

    auto result = transfer::completeBatch(state, 1);
    QVERIFY(!result.isFolderOperation);
    QVERIFY(result.hasRemainingActiveBatches);
    QCOMPARE(result.newState.activeBatchIndex, 1);
}

// ---------------------------------------------------------------------------
// markItemComplete tests
// ---------------------------------------------------------------------------

void TestTransferCore::testMarkItemComplete_setsStatus()
{
    transfer::State state;
    transfer::TransferItem item;
    item.status = transfer::TransferItem::Status::InProgress;
    item.batchId = 1;
    state.items.append(item);
    state.currentIndex = 0;

    transfer::TransferBatch batch;
    batch.batchId = 1;
    batch.scanned = true;
    batch.folderConfirmed = true;
    batch.items.append(item);
    state.batches.append(batch);

    auto result = transfer::markItemComplete(state, 0, transfer::TransferItem::Status::Completed);
    QCOMPARE(result.newState.items[0].status, transfer::TransferItem::Status::Completed);
}

void TestTransferCore::testMarkItemComplete_completedSetsBytes()
{
    transfer::State state;
    transfer::TransferItem item;
    item.totalBytes = 1024;
    item.bytesTransferred = 0;
    item.batchId = 1;
    state.items.append(item);

    transfer::TransferBatch batch;
    batch.batchId = 1;
    batch.scanned = true;
    batch.folderConfirmed = true;
    batch.items.append(item);
    state.batches.append(batch);

    auto result = transfer::markItemComplete(state, 0, transfer::TransferItem::Status::Completed);
    QCOMPARE(result.newState.items[0].bytesTransferred, 1024LL);
}

void TestTransferCore::testMarkItemComplete_incrementsCompletedCount()
{
    transfer::State state;
    transfer::TransferItem item;
    item.batchId = 1;
    state.items.append(item);

    transfer::TransferBatch batch;
    batch.batchId = 1;
    batch.scanned = true;
    batch.folderConfirmed = true;
    batch.completedCount = 0;
    batch.items.append(item);
    state.batches.append(batch);

    auto result = transfer::markItemComplete(state, 0, transfer::TransferItem::Status::Completed);
    QCOMPARE(result.newState.batches[0].completedCount, 1);
}

void TestTransferCore::testMarkItemComplete_incrementsFailedCount()
{
    transfer::State state;
    transfer::TransferItem item;
    item.batchId = 1;
    state.items.append(item);

    transfer::TransferBatch batch;
    batch.batchId = 1;
    batch.scanned = true;
    batch.folderConfirmed = true;
    batch.failedCount = 0;
    batch.items.append(item);
    state.batches.append(batch);

    auto result = transfer::markItemComplete(state, 0, transfer::TransferItem::Status::Failed,
                                             "Network error");
    QCOMPARE(result.newState.batches[0].failedCount, 1);
    QCOMPARE(result.newState.items[0].errorMessage, QString("Network error"));
}

void TestTransferCore::testMarkItemComplete_detectsBatchComplete()
{
    transfer::State state;
    transfer::TransferItem item;
    item.batchId = 1;
    item.status = transfer::TransferItem::Status::InProgress;
    state.items.append(item);

    transfer::TransferBatch batch;
    batch.batchId = 1;
    batch.scanned = true;
    batch.folderConfirmed = true;
    batch.items.append(item);
    state.batches.append(batch);

    auto result = transfer::markItemComplete(state, 0, transfer::TransferItem::Status::Completed);
    QVERIFY(result.batchIsComplete);
}

void TestTransferCore::testMarkItemComplete_resetsCurrentIndex()
{
    transfer::State state;
    transfer::TransferItem item;
    item.batchId = 1;
    state.items.append(item);
    state.currentIndex = 0;

    transfer::TransferBatch batch;
    batch.batchId = 1;
    batch.scanned = true;
    batch.folderConfirmed = true;
    batch.items.append(item);
    state.batches.append(batch);

    auto result = transfer::markItemComplete(state, 0, transfer::TransferItem::Status::Completed);
    QCOMPARE(result.newState.currentIndex, -1);
}

void TestTransferCore::testMarkItemComplete_invalidIndex_unchanged()
{
    transfer::State state;
    auto result = transfer::markItemComplete(state, 5, transfer::TransferItem::Status::Completed);
    QCOMPARE(result.newState.items.size(), 0);
    QCOMPARE(result.batchId, -1);
}

// ---------------------------------------------------------------------------
// clearAll tests
// ---------------------------------------------------------------------------

void TestTransferCore::testClearAll_emptiesEverything()
{
    transfer::State state = makeStateWithItem(transfer::OperationType::Download,
                                              transfer::TransferItem::Status::InProgress, 1);
    state.pendingScans.enqueue(transfer::PendingScan{});
    transfer::State result = transfer::clearAll(state);
    QCOMPARE(result.items.size(), 0);
    QCOMPARE(result.batches.size(), 0);
    QCOMPARE(result.currentIndex, -1);
    QCOMPARE(result.activeBatchIndex, -1);
    QCOMPARE(result.pendingScans.size(), 0);
}

void TestTransferCore::testClearAll_transitionsToIdle()
{
    transfer::State state;
    state.queueState = transfer::QueueState::Transferring;
    transfer::State result = transfer::clearAll(state);
    QCOMPARE(result.queueState, transfer::QueueState::Idle);
}

// ---------------------------------------------------------------------------
// removeCompleted tests
// ---------------------------------------------------------------------------

void TestTransferCore::testRemoveCompleted_keepsOnlyPending()
{
    transfer::State state;
    transfer::TransferItem pending;
    transfer::TransferItem completed;
    transfer::TransferItem failed;
    pending.status = transfer::TransferItem::Status::Pending;
    completed.status = transfer::TransferItem::Status::Completed;
    failed.status = transfer::TransferItem::Status::Failed;
    state.items = {pending, completed, failed};

    transfer::State result = transfer::removeCompleted(state);
    QCOMPARE(result.items.size(), 1);
    QCOMPARE(result.items[0].status, transfer::TransferItem::Status::Pending);
}

void TestTransferCore::testRemoveCompleted_adjustsCurrentIndex()
{
    transfer::State state;
    transfer::TransferItem completed;
    transfer::TransferItem inProgress;
    completed.status = transfer::TransferItem::Status::Completed;
    inProgress.status = transfer::TransferItem::Status::InProgress;
    state.items = {completed, inProgress};
    state.currentIndex = 1;

    transfer::State result = transfer::removeCompleted(state);
    // After removing completed at index 0, currentIndex should adjust to 0
    QCOMPARE(result.currentIndex, 0);
}

// ---------------------------------------------------------------------------
// cancelAllItems tests
// ---------------------------------------------------------------------------

void TestTransferCore::testCancelAllItems_marksPendingAsFailed()
{
    transfer::State state;
    transfer::TransferItem item;
    item.status = transfer::TransferItem::Status::Pending;
    state.items.append(item);

    transfer::State result = transfer::cancelAllItems(state);
    QCOMPARE(result.items[0].status, transfer::TransferItem::Status::Failed);
    QCOMPARE(result.items[0].errorMessage, QString("Cancelled"));
}

void TestTransferCore::testCancelAllItems_marksInProgressAsFailed()
{
    transfer::State state;
    transfer::TransferItem item;
    item.status = transfer::TransferItem::Status::InProgress;
    state.items.append(item);

    transfer::State result = transfer::cancelAllItems(state);
    QCOMPARE(result.items[0].status, transfer::TransferItem::Status::Failed);
}

void TestTransferCore::testCancelAllItems_leavesCompletedAlone()
{
    transfer::State state;
    transfer::TransferItem item;
    item.status = transfer::TransferItem::Status::Completed;
    state.items.append(item);

    transfer::State result = transfer::cancelAllItems(state);
    QCOMPARE(result.items[0].status, transfer::TransferItem::Status::Completed);
}

void TestTransferCore::testCancelAllItems_clearsBatches()
{
    transfer::State state;
    transfer::TransferBatch batch;
    batch.batchId = 1;
    state.batches.append(batch);

    transfer::State result = transfer::cancelAllItems(state);
    QCOMPARE(result.batches.size(), 0);
}

// ---------------------------------------------------------------------------
// cancelBatch tests
// ---------------------------------------------------------------------------

void TestTransferCore::testCancelBatch_marksItemsAsFailed()
{
    transfer::State state = makeStateWithItem(transfer::OperationType::Download,
                                              transfer::TransferItem::Status::Pending, 1);
    auto result = transfer::cancelBatch(state, 1);
    // Items are failed AND purged (purgeBatch is called inside cancelBatch)
    QCOMPARE(result.newState.batches.size(), 0);
    QCOMPARE(result.newState.items.size(), 0);
}

void TestTransferCore::testCancelBatch_detectsActiveBatch()
{
    transfer::State state = makeStateWithItem(transfer::OperationType::Download,
                                              transfer::TransferItem::Status::Pending, 1);
    state.activeBatchIndex = 0;

    auto result = transfer::cancelBatch(state, 1);
    QVERIFY(result.wasActiveBatch);
}

void TestTransferCore::testCancelBatch_nonExistentBatch_unchanged()
{
    transfer::State state = makeStateWithItem(transfer::OperationType::Download,
                                              transfer::TransferItem::Status::Pending, 1);
    auto result = transfer::cancelBatch(state, 999);
    QCOMPARE(result.newState.items.size(), 1);
    QVERIFY(!result.wasActiveBatch);
}

void TestTransferCore::testCancelBatch_activatesNextBatchAfterCancel()
{
    transfer::State state;
    transfer::TransferBatch b1;
    b1.batchId = 1;
    b1.scanned = false;
    b1.folderConfirmed = true;
    transfer::TransferItem item1;
    item1.status = transfer::TransferItem::Status::Pending;
    item1.batchId = 1;
    b1.items.append(item1);

    transfer::TransferBatch b2;
    b2.batchId = 2;
    b2.scanned = false;
    b2.folderConfirmed = true;
    transfer::TransferItem item2;
    item2.status = transfer::TransferItem::Status::Pending;
    item2.batchId = 2;
    b2.items.append(item2);

    state.batches = {b1, b2};
    state.items = {item1, item2};
    state.activeBatchIndex = 0;

    auto result = transfer::cancelBatch(state, 1);
    QVERIFY(result.wasActiveBatch);
    // After cancel, batch 2 should be active (now at index 0 after purge)
    QCOMPARE(result.newState.activeBatchIndex, 0);
    QCOMPARE(result.newState.batches[0].batchId, 2);
}

// ---------------------------------------------------------------------------
// respondToOverwrite tests
// ---------------------------------------------------------------------------

void TestTransferCore::testRespondToOverwrite_overwrite_confirmsItem()
{
    transfer::State state;
    transfer::TransferItem item;
    item.status = transfer::TransferItem::Status::Pending;
    item.confirmed = false;
    item.batchId = 1;
    state.items.append(item);
    state.queueState = transfer::QueueState::AwaitingFileConfirm;
    state.pendingConfirmation.itemIndex = 0;

    auto result = transfer::respondToOverwrite(state, transfer::OverwriteResponse::Overwrite);
    QVERIFY(result.newState.items[0].confirmed);
    QVERIFY(result.shouldScheduleProcessNext);
    QVERIFY(!result.shouldCancelAll);
}

void TestTransferCore::testRespondToOverwrite_overwriteAll_setsFlag()
{
    transfer::State state;
    state.queueState = transfer::QueueState::AwaitingFileConfirm;

    auto result = transfer::respondToOverwrite(state, transfer::OverwriteResponse::OverwriteAll);
    QVERIFY(result.newState.overwriteAll);
    QVERIFY(result.shouldScheduleProcessNext);
}

void TestTransferCore::testRespondToOverwrite_skip_marksSkipped()
{
    transfer::State state;
    transfer::TransferItem item;
    item.status = transfer::TransferItem::Status::Pending;
    item.batchId = 1;
    state.items.append(item);
    state.queueState = transfer::QueueState::AwaitingFileConfirm;
    state.pendingConfirmation.itemIndex = 0;

    transfer::TransferBatch batch;
    batch.batchId = 1;
    batch.scanned = true;
    batch.folderConfirmed = true;
    batch.items.append(item);
    state.batches.append(batch);

    auto result = transfer::respondToOverwrite(state, transfer::OverwriteResponse::Skip);
    QCOMPARE(result.newState.items[0].status, transfer::TransferItem::Status::Skipped);
    QCOMPARE(result.newState.currentIndex, -1);
    QVERIFY(result.shouldScheduleProcessNext);
}

void TestTransferCore::testRespondToOverwrite_skip_updatesBatchCount()
{
    transfer::State state;
    transfer::TransferItem item;
    item.batchId = 1;
    state.items.append(item);
    state.queueState = transfer::QueueState::AwaitingFileConfirm;
    state.pendingConfirmation.itemIndex = 0;

    transfer::TransferBatch batch;
    batch.batchId = 1;
    batch.scanned = true;
    batch.folderConfirmed = true;
    batch.completedCount = 0;
    batch.items.append(item);
    state.batches.append(batch);

    auto result = transfer::respondToOverwrite(state, transfer::OverwriteResponse::Skip);
    QCOMPARE(result.newState.batches[0].completedCount, 1);
}

void TestTransferCore::testRespondToOverwrite_cancel_flagsCancelAll()
{
    transfer::State state;
    state.queueState = transfer::QueueState::AwaitingFileConfirm;

    auto result = transfer::respondToOverwrite(state, transfer::OverwriteResponse::Cancel);
    QVERIFY(result.shouldCancelAll);
    QVERIFY(!result.shouldScheduleProcessNext);
}

void TestTransferCore::testRespondToOverwrite_wrongState_unchanged()
{
    transfer::State state;
    state.queueState = transfer::QueueState::Idle;

    auto result = transfer::respondToOverwrite(state, transfer::OverwriteResponse::Overwrite);
    QCOMPARE(result.newState.queueState, transfer::QueueState::Idle);
    QVERIFY(!result.shouldScheduleProcessNext);
    QVERIFY(!result.shouldCancelAll);
}

// ---------------------------------------------------------------------------
// respondToFolderExists tests
// ---------------------------------------------------------------------------

void TestTransferCore::testRespondToFolderExists_merge_dequeuesOp()
{
    transfer::State state;
    state.queueState = transfer::QueueState::AwaitingFolderConfirm;
    transfer::PendingFolderOp op;
    op.sourcePath = "/local/folder";
    state.pendingFolderOps.enqueue(op);

    auto result = transfer::respondToFolderExists(state, transfer::FolderExistsResponse::Merge);
    QVERIFY(result.shouldStartFolderOp);
    QVERIFY(!result.newState.replaceExisting);
    QCOMPARE(result.folderOpToStart.sourcePath, QString("/local/folder"));
    QCOMPARE(result.newState.pendingFolderOps.size(), 0);
}

void TestTransferCore::testRespondToFolderExists_replace_setsFlag()
{
    transfer::State state;
    state.queueState = transfer::QueueState::AwaitingFolderConfirm;
    transfer::PendingFolderOp op;
    state.pendingFolderOps.enqueue(op);

    auto result = transfer::respondToFolderExists(state, transfer::FolderExistsResponse::Replace);
    QVERIFY(result.shouldStartFolderOp);
    QVERIFY(result.newState.replaceExisting);
}

void TestTransferCore::testRespondToFolderExists_cancel_clearsFolderOps()
{
    transfer::State state;
    state.queueState = transfer::QueueState::AwaitingFolderConfirm;
    transfer::PendingFolderOp op;
    state.pendingFolderOps.enqueue(op);

    auto result = transfer::respondToFolderExists(state, transfer::FolderExistsResponse::Cancel);
    QVERIFY(result.shouldCancelFolderOps);
    QCOMPARE(result.newState.pendingFolderOps.size(), 0);
}

void TestTransferCore::testRespondToFolderExists_wrongState_unchanged()
{
    transfer::State state;
    state.queueState = transfer::QueueState::Idle;

    auto result = transfer::respondToFolderExists(state, transfer::FolderExistsResponse::Merge);
    QCOMPARE(result.newState.queueState, transfer::QueueState::Idle);
    QVERIFY(!result.shouldStartFolderOp);
}

// ---------------------------------------------------------------------------
// checkFolderConfirmation tests
// ---------------------------------------------------------------------------

void TestTransferCore::testCheckFolderConfirmation_noExisting_startsOp()
{
    transfer::State state;
    transfer::PendingFolderOp op;
    op.sourcePath = "/local/folder";
    op.destExists = false;
    state.pendingFolderOps.enqueue(op);

    auto result = transfer::checkFolderConfirmation(state);
    QVERIFY(!result.needsConfirmation);
    QVERIFY(result.shouldStartFolderOp);
    QCOMPARE(result.folderOpToStart.sourcePath, QString("/local/folder"));
}

void TestTransferCore::testCheckFolderConfirmation_someExisting_needsConfirm()
{
    transfer::State state;
    transfer::PendingFolderOp op;
    op.destExists = true;
    op.targetPath = "/remote/MyFolder";
    state.pendingFolderOps.enqueue(op);

    auto result = transfer::checkFolderConfirmation(state);
    QVERIFY(result.needsConfirmation);
    QCOMPARE(result.existingFolderNames.size(), 1);
    QCOMPARE(result.existingFolderNames[0], QString("MyFolder"));
}

void TestTransferCore::testCheckFolderConfirmation_empty_goesIdle()
{
    transfer::State state;
    state.queueState = transfer::QueueState::CollectingItems;
    // No pending folder ops

    auto result = transfer::checkFolderConfirmation(state);
    QVERIFY(!result.needsConfirmation);
    QVERIFY(!result.shouldStartFolderOp);
    QCOMPARE(result.newState.queueState, transfer::QueueState::Idle);
}

// ---------------------------------------------------------------------------
// sortDeleteQueue tests
// ---------------------------------------------------------------------------

void TestTransferCore::testSortDeleteQueue_filesBeforeDirectories()
{
    QList<transfer::DeleteItem> queue;
    transfer::DeleteItem dir;
    dir.path = "/SD/folder";
    dir.isDirectory = true;
    transfer::DeleteItem file;
    file.path = "/SD/folder/file.prg";
    file.isDirectory = false;
    queue = {dir, file};

    auto sorted = transfer::sortDeleteQueue(queue);
    QVERIFY(!sorted[0].isDirectory);  // file first
    QVERIFY(sorted[1].isDirectory);   // directory after
}

void TestTransferCore::testSortDeleteQueue_deepestDirectoriesFirst()
{
    QList<transfer::DeleteItem> queue;
    transfer::DeleteItem shallow;
    shallow.path = "/SD/a";
    shallow.isDirectory = true;
    transfer::DeleteItem deep;
    deep.path = "/SD/a/b/c";
    deep.isDirectory = true;
    queue = {shallow, deep};

    auto sorted = transfer::sortDeleteQueue(queue);
    QCOMPARE(sorted[0].path, QString("/SD/a/b/c"));
    QCOMPARE(sorted[1].path, QString("/SD/a"));
}

void TestTransferCore::testSortDeleteQueue_empty()
{
    QList<transfer::DeleteItem> queue;
    auto sorted = transfer::sortDeleteQueue(queue);
    QCOMPARE(sorted.size(), 0);
}

// ---------------------------------------------------------------------------
// processDirectoryListingForDownload tests
// ---------------------------------------------------------------------------

void TestTransferCore::testProcessDirListingDownload_filesOnly()
{
    transfer::PendingScan scan;
    scan.remotePath = "/SD/Games";
    scan.localBasePath = "/home/user/Games";
    scan.remoteBasePath = "/SD/Games";
    scan.batchId = 1;

    FtpEntry file;
    file.name = "Turrican.prg";
    file.isDirectory = false;

    auto result = transfer::processDirectoryListingForDownload(scan, {file});
    QCOMPARE(result.newFileDownloads.size(), 1);
    QCOMPARE(result.newSubScans.size(), 0);
    QCOMPARE(result.newFileDownloads[0].first, QString("/SD/Games/Turrican.prg"));
    QCOMPARE(result.newFileDownloads[0].second, QString("/home/user/Games/Turrican.prg"));
}

void TestTransferCore::testProcessDirListingDownload_directoriesOnly()
{
    transfer::PendingScan scan;
    scan.remotePath = "/SD/Games";
    scan.localBasePath = "/home/user/Games";
    scan.remoteBasePath = "/SD/Games";
    scan.batchId = 1;

    FtpEntry dir;
    dir.name = "Action";
    dir.isDirectory = true;

    auto result = transfer::processDirectoryListingForDownload(scan, {dir});
    QCOMPARE(result.newSubScans.size(), 1);
    QCOMPARE(result.newFileDownloads.size(), 0);
    QCOMPARE(result.newSubScans[0].remotePath, QString("/SD/Games/Action"));
}

void TestTransferCore::testProcessDirListingDownload_mixed()
{
    transfer::PendingScan scan;
    scan.remotePath = "/SD/Games";
    scan.localBasePath = "/home/user/Games";
    scan.remoteBasePath = "/SD/Games";
    scan.batchId = 1;

    FtpEntry file;
    file.name = "game.prg";
    file.isDirectory = false;
    FtpEntry dir;
    dir.name = "Extras";
    dir.isDirectory = true;

    auto result = transfer::processDirectoryListingForDownload(scan, {file, dir});
    QCOMPARE(result.newFileDownloads.size(), 1);
    QCOMPARE(result.newSubScans.size(), 1);
}

void TestTransferCore::testProcessDirListingDownload_correctPathConstruction()
{
    // Subdirectory scan — test relative path calculation
    transfer::PendingScan scan;
    scan.remotePath = "/SD/Games/Action";
    scan.localBasePath = "/home/user/Games";
    scan.remoteBasePath = "/SD/Games";
    scan.batchId = 1;

    FtpEntry file;
    file.name = "Turrican.prg";
    file.isDirectory = false;

    auto result = transfer::processDirectoryListingForDownload(scan, {file});
    QCOMPARE(result.newFileDownloads[0].second, QString("/home/user/Games/Action/Turrican.prg"));
}

// ---------------------------------------------------------------------------
// processDirectoryListingForDelete tests
// ---------------------------------------------------------------------------

void TestTransferCore::testProcessDirListingDelete_filesAddedToQueue()
{
    FtpEntry file;
    file.name = "game.prg";
    file.isDirectory = false;

    auto result = transfer::processDirectoryListingForDelete("/SD/Games", {file});
    QCOMPARE(result.fileItems.size(), 1);
    QCOMPARE(result.fileItems[0].path, QString("/SD/Games/game.prg"));
    QVERIFY(!result.fileItems[0].isDirectory);
}

void TestTransferCore::testProcessDirListingDelete_subdirectoriesQueued()
{
    FtpEntry dir;
    dir.name = "SubDir";
    dir.isDirectory = true;

    auto result = transfer::processDirectoryListingForDelete("/SD/Games", {dir});
    QCOMPARE(result.newSubScans.size(), 1);
    QCOMPARE(result.newSubScans[0].remotePath, QString("/SD/Games/SubDir"));
}

void TestTransferCore::testProcessDirListingDelete_parentDirectoryIncluded()
{
    auto result = transfer::processDirectoryListingForDelete("/SD/Games", {});
    QCOMPARE(result.directoryItem.path, QString("/SD/Games"));
    QVERIFY(result.directoryItem.isDirectory);
}

// ---------------------------------------------------------------------------
// updateFolderExistence tests
// ---------------------------------------------------------------------------

void TestTransferCore::testUpdateFolderExistence_marksExisting()
{
    transfer::State state;
    transfer::PendingFolderOp op;
    op.destPath = "/SD";
    op.targetPath = "/SD/Games";
    op.destExists = false;
    state.pendingFolderOps.enqueue(op);

    FtpEntry dir;
    dir.name = "Games";
    dir.isDirectory = true;

    transfer::State result = transfer::updateFolderExistence(state, "/SD", {dir});
    QVERIFY(result.pendingFolderOps.head().destExists);
}

void TestTransferCore::testUpdateFolderExistence_marksNonExisting()
{
    transfer::State state;
    transfer::PendingFolderOp op;
    op.destPath = "/SD";
    op.targetPath = "/SD/Games";
    op.destExists = true;
    state.pendingFolderOps.enqueue(op);

    FtpEntry dir;
    dir.name = "Music";
    dir.isDirectory = true;

    transfer::State result = transfer::updateFolderExistence(state, "/SD", {dir});
    QVERIFY(!result.pendingFolderOps.head().destExists);
}

// ---------------------------------------------------------------------------
// checkUploadFileExists tests
// ---------------------------------------------------------------------------

void TestTransferCore::testCheckUploadFileExists_fileExists()
{
    transfer::State state;
    transfer::TransferItem item;
    item.remotePath = "/SD/Games/Turrican.prg";
    item.operationType = transfer::OperationType::Upload;
    state.items.append(item);
    state.currentIndex = 0;

    FtpEntry existing;
    existing.name = "Turrican.prg";
    existing.isDirectory = false;

    auto result = transfer::checkUploadFileExists(state, {existing});
    QVERIFY(result.fileExists);
    QCOMPARE(result.newState.queueState, transfer::QueueState::AwaitingFileConfirm);
    QCOMPARE(result.newState.pendingConfirmation.itemIndex, 0);
}

void TestTransferCore::testCheckUploadFileExists_fileDoesNotExist()
{
    transfer::State state;
    transfer::TransferItem item;
    item.remotePath = "/SD/Games/Turrican.prg";
    item.confirmed = false;
    state.items.append(item);
    state.currentIndex = 0;

    auto result = transfer::checkUploadFileExists(state, {});
    QVERIFY(!result.fileExists);
    QVERIFY(result.newState.items[0].confirmed);
}

void TestTransferCore::testCheckUploadFileExists_invalidCurrentIndex()
{
    transfer::State state;
    state.currentIndex = -1;

    auto result = transfer::checkUploadFileExists(state, {});
    QVERIFY(!result.fileExists);
    QCOMPARE(result.newState.currentIndex, -1);
}

// =============================================================================
// decideNextAction tests
// =============================================================================

void TestTransferCore::testDecideNext_blocked_whenNotIdle()
{
    transfer::State state;
    state.queueState = transfer::QueueState::Transferring;
    auto d = transfer::decideNextAction(state, true, [](const QString &) { return false; });
    QCOMPARE(d.action, transfer::ProcessNextAction::Blocked);
}

void TestTransferCore::testDecideNext_noFtpClient()
{
    transfer::State state;
    state.queueState = transfer::QueueState::Idle;
    auto d = transfer::decideNextAction(state, false, [](const QString &) { return false; });
    QCOMPARE(d.action, transfer::ProcessNextAction::NoFtpClient);
}

void TestTransferCore::testDecideNext_startFolderOp_whenPending()
{
    transfer::State state;
    state.queueState = transfer::QueueState::Idle;
    transfer::PendingFolderOp op;
    op.sourcePath = "/local/folder";
    state.pendingFolderOps.enqueue(op);
    // currentFolderOp.batchId defaults to -1, so folder op is ready to start

    auto d = transfer::decideNextAction(state, true, [](const QString &) { return false; });
    QCOMPARE(d.action, transfer::ProcessNextAction::StartFolderOp);
    QCOMPARE(d.folderOpToStart.sourcePath, QString("/local/folder"));
}

void TestTransferCore::testDecideNext_overwriteCheck_download_fileExists()
{
    transfer::State state;
    state.queueState = transfer::QueueState::Idle;
    transfer::TransferItem item;
    item.status = transfer::TransferItem::Status::Pending;
    item.operationType = transfer::OperationType::Download;
    item.localPath = "/home/user/file.prg";
    item.confirmed = false;
    state.items.append(item);
    state.overwriteAll = false;

    // Local file "exists"
    auto d = transfer::decideNextAction(state, true, [](const QString &) { return true; });
    QCOMPARE(d.action, transfer::ProcessNextAction::NeedOverwriteCheck_Download);
    QCOMPARE(d.itemIndex, 0);
}

void TestTransferCore::testDecideNext_startTransfer_download()
{
    transfer::State state;
    state.queueState = transfer::QueueState::Idle;
    transfer::TransferItem item;
    item.status = transfer::TransferItem::Status::Pending;
    item.operationType = transfer::OperationType::Download;
    item.localPath = "/home/user/file.prg";
    item.remotePath = "/SD/file.prg";
    item.confirmed = false;
    state.overwriteAll = true;  // Skip overwrite check
    state.items.append(item);

    auto d = transfer::decideNextAction(state, true, [](const QString &) { return false; });
    QCOMPARE(d.action, transfer::ProcessNextAction::StartTransfer);
    QCOMPARE(d.itemIndex, 0);
    QCOMPARE(d.fileNameForSignal, QString("file.prg"));
}

void TestTransferCore::testDecideNext_startTransfer_upload_confirmed()
{
    transfer::State state;
    state.queueState = transfer::QueueState::Idle;
    transfer::TransferItem item;
    item.status = transfer::TransferItem::Status::Pending;
    item.operationType = transfer::OperationType::Upload;
    item.localPath = "/home/user/game.prg";
    item.remotePath = "/SD/game.prg";
    item.confirmed = true;  // Already confirmed
    state.items.append(item);

    auto d = transfer::decideNextAction(state, true, [](const QString &) { return false; });
    QCOMPARE(d.action, transfer::ProcessNextAction::StartTransfer);
}

void TestTransferCore::testDecideNext_overwriteCheck_upload_notConfirmed()
{
    transfer::State state;
    state.queueState = transfer::QueueState::Idle;
    transfer::TransferItem item;
    item.status = transfer::TransferItem::Status::Pending;
    item.operationType = transfer::OperationType::Upload;
    item.localPath = "/home/user/game.prg";
    item.remotePath = "/SD/games/game.prg";
    item.confirmed = false;
    state.overwriteAll = false;
    state.items.append(item);

    auto d = transfer::decideNextAction(state, true, [](const QString &) { return false; });
    QCOMPARE(d.action, transfer::ProcessNextAction::NeedOverwriteCheck_Upload);
    QCOMPARE(d.uploadCheckDir, QString("/SD/games"));
}

void TestTransferCore::testDecideNext_noPending()
{
    transfer::State state;
    state.queueState = transfer::QueueState::Idle;
    // No items
    auto d = transfer::decideNextAction(state, true, [](const QString &) { return false; });
    QCOMPARE(d.action, transfer::ProcessNextAction::NoPending);
}

// =============================================================================
// handleFtpError tests
// =============================================================================

void TestTransferCore::testHandleFtpError_duringDelete_skipsAndContinues()
{
    transfer::State state;
    state.queueState = transfer::QueueState::Deleting;
    transfer::DeleteItem item;
    item.path = "/SD/Games/Turrican.prg";
    item.isDirectory = false;
    state.deleteQueue.append(item);
    state.deletedCount = 0;

    auto result = transfer::handleFtpError(state, "Connection lost");
    QVERIFY(result.isDeleteError);
    QCOMPARE(result.deleteFileName, QString("Turrican.prg"));
    QCOMPARE(result.newState.deletedCount, 1);
    QVERIFY(result.shouldProcessNextDelete);
}

void TestTransferCore::testHandleFtpError_duringDirCreation()
{
    transfer::State state;
    state.queueState = transfer::QueueState::CreatingDirectories;
    state.currentFolderOp.batchId = 3;
    state.currentFolderOp.sourcePath = "/local/MyProject";

    auto result = transfer::handleFtpError(state, "Permission denied");
    QVERIFY(result.isFolderCreationError);
    QCOMPARE(result.folderName, QString("MyProject"));
    QCOMPARE(result.folderBatchId, 3);
}

void TestTransferCore::testHandleFtpError_duringTransfer_marksItemFailed()
{
    transfer::State state;
    transfer::TransferItem item;
    item.status = transfer::TransferItem::Status::InProgress;
    item.localPath = "/home/user/game.prg";
    item.remotePath = "/SD/game.prg";
    item.operationType = transfer::OperationType::Upload;
    item.batchId = 1;
    state.items.append(item);
    state.currentIndex = 0;

    transfer::TransferBatch batch;
    batch.batchId = 1;
    batch.scanned = true;
    batch.folderConfirmed = true;
    batch.items.append(item);
    state.batches.append(batch);

    auto result = transfer::handleFtpError(state, "Timeout");
    QVERIFY(result.hasCurrentItem);
    QCOMPARE(result.transferFileName, QString("game.prg"));
    QCOMPARE(result.newState.items[0].status, transfer::TransferItem::Status::Failed);
    QCOMPARE(result.newState.currentIndex, -1);
}

void TestTransferCore::testHandleFtpError_clearsAllPendingRequests()
{
    transfer::State state;
    state.requestedListings.insert("/SD/Games");
    state.requestedDeleteListings.insert("/SD/Old");
    transfer::PendingScan scan;
    scan.remotePath = "/SD/test";
    state.pendingScans.enqueue(scan);

    auto result = transfer::handleFtpError(state, "Error");
    QVERIFY(result.newState.requestedListings.isEmpty());
    QVERIFY(result.newState.requestedDeleteListings.isEmpty());
    QVERIFY(result.newState.pendingScans.isEmpty());
}

// =============================================================================
// handleOperationTimeout tests
// =============================================================================

void TestTransferCore::testHandleOperationTimeout_marksInProgressAsFailed()
{
    transfer::State state;
    transfer::TransferItem item;
    item.status = transfer::TransferItem::Status::InProgress;
    item.localPath = "/home/user/game.prg";
    state.items.append(item);
    state.currentIndex = 0;

    transfer::State result = transfer::handleOperationTimeout(state);
    QCOMPARE(result.items[0].status, transfer::TransferItem::Status::Failed);
    QVERIFY(!result.items[0].errorMessage.isEmpty());
}

void TestTransferCore::testHandleOperationTimeout_resetsCurrentIndex()
{
    transfer::State state;
    state.currentIndex = 5;
    transfer::State result = transfer::handleOperationTimeout(state);
    QCOMPARE(result.currentIndex, -1);
}

void TestTransferCore::testHandleOperationTimeout_transitionsToIdle()
{
    transfer::State state;
    state.queueState = transfer::QueueState::Transferring;
    transfer::State result = transfer::handleOperationTimeout(state);
    QCOMPARE(result.queueState, transfer::QueueState::Idle);
}

QTEST_MAIN(TestTransferCore)
#include "test_transfercore.moc"
