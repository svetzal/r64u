/**
 * @file test_singlefileenqueuehandler.cpp
 * @brief Unit tests for SingleFileEnqueueHandler.
 *
 * Tests cover the single-file enqueue logic for uploads and downloads,
 * including batch creation, append-to-existing-batch, explicit batch ID,
 * and error paths.
 */

#include "mocks/mocklocalfilesystemservice.h"
#include "models/singlefileenqueuehandler.h"
#include "services/transfercore.h"
#include "services/transferftpcore.h"

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
        handler->setCreateBatchCallback([](transfer::OperationType, const QString &,
                                           const QString &, const QString &) -> int {
            return 9999;  // ID not in state_
        });

        handler->enqueueUpload("/local/file.prg", "/remote/file.prg");

        QCOMPARE(failSpy.count(), 1);
        const QString errorMsg = failSpy.at(0).at(1).toString();
        QVERIFY2(errorMsg.contains("upload batch", Qt::CaseInsensitive),
                 qPrintable(QString("Expected 'upload batch' in error message, got: %1").arg(errorMsg)));
    }
};

QTEST_MAIN(TestSingleFileEnqueueHandler)
#include "test_singlefileenqueuehandler.moc"
