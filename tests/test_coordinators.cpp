/**
 * @file test_coordinators.cpp
 * @brief Isolated unit tests for coordinator classes.
 *
 * These tests exercise coordinator behaviour without any real disk I/O by
 * injecting MockLocalFileSystem and MockFtpClient. This is possible because
 * all file system and FTP operations are now routed through gateway interfaces.
 *
 * Compare with test_transferqueue.cpp, which tests the full stack end-to-end
 * with a real LocalFileSystem and a mock FTP client.
 */

#include "ftp/folderoperationcoordinator.h"
#include "ftp/recursivescancoordinator.h"
#include "ftp/remotedirectorycreator.h"
#include "mocks/mockftpclient.h"
#include "mocks/mocklocalfilesystem.h"
#include "services/ftpentry.h"
#include "services/transfercore.h"

#include <QSignalSpy>
#include <QtTest>

// ============================================================================
// RemoteDirectoryCreator tests
// ============================================================================

class TestRemoteDirectoryCreator : public QObject
{
    Q_OBJECT

private:
    transfer::State state_;
    MockFtpClient *mockFtp = nullptr;
    MockLocalFileSystem *mockFs = nullptr;
    RemoteDirectoryCreator *creator = nullptr;

private slots:
    void init()
    {
        state_ = transfer::State();
        mockFtp = new MockFtpClient(this);
        mockFs = new MockLocalFileSystem(this);
        creator = new RemoteDirectoryCreator(state_, mockFtp, mockFs, this);
        mockFtp->mockSetConnected(true);
    }

    void cleanup()
    {
        delete creator;
        delete mockFs;
        delete mockFtp;
        creator = nullptr;
        mockFs = nullptr;
        mockFtp = nullptr;
    }

    // -----------------------------------------------------------------------
    // queueDirectoriesForUpload tests
    // -----------------------------------------------------------------------

    void testQueueDirectoriesForUpload_rootOnlyWhenNoSubdirs()
    {
        // Given: no subdirectories
        mockFs->mockSetSubdirectories("/local/mydir", QStringList{});

        creator->queueDirectoriesForUpload("/local/mydir", "/remote/mydir");

        // Root directory should be queued
        QCOMPARE(state_.pendingMkdirs.size(), 1);
        QCOMPARE(state_.pendingMkdirs.head().remotePath, QString("/remote/mydir"));
        QCOMPARE(state_.totalDirectoriesToCreate, 1);
        QCOMPARE(state_.directoriesCreated, 0);
    }

    void testQueueDirectoriesForUpload_populatesSubdirectories()
    {
        // Given: two subdirectories
        mockFs->mockSetSubdirectories("/local/root", {"/local/root/subA", "/local/root/subA/deep"});

        creator->queueDirectoriesForUpload("/local/root", "/remote/root");

        // Root + 2 subdirectories = 3 total
        QCOMPARE(state_.pendingMkdirs.size(), 3);
        QCOMPARE(state_.totalDirectoriesToCreate, 3);

        // Verify root is first
        QCOMPARE(state_.pendingMkdirs.dequeue().remotePath, QString("/remote/root"));

        // Verify subdir relative paths are computed correctly
        QCOMPARE(state_.pendingMkdirs.dequeue().remotePath, QString("/remote/root/subA"));
        QCOMPARE(state_.pendingMkdirs.dequeue().remotePath, QString("/remote/root/subA/deep"));
    }

    void testQueueDirectoriesForUpload_resetsCounters()
    {
        state_.directoriesCreated = 99;  // Simulate stale state

        mockFs->mockSetSubdirectories("/local/dir", QStringList{});
        creator->queueDirectoriesForUpload("/local/dir", "/remote/dir");

        QCOMPARE(state_.directoriesCreated, 0);
    }

    void testQueueDirectoriesForUpload_emitsProgressSignal()
    {
        QSignalSpy spy(creator, &RemoteDirectoryCreator::directoryCreationProgress);
        mockFs->mockSetSubdirectories("/local/dir", QStringList{});

        creator->queueDirectoriesForUpload("/local/dir", "/remote/dir");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 0);  // created
        QCOMPARE(spy.at(0).at(1).toInt(), 1);  // total (root only)
    }

    // -----------------------------------------------------------------------
    // createNextDirectory tests
    // -----------------------------------------------------------------------

    void testCreateNextDirectory_issuesMkdirForHead()
    {
        mockFs->mockSetSubdirectories("/local/dir", QStringList{});
        creator->queueDirectoriesForUpload("/local/dir", "/remote/dir");

        creator->createNextDirectory();

        QCOMPARE(mockFtp->mockGetMkdirRequests().size(), 1);
        QCOMPARE(mockFtp->mockGetMkdirRequests().first(), QString("/remote/dir"));
    }

    void testCreateNextDirectory_emitsAllCreatedWhenQueueEmpty()
    {
        QSignalSpy spy(creator, &RemoteDirectoryCreator::allDirectoriesCreated);

        // Empty queue — nothing queued yet
        creator->createNextDirectory();

        QCOMPARE(spy.count(), 1);
        QVERIFY(mockFtp->mockGetMkdirRequests().isEmpty());
    }

    // -----------------------------------------------------------------------
    // onDirectoryCreated tests
    // -----------------------------------------------------------------------

    void testOnDirectoryCreated_dequeuesAndAdvances()
    {
        mockFs->mockSetSubdirectories("/local/dir", {"/local/dir/sub"});
        creator->queueDirectoriesForUpload("/local/dir", "/remote/dir");
        state_.queueState = transfer::QueueState::CreatingDirectories;

        creator->onDirectoryCreated("/remote/dir");

        QCOMPARE(state_.directoriesCreated, 1);
        QCOMPARE(state_.pendingMkdirs.size(), 1);  // sub still pending
    }

    void testOnDirectoryCreated_emitsAllCreatedWhenLastDir()
    {
        QSignalSpy spy(creator, &RemoteDirectoryCreator::allDirectoriesCreated);
        mockFs->mockSetSubdirectories("/local/dir", QStringList{});
        creator->queueDirectoriesForUpload("/local/dir", "/remote/dir");
        state_.queueState = transfer::QueueState::CreatingDirectories;

        creator->onDirectoryCreated("/remote/dir");  // only one dir

        QCOMPARE(spy.count(), 1);
    }

    void testOnDirectoryCreated_ignoredWhenNotCreatingDirectories()
    {
        mockFs->mockSetSubdirectories("/local/dir", QStringList{});
        creator->queueDirectoriesForUpload("/local/dir", "/remote/dir");
        state_.queueState = transfer::QueueState::Idle;  // Wrong state

        creator->onDirectoryCreated("/remote/dir");

        // Should be ignored — counter not incremented
        QCOMPARE(state_.directoriesCreated, 0);
    }
};

// ============================================================================
// RecursiveScanCoordinator tests
// ============================================================================

class TestRecursiveScanCoordinator : public QObject
{
    Q_OBJECT

private:
    transfer::State state_;
    MockFtpClient *mockFtp = nullptr;
    MockLocalFileSystem *mockFs = nullptr;
    RecursiveScanCoordinator *scanner = nullptr;

    static FtpEntry makeDir(const QString &name)
    {
        FtpEntry e;
        e.name = name;
        e.isDirectory = true;
        return e;
    }

    static FtpEntry makeFile(const QString &name, qint64 size = 0)
    {
        FtpEntry e;
        e.name = name;
        e.isDirectory = false;
        e.size = size;
        return e;
    }

private slots:
    void init()
    {
        state_ = transfer::State();
        mockFtp = new MockFtpClient(this);
        mockFs = new MockLocalFileSystem(this);
        scanner = new RecursiveScanCoordinator(state_, mockFtp, mockFs, this);
        mockFtp->mockSetConnected(true);

        // Wire mock's directoryListed signal to the scanner, mirroring how
        // TransferQueue connects them in production.
        connect(mockFtp, &IFtpClient::directoryListed, scanner,
                &RecursiveScanCoordinator::onDirectoryListed);
    }

    void cleanup()
    {
        delete scanner;
        delete mockFs;
        delete mockFtp;
        scanner = nullptr;
        mockFs = nullptr;
        mockFtp = nullptr;
    }

    // -----------------------------------------------------------------------
    // startDownloadScan tests
    // -----------------------------------------------------------------------

    void testStartDownloadScan_initializesState()
    {
        state_.directoriesScanned = 5;
        state_.filesDiscovered = 10;

        scanner->startDownloadScan("/remote/Games", "/local/downloads", "/remote/Games", 1);

        QCOMPARE(state_.scanningFolderName, QString("Games"));
        QCOMPARE(state_.directoriesScanned, 0);
        QCOMPARE(state_.filesDiscovered, 0);
        QVERIFY(state_.requestedListings.contains("/remote/Games"));
    }

    void testStartDownloadScan_emitsScanningStarted()
    {
        QSignalSpy spy(scanner, &RecursiveScanCoordinator::scanningStarted);

        scanner->startDownloadScan("/remote/Games", "/local/downloads", "/remote/Games", 1);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("Games"));
        QCOMPARE(spy.at(0).at(1).value<transfer::OperationType>(),
                 transfer::OperationType::Download);
    }

    void testStartDownloadScan_listsRemotePath()
    {
        scanner->startDownloadScan("/remote/dir", "/local/dir", "/remote/dir", 42);

        QCOMPARE(mockFtp->mockGetListRequests().size(), 1);
        QCOMPARE(mockFtp->mockGetListRequests().first(), QString("/remote/dir"));
    }

    // -----------------------------------------------------------------------
    // handleDirectoryListingForDownload tests
    // -----------------------------------------------------------------------

    void testDownloadListing_discoversFiles()
    {
        scanner->startDownloadScan("/remote/dir", "/local/base", "/remote/dir", 1);

        QSignalSpy spy(scanner, &RecursiveScanCoordinator::downloadFileDiscovered);

        QList<FtpEntry> entries = {makeFile("game.prg", 4096)};
        mockFtp->mockSetDirectoryListing("/remote/dir", entries);
        mockFtp->mockProcessNextOperation();  // emit directoryListed

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("/remote/dir/game.prg"));
        QCOMPARE(spy.at(0).at(2).toInt(), 1);  // batchId
        QCOMPARE(state_.filesDiscovered, 1);
    }

    void testDownloadListing_createsLocalDirForSubdirectory()
    {
        scanner->startDownloadScan("/remote/root", "/local/base", "/remote/root", 1);

        QList<FtpEntry> entries = {makeDir("sub")};
        mockFtp->mockSetDirectoryListing("/remote/root", entries);
        mockFtp->mockProcessNextOperation();

        // A local directory should have been created via the gateway
        QVERIFY(mockFs->mockGetMkpathRequests().contains("/local/base/sub"));
    }

    void testDownloadListing_emitsProgressAfterScan()
    {
        scanner->startDownloadScan("/remote/dir", "/local/base", "/remote/dir", 1);

        QSignalSpy spy(scanner, &RecursiveScanCoordinator::scanningProgress);

        QList<FtpEntry> entries = {makeFile("a.prg")};
        mockFtp->mockSetDirectoryListing("/remote/dir", entries);
        mockFtp->mockProcessNextOperation();

        QVERIFY(spy.count() >= 1);
    }

    void testDownloadListing_noLocalDirCreatedForFiles()
    {
        scanner->startDownloadScan("/remote/dir", "/local/base", "/remote/dir", 1);

        // A listing with only files (no subdirs)
        QList<FtpEntry> entries = {makeFile("a.prg"), makeFile("b.prg")};
        mockFtp->mockSetDirectoryListing("/remote/dir", entries);
        mockFtp->mockProcessNextOperation();

        // No mkpath calls — no subdirectories discovered
        QVERIFY(mockFs->mockGetMkpathRequests().isEmpty());
    }

    // -----------------------------------------------------------------------
    // startDeleteScan tests
    // -----------------------------------------------------------------------

    void testStartDeleteScan_initializesState()
    {
        scanner->startDeleteScan("/remote/Demos");

        QCOMPARE(state_.scanningFolderName, QString("Demos"));
        QCOMPARE(state_.directoriesScanned, 0);
        QCOMPARE(state_.filesDiscovered, 0);
        QVERIFY(state_.requestedDeleteListings.contains("/remote/Demos"));
    }

    void testStartDeleteScan_emitsScanningStartedForDelete()
    {
        QSignalSpy spy(scanner, &RecursiveScanCoordinator::scanningStarted);

        scanner->startDeleteScan("/remote/Demos");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).value<transfer::OperationType>(), transfer::OperationType::Delete);
    }

    void testDeleteListing_emitsDeleteScanCompleteWhenDone()
    {
        scanner->startDeleteScan("/remote/dir");

        QSignalSpy spy(scanner, &RecursiveScanCoordinator::deleteScanComplete);

        QList<FtpEntry> entries = {makeFile("file.prg")};
        mockFtp->mockSetDirectoryListing("/remote/dir", entries);
        mockFtp->mockProcessNextOperation();

        QCOMPARE(spy.count(), 1);
    }

    // -----------------------------------------------------------------------
    // handlesListing tests
    // -----------------------------------------------------------------------

    void testHandlesListing_trueForRequestedPath()
    {
        scanner->startDownloadScan("/remote/dir", "/local", "/remote/dir", 1);
        QVERIFY(scanner->handlesListing("/remote/dir"));
    }

    void testHandlesListing_falseForUnknownPath()
    {
        QVERIFY(!scanner->handlesListing("/unknown/path"));
    }
};

// ============================================================================
// FolderOperationCoordinator tests
// ============================================================================

class TestFolderOperationCoordinator : public QObject
{
    Q_OBJECT

private:
    transfer::State state_;
    MockFtpClient *mockFtp = nullptr;
    MockLocalFileSystem *mockFs = nullptr;
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
        mockFs = new MockLocalFileSystem(this);
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

    // -----------------------------------------------------------------------
    // enqueueRecursive download tests
    // -----------------------------------------------------------------------

    void testEnqueueRecursive_download_destNotExist_startsImmediately()
    {
        // Given: destination does not exist — no confirmation needed
        mockFs->mockSetDirectoryExists("/local/target/Games", false);
        state_.autoMerge = false;

        QSignalSpy spy(coordinator, &FolderOperationCoordinator::startDownloadScanRequested);

        coordinator->enqueueRecursive(transfer::OperationType::Download, "/remote/Games",
                                      "/local/target");

        // Started immediately — signal emitted from startFolderOperation
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("/remote/Games"));
    }

    void testEnqueueRecursive_download_destExists_queuesForDebounce()
    {
        // Given: destination exists — requires confirmation
        mockFs->mockSetDirectoryExists("/local/target/Games", true);
        state_.autoMerge = false;

        coordinator->enqueueRecursive(transfer::OperationType::Download, "/remote/Games",
                                      "/local/target");

        // Should be pending, not started yet
        QCOMPARE(state_.pendingFolderOps.size(), 1);
        QVERIFY(state_.pendingFolderOps.head().destExists);
    }

    void testEnqueueRecursive_download_autoMerge_skipsConfirmation()
    {
        // autoMerge skips confirmation even when destination exists
        mockFs->mockSetDirectoryExists("/local/target/Games", true);
        state_.autoMerge = true;

        QSignalSpy spy(coordinator, &FolderOperationCoordinator::startDownloadScanRequested);

        coordinator->enqueueRecursive(transfer::OperationType::Download, "/remote/Games",
                                      "/local/target");

        QCOMPARE(spy.count(), 1);
    }

    // -----------------------------------------------------------------------
    // enqueueRecursive upload tests
    // -----------------------------------------------------------------------

    void testEnqueueRecursive_upload_emitsDirectoryCreationRequested()
    {
        state_.autoMerge = true;

        QSignalSpy spy(coordinator, &FolderOperationCoordinator::startDirectoryCreationRequested);

        coordinator->enqueueRecursive(transfer::OperationType::Upload, "/local/mydir",
                                      "/remote/target");

        QCOMPARE(spy.count(), 1);
    }

    // -----------------------------------------------------------------------
    // Download replace: local dir is deleted and recreated via gateway
    // -----------------------------------------------------------------------

    void testDownloadReplace_deletesLocalDirBeforeDownload()
    {
        mockFs->mockSetDirectoryExists("/local/target/Games", true);
        state_.replaceExisting = true;

        transfer::PendingFolderOp op;
        op.operationType = transfer::OperationType::Download;
        op.sourcePath = "/remote/Games";
        op.destPath = "/local/target";
        op.targetPath = "/local/target/Games";
        op.destExists = true;
        state_.pendingFolderOps.enqueue(op);

        coordinator->startNextPendingFolderOp();

        // Local directory removed via gateway
        QVERIFY(mockFs->mockGetRemoveDirRequests().contains("/local/target/Games"));
    }

    void testDownloadReplace_createsMkpathAfterDelete()
    {
        mockFs->mockSetDirectoryExists("/local/target/Games", true);
        state_.replaceExisting = true;

        transfer::PendingFolderOp op;
        op.operationType = transfer::OperationType::Download;
        op.sourcePath = "/remote/Games";
        op.destPath = "/local/target";
        op.targetPath = "/local/target/Games";
        op.destExists = true;
        state_.pendingFolderOps.enqueue(op);

        coordinator->startNextPendingFolderOp();

        // mkpath called after delete
        QVERIFY(mockFs->mockGetMkpathRequests().contains("/local/target/Games"));
    }

    void testDownloadMerge_mkpathWithoutDelete()
    {
        mockFs->mockSetDirectoryExists("/local/target/Games", true);
        state_.replaceExisting = false;  // Merge

        transfer::PendingFolderOp op;
        op.operationType = transfer::OperationType::Download;
        op.sourcePath = "/remote/Games";
        op.destPath = "/local/target";
        op.targetPath = "/local/target/Games";
        op.destExists = true;
        state_.pendingFolderOps.enqueue(op);

        coordinator->startNextPendingFolderOp();

        // mkpath called; removeDirectoryRecursively NOT called
        QVERIFY(mockFs->mockGetMkpathRequests().contains("/local/target/Games"));
        QVERIFY(!mockFs->mockGetRemoveDirRequests().contains("/local/target/Games"));
    }

    // -----------------------------------------------------------------------
    // Upload replace: pendingUploadAfterDelete flag set
    // -----------------------------------------------------------------------

    void testUploadReplace_setsPendingUploadAfterDelete()
    {
        state_.replaceExisting = true;

        QSignalSpy spy(coordinator, &FolderOperationCoordinator::pendingUploadAfterDeleteSet);

        transfer::PendingFolderOp op;
        op.operationType = transfer::OperationType::Upload;
        op.sourcePath = "/local/mydir";
        op.destPath = "/remote/target";
        op.targetPath = "/remote/target/mydir";
        op.destExists = true;
        state_.pendingFolderOps.enqueue(op);

        coordinator->startNextPendingFolderOp();

        QVERIFY(state_.pendingUploadAfterDelete);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("/remote/target/mydir"));
    }

    // -----------------------------------------------------------------------
    // Duplicate detection
    // -----------------------------------------------------------------------

    void testEnqueueRecursive_duplicateIgnored()
    {
        // Simulate path already tracked via an active batch
        auto bResult = transfer::createBatch(state_, transfer::OperationType::Download,
                                             "Downloading Games", "Games", "/remote/Games");
        state_ = bResult.newState;

        QSignalSpy spy(coordinator, &FolderOperationCoordinator::statusMessage);

        coordinator->enqueueRecursive(transfer::OperationType::Download, "/remote/Games",
                                      "/local/target");

        // Status message emitted; nothing new queued
        QCOMPARE(spy.count(), 1);
    }

    // -----------------------------------------------------------------------
    // onFolderOperationComplete tests
    // -----------------------------------------------------------------------

    void testOnFolderOperationComplete_noPending_emitsAllCompleted()
    {
        QSignalSpy spy(coordinator, &FolderOperationCoordinator::allOperationsCompleted);

        coordinator->onFolderOperationComplete();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(state_.replaceExisting, false);
    }

    void testOnFolderOperationComplete_moreOps_startsNext()
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

// ============================================================================
// Main: run all test suites
// ============================================================================

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    int status = 0;

    {
        TestRemoteDirectoryCreator t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestRecursiveScanCoordinator t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestFolderOperationCoordinator t;
        status |= QTest::qExec(&t, argc, argv);
    }

    return status;
}

#include "test_coordinators.moc"
