/**
 * @file test_singlefileenqueuehandler.cpp
 * @brief Unit tests for SingleFileEnqueueHandler.
 *
 * Tests cover the single-file enqueue logic for uploads and downloads,
 * including batch creation, append-to-existing-batch, explicit batch ID,
 * and error paths.
 */

#include "core/transfercore.h"
#include "core/transferftpcore.h"
#include "mocks/mocklocalfilesystemservice.h"
#include "services/singlefileenqueuehandler.h"

#include <QSignalSpy>
#include <QtTest>

class TestSingleFileEnqueueHandler : public QObject
{
    Q_OBJECT

private:
    transfer::State state_;
    MockLocalFileSystemService *mockFs = nullptr;
    SingleFileEnqueueHandler *handler = nullptr;

    void setupCreateBatch()
    {
        handler->setCreateBatchCallback([this](transfer::OperationType type, const QString &desc,
                                               const QString &folder, const QString &src) -> int {
            auto r = transfer::createBatch(state_, type, desc, folder, src);
            state_ = r.newState;
            return r.batchId;
        });
    }

private slots:
    void init()
    {
        state_ = transfer::State();
        mockFs = new MockLocalFileSystemService(this);
        handler = new SingleFileEnqueueHandler(state_, mockFs, this);
        setupCreateBatch();
    }

    void cleanup()
    {
        delete handler;
        delete mockFs;
        handler = nullptr;
        mockFs = nullptr;
    }

    // ========================================================================
    // enqueueDownload — new batch path
    // ========================================================================

    void testEnqueueDownload_newBatch_createsDownloadBatch()
    {
        // No active batch yet — handler must create a new Download batch.
        handler->enqueueDownload("/remote/file.prg", "/local/file.prg");

        QCOMPARE(state_.batches.size(), 1);
        QCOMPARE(state_.batches[0].operationType, transfer::OperationType::Download);
        QCOMPARE(state_.items.size(), 1);
        QCOMPARE(state_.items[0].remotePath, QString("/remote/file.prg"));
        QCOMPARE(state_.items[0].operationType, transfer::OperationType::Download);
    }

    void testEnqueueDownload_newBatch_emitsItemsAboutToBeInserted()
    {
        QSignalSpy spy(handler, &SingleFileEnqueueHandler::itemsAboutToBeInserted);

        handler->enqueueDownload("/remote/file.prg", "/local/file.prg");

        QCOMPARE(spy.count(), 1);
        // Emitted with (0, 0) — first item at row 0
        QCOMPARE(spy.at(0).at(0).toInt(), 0);
        QCOMPARE(spy.at(0).at(1).toInt(), 0);
    }

    void testEnqueueDownload_newBatch_emitsItemsInserted()
    {
        QSignalSpy spy(handler, &SingleFileEnqueueHandler::itemsInserted);

        handler->enqueueDownload("/remote/file.prg", "/local/file.prg");

        QCOMPARE(spy.count(), 1);
    }

    void testEnqueueDownload_newBatch_emitsBatchStarted()
    {
        QSignalSpy spy(handler, &SingleFileEnqueueHandler::batchStarted);

        handler->enqueueDownload("/remote/file.prg", "/local/file.prg");

        QCOMPARE(spy.count(), 1);
        // batchStarted receives the new batch's ID
        QCOMPARE(spy.at(0).at(0).toInt(), state_.batches[0].batchId);
    }

    void testEnqueueDownload_newBatch_emitsQueueChanged()
    {
        QSignalSpy spy(handler, &SingleFileEnqueueHandler::queueChanged);

        handler->enqueueDownload("/remote/file.prg", "/local/file.prg");

        QCOMPARE(spy.count(), 1);
    }

    // ========================================================================
    // enqueueDownload — append to active batch
    // ========================================================================

    void testEnqueueDownload_appendToActiveBatch_doesNotEmitBatchStartedAgain()
    {
        // First enqueue creates the batch and makes it active.
        handler->enqueueDownload("/remote/a.prg", "/local/a.prg");

        QSignalSpy batchStartedSpy(handler, &SingleFileEnqueueHandler::batchStarted);
        QSignalSpy itemsInsertedSpy(handler, &SingleFileEnqueueHandler::itemsInserted);

        // Second enqueue should append to the same active Download batch.
        handler->enqueueDownload("/remote/b.prg", "/local/b.prg");

        // batchStarted must NOT fire again — it is already active.
        QCOMPARE(batchStartedSpy.count(), 0);
        // itemsInserted must fire once for the second item.
        QCOMPARE(itemsInsertedSpy.count(), 1);

        QCOMPARE(state_.items.size(), 2);
        QCOMPARE(state_.batches.size(), 1);
    }

    void testEnqueueDownload_appendToActiveBatch_itemGoesIntoSameBatch()
    {
        handler->enqueueDownload("/remote/a.prg", "/local/a.prg");
        int firstBatchId = state_.batches[0].batchId;

        handler->enqueueDownload("/remote/b.prg", "/local/b.prg");

        QCOMPARE(state_.items[1].batchId, firstBatchId);
    }

    // ========================================================================
    // enqueueUpload — explicit targetBatchId
    // ========================================================================

    void testEnqueueUpload_explicitBatchId_landsInSpecifiedBatch()
    {
        // Create a batch manually via the callback.
        int batchId = -1;
        handler->setCreateBatchCallback([this, &batchId](transfer::OperationType type,
                                                         const QString &desc, const QString &folder,
                                                         const QString &src) -> int {
            auto r = transfer::createBatch(state_, type, desc, folder, src);
            state_ = r.newState;
            batchId = r.batchId;
            return r.batchId;
        });

        // Enqueue once to create a batch.
        mockFs->mockSetFileSize("/local/x.prg", 1024);
        handler->enqueueUpload("/local/x.prg", "/remote/x.prg");
        QVERIFY(batchId >= 0);
        int targetId = batchId;

        // Now enqueue a second upload into that explicit batch.
        mockFs->mockSetFileSize("/local/y.prg", 512);
        handler->enqueueUpload("/local/y.prg", "/remote/y.prg", targetId);

        QCOMPARE(state_.items.size(), 2);
        QCOMPARE(state_.items[1].batchId, targetId);
        QCOMPARE(state_.batches.size(), 1);
    }

    // ========================================================================
    // enqueueDownload — invalid explicit targetBatchId
    // ========================================================================

    void testEnqueueDownload_invalidExplicitBatchId_emitsOperationFailed()
    {
        QSignalSpy failSpy(handler, &SingleFileEnqueueHandler::operationFailed);

        // Use a batch ID that doesn't exist.
        handler->enqueueDownload("/remote/file.prg", "/local/file.prg", 9999);

        QCOMPARE(failSpy.count(), 1);
        // No items or batches should be created.
        QCOMPARE(state_.items.size(), 0);
        QCOMPARE(state_.batches.size(), 0);
    }

    // ========================================================================
    // enqueueUpload — invalid batch with no active Upload batch
    // ========================================================================

    void testEnqueueUpload_invalidBatchNoActiveUpload_emitsOperationFailedWithMessage()
    {
        QSignalSpy failSpy(handler, &SingleFileEnqueueHandler::operationFailed);

        // Override callback to return -1 (simulate createBatch failure scenario by
        // returning an ID for a batch that will not be found in state_).
        // We do this by providing an ID that is never added to state_.
        handler->setCreateBatchCallback(
            [](transfer::OperationType, const QString &, const QString &, const QString &) -> int {
                return 9999;  // ID not in state_
            });

        handler->enqueueUpload("/local/file.prg", "/remote/file.prg");

        QCOMPARE(failSpy.count(), 1);
        const QString errorMsg = failSpy.at(0).at(1).toString();
        QVERIFY2(
            errorMsg.contains("upload batch", Qt::CaseInsensitive),
            qPrintable(QString("Expected 'upload batch' in error message, got: %1").arg(errorMsg)));
    }

    // ========================================================================
    // finishDirectoryCreation — files present
    // ========================================================================

    void testFinishDirectoryCreation_withFiles_enqueuedUploadsAndSchedules()
    {
        // Create a batch that finishDirectoryCreation will mark as scanned.
        auto r = transfer::createBatch(state_, transfer::OperationType::Upload, "Uploading src",
                                       "src", "/local/src");
        state_ = r.newState;
        state_.currentFolderOp.batchId = r.batchId;
        state_.currentFolderOp.sourcePath = "/local/src";
        state_.currentFolderOp.targetPath = "/remote/dst";

        // Wire the completeBatch callback (should NOT be called in this path).
        bool completeBatchCalled = false;
        handler->setCompleteBatchCallback(
            [&completeBatchCalled](int) { completeBatchCalled = true; });

        // Configure mock FS: source dir exists with two files.
        mockFs->mockSetDirectoryExists("/local/src", true);
        mockFs->mockSetFiles("/local/src", {"/local/src/a.prg", "/local/src/sub/b.sid"});
        mockFs->mockSetFileSize("/local/src/a.prg", 1024);
        mockFs->mockSetFileSize("/local/src/sub/b.sid", 512);

        QSignalSpy scheduleSpy(handler, &SingleFileEnqueueHandler::scheduleProcessNextRequested);
        QSignalSpy transitionSpy(handler, &SingleFileEnqueueHandler::transitionToIdleRequested);

        handler->finishDirectoryCreation();

        // Two items should have been enqueued.
        QCOMPARE(state_.items.size(), 2);
        QCOMPARE(state_.items[0].batchId, r.batchId);
        QCOMPARE(state_.items[1].batchId, r.batchId);

        // Remote paths are built as targetPath + '/' + relativePath.
        QStringList remotePaths;
        for (const auto &item : state_.items)
            remotePaths << item.remotePath;
        QVERIFY(remotePaths.contains("/remote/dst/a.prg"));
        QVERIFY(remotePaths.contains("/remote/dst/sub/b.sid"));

        // Both signals must fire; completeBatch must NOT have been called.
        QCOMPARE(transitionSpy.count(), 1);
        QCOMPARE(scheduleSpy.count(), 1);
        QVERIFY(!completeBatchCalled);
    }

    // ========================================================================
    // finishDirectoryCreation — empty folder
    // ========================================================================

    void testFinishDirectoryCreation_emptyFolder_callsCompleteBatchAndDoesNotSchedule()
    {
        auto r = transfer::createBatch(state_, transfer::OperationType::Upload, "Uploading empty",
                                       "empty", "/local/empty");
        state_ = r.newState;
        state_.currentFolderOp.batchId = r.batchId;
        state_.currentFolderOp.sourcePath = "/local/empty";
        state_.currentFolderOp.targetPath = "/remote/dst";

        int completedBatchId = -1;
        handler->setCompleteBatchCallback([&completedBatchId](int id) { completedBatchId = id; });

        // Source dir exists but has no files.
        mockFs->mockSetDirectoryExists("/local/empty", true);
        mockFs->mockSetFiles("/local/empty", {});

        QSignalSpy scheduleSpy(handler, &SingleFileEnqueueHandler::scheduleProcessNextRequested);
        QSignalSpy transitionSpy(handler, &SingleFileEnqueueHandler::transitionToIdleRequested);

        handler->finishDirectoryCreation();

        // No items enqueued.
        QCOMPARE(state_.items.size(), 0);

        // completeBatch callback must be invoked with the correct batch ID.
        QCOMPARE(completedBatchId, r.batchId);

        // scheduleProcessNext and transitionToIdle must NOT fire for empty folder.
        QCOMPARE(scheduleSpy.count(), 0);
        QCOMPARE(transitionSpy.count(), 0);
    }
};

QTEST_MAIN(TestSingleFileEnqueueHandler)
#include "test_singlefileenqueuehandler.moc"
