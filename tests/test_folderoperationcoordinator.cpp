#include "core/transfercore.h"
#include "ftp/folderoperationcoordinator.h"
#include "mocks/mockftpclient.h"
#include "mocks/mocklocalfilesystemservice.h"

#include <QSignalSpy>
#include <QtTest>

class TestFolderOperationCoordinator : public QObject
{
    Q_OBJECT

private:
    transfer::State state_;
    MockFtpClient *mockFtp = nullptr;
    MockLocalFileSystemService *mockFs = nullptr;
    FolderOperationCoordinator *coordinator = nullptr;

    void setupBatchCallback()
    {
        coordinator->setCreateBatchCallback([this](transfer::OperationType type,
                                                   const QString &desc, const QString &folder,
                                                   const QString &src) -> int {
            auto r = transfer::createBatch(state_, type, desc, folder, src);
            state_ = r.newState;
            return r.batchId;
        });
    }

private slots:
    void init()
    {
        state_ = transfer::State();
        mockFtp = new MockFtpClient(this);
        mockFs = new MockLocalFileSystemService(this);
        coordinator = new FolderOperationCoordinator(state_, mockFtp, mockFs, this);
        setupBatchCallback();
        mockFtp->mockSetConnected(true);
    }

    void cleanup()
    {
        delete coordinator;
        delete mockFs;
        delete mockFtp;
        coordinator = nullptr;
        mockFs = nullptr;
        mockFtp = nullptr;
    }

    void testEnqueueRecursive_download_destNotExist_startsImmediately()
    {
        mockFs->mockSetDirectoryExists("/local/target/Games", false);
        state_.autoMerge = false;

        QSignalSpy spy(coordinator, &FolderOperationCoordinator::startDownloadScanRequested);

        coordinator->enqueueRecursive(transfer::OperationType::Download, "/remote/Games",
                                      "/local/target");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("/remote/Games"));
    }

    void testEnqueueRecursive_delete_whenFree_startsADeleteOperationWithItsOwnBatch()
    {
        QSignalSpy spy(coordinator, &FolderOperationCoordinator::startDeleteRequested);

        coordinator->enqueueRecursive(transfer::OperationType::Delete, "/remote/Games", QString());

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("/remote/Games"));
        QCOMPARE(state_.batches.size(), 1);
        QCOMPARE(state_.batches.first().operationType, transfer::OperationType::Delete);
        QCOMPARE(state_.currentFolderOp.batchId, state_.batches.first().batchId);
        QVERIFY(transfer::isPathBeingTransferred(state_, "/remote/Games",
                                                 transfer::OperationType::Delete));
    }

    void testEnqueueRecursive_whileAnotherFolderOperationRuns_waitsItsTurn()
    {
        coordinator->enqueueRecursive(transfer::OperationType::Delete, "/remote/A", QString());
        state_.queueState = transfer::QueueState::Idle;  // e.g. between two of its steps
        QSignalSpy startSpy(coordinator, &FolderOperationCoordinator::startDeleteRequested);
        QSignalSpy scheduleSpy(coordinator,
                               &FolderOperationCoordinator::scheduleProcessNextRequested);

        coordinator->enqueueRecursive(transfer::OperationType::Delete, "/remote/B", QString());

        QCOMPARE(startSpy.count(), 0);
        QCOMPARE(state_.pendingFolderOps.size(), 1);
        QCOMPARE(state_.pendingFolderOps.head().sourcePath, QString("/remote/B"));
        QCOMPARE(scheduleSpy.count(), 1);
    }

    void testOnFolderOperationComplete_notConnected_keepsQueuedOperations()
    {
        coordinator->enqueueRecursive(transfer::OperationType::Delete, "/remote/A", QString());
        state_.queueState = transfer::QueueState::Scanning;
        coordinator->enqueueRecursive(transfer::OperationType::Delete, "/remote/B", QString());
        mockFtp->mockSetConnected(false);
        QSignalSpy startSpy(coordinator, &FolderOperationCoordinator::startDeleteRequested);
        QSignalSpy allDoneSpy(coordinator, &FolderOperationCoordinator::allOperationsCompleted);

        coordinator->onFolderOperationComplete();

        QCOMPARE(startSpy.count(), 0);
        QCOMPARE(allDoneSpy.count(), 0);
        QCOMPARE(state_.pendingFolderOps.size(), 1);
        QCOMPARE(state_.currentFolderOp.batchId, -1);
    }

    void testEnqueueRecursive_download_destExists_queuesInPendingFolderOps()
    {
        mockFs->mockSetDirectoryExists("/local/target/Games", true);
        state_.autoMerge = false;

        coordinator->enqueueRecursive(transfer::OperationType::Download, "/remote/Games",
                                      "/local/target");

        QCOMPARE(state_.pendingFolderOps.size(), 1);
        QVERIFY(state_.pendingFolderOps.head().destExists);
    }

    void testEnqueueRecursive_download_autoMerge_skipsConfirmation()
    {
        mockFs->mockSetDirectoryExists("/local/target/Games", true);
        state_.autoMerge = true;

        QSignalSpy spy(coordinator, &FolderOperationCoordinator::startDownloadScanRequested);

        coordinator->enqueueRecursive(transfer::OperationType::Download, "/remote/Games",
                                      "/local/target");

        QCOMPARE(spy.count(), 1);
    }

    void testEnqueueRecursive_upload_autoMerge_emitsDirectoryCreationRequested()
    {
        state_.autoMerge = true;

        QSignalSpy spy(coordinator, &FolderOperationCoordinator::startDirectoryCreationRequested);

        coordinator->enqueueRecursive(transfer::OperationType::Upload, "/local/mydir",
                                      "/remote/target");

        QCOMPARE(spy.count(), 1);
    }

    void testRespondToFolderExists_merge_emitsStartDownloadScanRequested()
    {
        state_.queueState = transfer::QueueState::AwaitingFolderConfirm;
        transfer::PendingFolderOp op;
        op.operationType = transfer::OperationType::Download;
        op.sourcePath = "/remote/Games";
        op.destPath = "/local/target";
        op.targetPath = "/local/target/Games";
        op.destExists = false;
        state_.pendingFolderOps.enqueue(op);

        QSignalSpy spy(coordinator, &FolderOperationCoordinator::startDownloadScanRequested);

        coordinator->respondToFolderExists(transfer::FolderExistsResponse::Merge);

        QCOMPARE(spy.count(), 1);
    }

    void testOnFolderOperationComplete_noPending_emitsAllOperationsCompleted()
    {
        QSignalSpy spy(coordinator, &FolderOperationCoordinator::allOperationsCompleted);

        coordinator->onFolderOperationComplete();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(state_.replaceExisting, false);
    }

    void testOnFolderOperationComplete_otherBatchStillQueued_doesNotEmitAllOperationsCompleted()
    {
        transfer::TransferBatch queuedUpload;
        queuedUpload.batchId = 9;
        transfer::TransferItem item;
        item.batchId = 9;
        queuedUpload.items.append(item);
        state_.batches.append(queuedUpload);

        QSignalSpy spy(coordinator, &FolderOperationCoordinator::allOperationsCompleted);

        coordinator->onFolderOperationComplete();

        QCOMPARE(spy.count(), 0);
    }

    void testOnFolderOperationComplete_morePending_emitsStartDownloadScanNotAllCompleted()
    {
        mockFs->mockSetDirectoryExists("/local/target/dir2", false);
        transfer::PendingFolderOp op;
        op.operationType = transfer::OperationType::Download;
        op.sourcePath = "/remote/dir2";
        op.destPath = "/local/target";
        op.targetPath = "/local/target/dir2";
        op.destExists = false;
        state_.pendingFolderOps.enqueue(op);

        QSignalSpy allDone(coordinator, &FolderOperationCoordinator::allOperationsCompleted);
        QSignalSpy startScan(coordinator, &FolderOperationCoordinator::startDownloadScanRequested);

        coordinator->onFolderOperationComplete();

        QCOMPARE(allDone.count(), 0);
        QCOMPARE(startScan.count(), 1);
    }
};

QTEST_MAIN(TestFolderOperationCoordinator)
#include "test_folderoperationcoordinator.moc"
