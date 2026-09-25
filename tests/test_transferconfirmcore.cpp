#include "core/transferconfirmcore.h"

#include <QTest>

namespace {

transfer::State makeConfirmState(int itemIdx = 0)
{
    transfer::State state;
    state.queueState = transfer::QueueState::AwaitingFileConfirm;
    state.pendingConfirmation.itemIndex = itemIdx;
    transfer::TransferBatch batch;
    batch.batchId = 1;
    batch.scanned = true;
    batch.folderConfirmed = true;
    transfer::TransferItem item;
    item.status = transfer::TransferItem::Status::Pending;
    item.batchId = 1;
    item.operationType = transfer::OperationType::Download;
    batch.items.append(item);
    state.items.append(item);
    state.batches.append(batch);
    return state;
}

}  // namespace

class TestTransferConfirmCore : public QObject
{
    Q_OBJECT

private slots:
    void testRespondToOverwrite_overwrite_confirmsItem()
    {
        auto state = makeConfirmState(0);

        auto result = transfer::respondToOverwrite(state, transfer::OverwriteResponse::Overwrite);

        QVERIFY(result.newState.items[0].confirmed);
        QVERIFY(result.shouldScheduleProcessNext);
        QCOMPARE(result.newState.queueState, transfer::QueueState::Idle);
    }

    void testRespondToOverwrite_overwriteAll_coversTheItemsBatchOnly()
    {
        auto state = makeConfirmState(0);

        auto result =
            transfer::respondToOverwrite(state, transfer::OverwriteResponse::OverwriteAll);

        QCOMPARE(result.newState.overwriteAllBatchId, 1);
        QVERIFY(result.newState.items[0].confirmed);
        QVERIFY(result.shouldScheduleProcessNext);

        transfer::TransferItem sameBatch;
        sameBatch.batchId = 1;
        transfer::TransferItem laterBatch;
        laterBatch.batchId = 2;
        QVERIFY(transfer::mayOverwriteWithoutAsking(result.newState, sameBatch));
        QVERIFY(!transfer::mayOverwriteWithoutAsking(result.newState, laterBatch));
    }

    void testRespondToOverwrite_skip_marksSkipped()
    {
        auto state = makeConfirmState(0);

        auto result = transfer::respondToOverwrite(state, transfer::OverwriteResponse::Skip);

        QCOMPARE(result.newState.items[0].status, transfer::TransferItem::Status::Skipped);
        QVERIFY(result.shouldScheduleProcessNext);
        QCOMPARE(result.newState.batches[0].completedCount, 1);
    }

    void testRespondToOverwrite_cancel_signalsCancelAll()
    {
        auto state = makeConfirmState(0);

        auto result = transfer::respondToOverwrite(state, transfer::OverwriteResponse::Cancel);

        QVERIFY(result.shouldCancelAll);
    }

    void testRespondToOverwrite_wrongState_returnsUnchanged()
    {
        auto state = makeConfirmState(0);
        state.queueState = transfer::QueueState::Idle;

        auto result = transfer::respondToOverwrite(state, transfer::OverwriteResponse::Overwrite);

        QVERIFY(!result.shouldScheduleProcessNext);
        QVERIFY(!result.newState.items[0].confirmed);
    }

    void testRespondToFolderExists_merge_dequeuesAndStartsFolderOp()
    {
        transfer::State state;
        state.queueState = transfer::QueueState::AwaitingFolderConfirm;
        transfer::PendingFolderOp op;
        op.batchId = 1;
        op.operationType = transfer::OperationType::Download;
        op.destExists = true;
        state.pendingFolderOps.enqueue(op);

        auto result = transfer::respondToFolderExists(state, transfer::FolderExistsResponse::Merge);

        QVERIFY(result.shouldStartFolderOp);
        QVERIFY(!result.newState.replaceExisting);
        QVERIFY(result.newState.pendingFolderOps.isEmpty());
    }

    void testRespondToFolderExists_replace_setsReplaceExisting()
    {
        transfer::State state;
        state.queueState = transfer::QueueState::AwaitingFolderConfirm;
        transfer::PendingFolderOp op;
        op.batchId = 1;
        op.operationType = transfer::OperationType::Download;
        state.pendingFolderOps.enqueue(op);

        auto result =
            transfer::respondToFolderExists(state, transfer::FolderExistsResponse::Replace);

        QVERIFY(result.shouldStartFolderOp);
        QVERIFY(result.newState.replaceExisting);
    }

    void testRespondToFolderExists_cancel_keepsFoldersTheDialogDidNotAskAbout()
    {
        transfer::State state;
        state.queueState = transfer::QueueState::AwaitingFolderConfirm;
        transfer::PendingFolderOp existing;
        existing.sourcePath = "/local/A";
        existing.destExists = true;
        transfer::PendingFolderOp fresh;
        fresh.sourcePath = "/local/B";
        fresh.destExists = false;
        transfer::PendingFolderOp alsoExisting;
        alsoExisting.sourcePath = "/local/C";
        alsoExisting.destExists = true;
        state.pendingFolderOps << existing << fresh << alsoExisting;

        auto result =
            transfer::respondToFolderExists(state, transfer::FolderExistsResponse::Cancel);

        QVERIFY(!result.shouldCancelFolderOps);
        QVERIFY(result.shouldStartFolderOp);
        QCOMPARE(result.folderOpToStart.sourcePath, QString("/local/B"));
        QVERIFY(result.newState.pendingFolderOps.isEmpty());
        QCOMPARE(result.newState.queueState, transfer::QueueState::Idle);
    }

    void testRespondToFolderExists_cancel_clearsPendingOps()
    {
        transfer::State state;
        state.queueState = transfer::QueueState::AwaitingFolderConfirm;
        transfer::PendingFolderOp op;
        op.batchId = 1;
        op.destExists = true;
        state.pendingFolderOps.enqueue(op);

        auto result =
            transfer::respondToFolderExists(state, transfer::FolderExistsResponse::Cancel);

        QVERIFY(result.shouldCancelFolderOps);
        QVERIFY(result.newState.pendingFolderOps.isEmpty());
    }

    void testRespondToFolderExists_merge_settlesTheQuestionForTheFoldersItListed()
    {
        transfer::State state;
        state.queueState = transfer::QueueState::AwaitingFolderConfirm;
        transfer::PendingFolderOp first;
        first.destExists = true;
        transfer::PendingFolderOp listedToo;
        listedToo.sourcePath = "/local/B";
        listedToo.destExists = true;
        transfer::PendingFolderOp notListed;
        notListed.sourcePath = "/local/C";
        state.pendingFolderOps << first << listedToo << notListed;

        auto result = transfer::respondToFolderExists(state, transfer::FolderExistsResponse::Merge);

        QCOMPARE(result.newState.pendingFolderOps.size(), 2);
        QVERIFY(result.newState.pendingFolderOps.at(0).confirmed);
        QVERIFY(!result.newState.pendingFolderOps.at(1).confirmed);
    }

    void testCheckFolderConfirmation_settledFolder_isNotAskedAboutAgain()
    {
        transfer::State state;
        transfer::PendingFolderOp unsettled;
        unsettled.targetPath = "/remote/A";
        unsettled.destExists = true;
        transfer::PendingFolderOp settled;
        settled.targetPath = "/remote/B";
        settled.destExists = true;
        settled.confirmed = true;
        state.pendingFolderOps << unsettled << settled;

        auto result = transfer::checkFolderConfirmation(state);

        QCOMPARE(result.existingFolderNames, QStringList{"A"});
    }

    void testNeedsFolderCheck_unsettledUpload_isChecked()
    {
        transfer::State state;
        transfer::PendingFolderOp op;
        op.operationType = transfer::OperationType::Upload;

        QVERIFY(transfer::needsFolderCheck(state, op));
        op.confirmed = true;
        QVERIFY(!transfer::needsFolderCheck(state, op));
    }

    void testNeedsFolderCheck_autoMergeOrDelete_isNotChecked()
    {
        transfer::State state;
        transfer::PendingFolderOp del;
        del.operationType = transfer::OperationType::Delete;
        QVERIFY(!transfer::needsFolderCheck(state, del));

        state.autoMerge = true;
        transfer::PendingFolderOp upload;
        upload.operationType = transfer::OperationType::Upload;
        QVERIFY(!transfer::needsFolderCheck(state, upload));
    }

    void testRespondToFolderExists_wrongState_returnsUnchanged()
    {
        transfer::State state;
        state.queueState = transfer::QueueState::Idle;
        transfer::PendingFolderOp op;
        op.batchId = 1;
        state.pendingFolderOps.enqueue(op);

        auto result = transfer::respondToFolderExists(state, transfer::FolderExistsResponse::Merge);

        QVERIFY(!result.shouldStartFolderOp);
        QCOMPARE(result.newState.pendingFolderOps.size(), 1);
    }

    void testCheckFolderConfirmation_destExists_needsConfirmation()
    {
        transfer::State state;
        transfer::PendingFolderOp op;
        op.batchId = 1;
        op.operationType = transfer::OperationType::Download;
        op.targetPath = "/local/target/Games";
        op.destExists = true;
        state.pendingFolderOps.enqueue(op);

        auto result = transfer::checkFolderConfirmation(state);

        QVERIFY(result.needsConfirmation);
        QCOMPARE(result.newState.queueState, transfer::QueueState::AwaitingFolderConfirm);
    }

    void testCheckFolderConfirmation_destNotExists_startsFolderOp()
    {
        transfer::State state;
        transfer::PendingFolderOp op;
        op.batchId = 1;
        op.operationType = transfer::OperationType::Download;
        op.targetPath = "/local/target/Games";
        op.destExists = false;
        state.pendingFolderOps.enqueue(op);

        auto result = transfer::checkFolderConfirmation(state);

        QVERIFY(!result.needsConfirmation);
        QVERIFY(result.shouldStartFolderOp);
    }

    void testCheckFolderConfirmation_emptyOps_transitionsToIdle()
    {
        transfer::State state;

        auto result = transfer::checkFolderConfirmation(state);

        QVERIFY(!result.needsConfirmation);
        QVERIFY(!result.shouldStartFolderOp);
        QCOMPARE(result.newState.queueState, transfer::QueueState::Idle);
    }
};

QTEST_MAIN(TestTransferConfirmCore)
#include "test_transferconfirmcore.moc"
