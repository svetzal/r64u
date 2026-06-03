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
