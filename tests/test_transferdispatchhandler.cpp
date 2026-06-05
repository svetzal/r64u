/**
 * @file test_transferdispatchhandler.cpp
 * @brief Unit tests for TransferDispatchHandler covering each processNext action branch.
 */

#include "core/transfercore.h"
#include "ftp/folderoperationcoordinator.h"
#include "mocks/mockftpclient.h"
#include "mocks/mocklocalfilesystemservice.h"
#include "services/transferdispatchhandler.h"

#include <QSignalSpy>
#include <QtTest>

class TestTransferDispatchHandler : public QObject
{
    Q_OBJECT

private:
    transfer::State state_;
    MockFtpClient *mockFtp = nullptr;
    MockLocalFileSystemService *mockFs = nullptr;
    FolderOperationCoordinator *folderCoordinator = nullptr;
    TransferDispatchHandler *handler = nullptr;

    /// Appends a pending upload item and returns its index in state_.items.
    int addPendingUpload(const QString &local = "/local/file.txt",
                         const QString &remote = "/remote/file.txt")
    {
        transfer::TransferItem item;
        item.localPath = local;
        item.remotePath = remote;
        item.operationType = transfer::OperationType::Upload;
        item.status = transfer::TransferItem::Status::Pending;
        item.batchId = -1;
        state_.items.append(item);
        return state_.items.size() - 1;
    }

    /// Appends a pending download item and returns its index in state_.items.
    int addPendingDownload(const QString &remote = "/remote/file.txt",
                           const QString &local = "/local/file.txt")
    {
        transfer::TransferItem item;
        item.localPath = local;
        item.remotePath = remote;
        item.operationType = transfer::OperationType::Download;
        item.status = transfer::TransferItem::Status::Pending;
        item.batchId = -1;
        state_.items.append(item);
        return state_.items.size() - 1;
    }

private slots:
    void init()
    {
        state_ = transfer::State();
        mockFtp = new MockFtpClient(this);
        mockFtp->mockSetConnected(true);
        mockFs = new MockLocalFileSystemService(this);
        folderCoordinator = new FolderOperationCoordinator(state_, mockFtp, mockFs, this);
        handler = new TransferDispatchHandler(state_, this);
        handler->setFtpClient(mockFtp);
        handler->setLocalFileSystem(mockFs);
        handler->setFolderCoordinator(folderCoordinator);
    }

    void cleanup()
    {
        delete handler;
        delete folderCoordinator;
        delete mockFs;
        delete mockFtp;
        handler = nullptr;
        folderCoordinator = nullptr;
        mockFs = nullptr;
        mockFtp = nullptr;
        state_ = transfer::State();
    }

    // -------------------------------------------------------------------------
    // Blocked branch
    // -------------------------------------------------------------------------

    void testProcessNext_blocked_emitsStatusMessage()
    {
        // Put queue in Transferring state with a pending item — decideNextAction returns Blocked
        addPendingUpload();
        state_.queueState = transfer::QueueState::Transferring;

        QSignalSpy statusSpy(handler, &TransferDispatchHandler::statusMessage);

        handler->processNext();

        QCOMPARE(statusSpy.count(), 1);
        QVERIFY(!statusSpy.at(0).at(0).toString().isEmpty());
    }

    // -------------------------------------------------------------------------
    // NoFtpClient branch
    // -------------------------------------------------------------------------

    void testProcessNext_noFtpClient_emitsOperationFailed()
    {
        addPendingUpload();
        state_.queueState = transfer::QueueState::Idle;

        // Remove FTP client so ftpReady is false
        handler->setFtpClient(nullptr);

        QSignalSpy failedSpy(handler, &TransferDispatchHandler::operationFailed);

        handler->processNext();

        QCOMPARE(failedSpy.count(), 1);
        QVERIFY(!failedSpy.at(0).at(1).toString().isEmpty());
    }

    void testProcessNext_disconnectedFtpClient_emitsOperationFailed()
    {
        addPendingUpload();
        state_.queueState = transfer::QueueState::Idle;

        mockFtp->mockSetConnected(false);

        QSignalSpy failedSpy(handler, &TransferDispatchHandler::operationFailed);

        handler->processNext();

        QCOMPARE(failedSpy.count(), 1);
    }

    // -------------------------------------------------------------------------
    // StartFolderOp branch
    // -------------------------------------------------------------------------

    void testProcessNext_startFolderOp_drainsPendingFolderOps()
    {
        // Populate a pending folder op so decideNextAction returns StartFolderOp
        transfer::PendingFolderOp op;
        op.operationType = transfer::OperationType::Download;
        op.sourcePath = "/SD/Music";
        op.destPath = "/local/Music";
        op.batchId = 1;
        state_.pendingFolderOps.enqueue(op);
        state_.queueState = transfer::QueueState::Idle;

        // No pending transfer items so state machine goes to StartFolderOp
        QCOMPARE(state_.pendingFolderOps.size(), 1);

        handler->processNext();

        // startNextPendingFolderOp() dequeues the entry
        QCOMPARE(state_.pendingFolderOps.size(), 0);
    }

    // -------------------------------------------------------------------------
    // NeedOverwriteCheck_Download branch
    // -------------------------------------------------------------------------

    void testProcessNext_needOverwriteDownload_emitsOverwriteConfirmationNeeded()
    {
        // Pre-configure the local file as existing so the overwrite check fires
        addPendingDownload("/remote/file.txt", "/local/file.txt");
        state_.queueState = transfer::QueueState::Idle;
        mockFs->mockSetFileExists("/local/file.txt", true);

        QSignalSpy overwriteSpy(handler, &TransferDispatchHandler::overwriteConfirmationNeeded);

        handler->processNext();

        QCOMPARE(overwriteSpy.count(), 1);
        QCOMPARE(overwriteSpy.at(0).at(1).value<transfer::OperationType>(),
                 transfer::OperationType::Download);
        QCOMPARE(state_.queueState, transfer::QueueState::AwaitingFileConfirm);
    }

    // -------------------------------------------------------------------------
    // StartTransfer — Upload branch
    // -------------------------------------------------------------------------

    void testProcessNext_startTransferUpload_callsFtpUpload()
    {
        addPendingUpload("/local/file.txt", "/remote/file.txt");
        state_.queueState = transfer::QueueState::Idle;
        // Skip the remote overwrite check so decideNextAction goes straight to StartTransfer
        state_.overwriteAll = true;

        QSignalSpy operationStartedSpy(handler, &TransferDispatchHandler::operationStarted);
        QSignalSpy dataChangedSpy(handler, &TransferDispatchHandler::itemDataChanged);
        QSignalSpy timeoutSpy(handler, &TransferDispatchHandler::startTimeoutRequested);

        handler->processNext();

        QCOMPARE(operationStartedSpy.count(), 1);
        QCOMPARE(dataChangedSpy.count(), 1);
        QCOMPARE(timeoutSpy.count(), 1);
        QCOMPARE(state_.queueState, transfer::QueueState::Transferring);
        QCOMPARE(mockFtp->mockGetUploadRequests().size(), 1);
        QCOMPARE(mockFtp->mockGetUploadRequests().first(), QString("/local/file.txt"));
    }

    // -------------------------------------------------------------------------
    // StartTransfer — Download branch
    // -------------------------------------------------------------------------

    void testProcessNext_startTransferDownload_callsFtpDownload()
    {
        // Ensure file doesn't exist locally so we skip the overwrite check
        addPendingDownload("/remote/file.txt", "/local/file.txt");
        state_.queueState = transfer::QueueState::Idle;
        mockFs->mockSetFileExists("/local/file.txt", false);

        QSignalSpy operationStartedSpy(handler, &TransferDispatchHandler::operationStarted);
        QSignalSpy timeoutSpy(handler, &TransferDispatchHandler::startTimeoutRequested);

        handler->processNext();

        QCOMPARE(operationStartedSpy.count(), 1);
        QCOMPARE(timeoutSpy.count(), 1);
        QCOMPARE(state_.queueState, transfer::QueueState::Transferring);
        QCOMPARE(mockFtp->mockGetDownloadRequests().size(), 1);
        QCOMPARE(mockFtp->mockGetDownloadRequests().first(), QString("/remote/file.txt"));
    }

    // -------------------------------------------------------------------------
    // NoPending branch
    // -------------------------------------------------------------------------

    void testProcessNext_noPending_withEmptyBatches_emitsAllOperationsCompleted()
    {
        // No items, no batches — NoPending with empty batches → allOperationsCompleted
        state_.queueState = transfer::QueueState::Idle;
        QVERIFY(state_.items.isEmpty());
        QVERIFY(state_.batches.isEmpty());

        QSignalSpy completedSpy(handler, &TransferDispatchHandler::allOperationsCompleted);
        QSignalSpy stopSpy(handler, &TransferDispatchHandler::stopTimeoutRequested);

        handler->processNext();

        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(stopSpy.count(), 1);
        QCOMPARE(state_.currentIndex, -1);
    }

    void testProcessNext_noPending_withPendingBatches_doesNotEmitAllCompleted()
    {
        // No items but a batch exists — NoPending without emitting allOperationsCompleted
        state_.queueState = transfer::QueueState::Idle;
        transfer::TransferBatch batch;
        batch.batchId = 42;
        state_.batches.append(batch);

        QSignalSpy completedSpy(handler, &TransferDispatchHandler::allOperationsCompleted);

        handler->processNext();

        QCOMPARE(completedSpy.count(), 0);
    }

    // -------------------------------------------------------------------------
    // transitionTo tests
    // -------------------------------------------------------------------------

    void testTransitionTo_changesState()
    {
        state_.queueState = transfer::QueueState::Idle;

        handler->transitionTo(transfer::QueueState::Transferring);

        QCOMPARE(state_.queueState, transfer::QueueState::Transferring);
    }

    void testTransitionTo_noopWhenAlreadyInState()
    {
        state_.queueState = transfer::QueueState::Idle;

        // Should be a no-op
        handler->transitionTo(transfer::QueueState::Idle);

        QCOMPARE(state_.queueState, transfer::QueueState::Idle);
    }

    // -------------------------------------------------------------------------
    // Null-guard: null folderCoordinator does not crash on StartFolderOp
    // -------------------------------------------------------------------------

    void testProcessNext_startFolderOp_nullCoordinator_doesNotCrash()
    {
        handler->setFolderCoordinator(nullptr);

        transfer::PendingFolderOp op;
        op.operationType = transfer::OperationType::Download;
        op.sourcePath = "/SD/Music";
        op.destPath = "/local/Music";
        op.batchId = 1;
        state_.pendingFolderOps.enqueue(op);
        state_.queueState = transfer::QueueState::Idle;

        // Should not crash even with null coordinator
        handler->processNext();

        // The pending op is NOT dequeued because coordinator was null
        QCOMPARE(state_.pendingFolderOps.size(), 1);
    }
};

QTEST_MAIN(TestTransferDispatchHandler)
#include "test_transferdispatchhandler.moc"
