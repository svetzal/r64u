#include "mocks/mockftpclient.h"
#include "services/transfermanager.h"
#include "services/transfertimeoutmanager.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

using Status = TransferItem::Status;

class TestTransferManager : public QObject
{
    Q_OBJECT

private:
    MockFtpClient *mockFtp = nullptr;
    TransferManager *orchestrator = nullptr;
    QTemporaryDir tempDir;

    void flushAndProcess()
    {
        int iterations = 0;
        const int kMaxIterations = 100;

        while (iterations++ < kMaxIterations) {
            orchestrator->flushEventQueue();
            if (mockFtp->mockPendingOperationCount() == 0) {
                break;
            }
            mockFtp->mockProcessAllOperations();
        }
        orchestrator->flushEventQueue();
    }

    [[nodiscard]] Status itemStatus(int row) const
    {
        return orchestrator->state().items.at(row).status;
    }

    [[nodiscard]] QTimer *timeoutTimer() const
    {
        const auto *timeout = orchestrator->findChild<TransferTimeoutManager *>();
        return timeout ? timeout->findChild<QTimer *>() : nullptr;
    }

    [[nodiscard]] bool timeoutArmed() const
    {
        const QTimer *timer = timeoutTimer();
        return timer && timer->isActive();
    }

    [[nodiscard]] static FtpEntry remoteFile(const QString &name)
    {
        FtpEntry entry;
        entry.name = name;
        entry.isDirectory = false;
        return entry;
    }

    [[nodiscard]] static FtpEntry remoteDir(const QString &name)
    {
        FtpEntry entry;
        entry.name = name;
        entry.isDirectory = true;
        return entry;
    }

    QString createLocalFile(const QString &relativePath)
    {
        const QString path = tempDir.path() + "/" + relativePath;
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write("local");
        }
        return path;
    }

    void fireOperationTimeout()
    {
        auto *timeout = orchestrator->findChild<TransferTimeoutManager *>();
        QVERIFY(timeout);
        emit timeout->operationTimedOut();
    }

    /// Queues downloads of /r/<name> into the temp dir, each with mock content.
    void enqueueDownloads(const QStringList &names)
    {
        for (const QString &name : names) {
            mockFtp->mockSetDownloadData("/r/" + name, "x");
            orchestrator->enqueueDownload("/r/" + name, tempDir.path() + "/" + name);
        }
    }

    void flushAndProcessNext()
    {
        orchestrator->flushEventQueue();
        mockFtp->mockProcessNextOperation();
        orchestrator->flushEventQueue();
    }

    /// Downloads @p remoteDir into the temp dir, answering its first overwrite prompt
    /// with "Overwrite All", and runs it to completion.
    void answerOverwriteAllForFolder(const QString &remoteDir)
    {
        orchestrator->enqueueRecursiveDownload(remoteDir, tempDir.path());
        flushAndProcessNext();  // listing
        QCOMPARE(orchestrator->state().queueState, QueueState::AwaitingFileConfirm);
        orchestrator->respondToOverwrite(OverwriteResponse::OverwriteAll);
        flushAndProcess();
    }

private slots:
    void init()
    {
        mockFtp = new MockFtpClient(this);
        orchestrator = new TransferManager(this);
        orchestrator->setFtpClient(mockFtp);
        orchestrator->setAutoOverwrite(true);
        orchestrator->setAutoMerge(true);
        mockFtp->mockSetConnected(true);
    }

    void cleanup()
    {
        delete orchestrator;
        delete mockFtp;
        orchestrator = nullptr;
        mockFtp = nullptr;
    }

    void testEnqueueDownloadAddsItemToState()
    {
        QString remotePath = "/test/file.txt";
        QString localPath = tempDir.path() + "/file.txt";

        orchestrator->enqueueDownload(remotePath, localPath);

        QCOMPARE(orchestrator->state().items.size(), 1);
        QCOMPARE(orchestrator->state().items[0].remotePath, remotePath);
        QCOMPARE(orchestrator->state().items[0].localPath, localPath);
        QCOMPARE(orchestrator->state().items[0].operationType, OperationType::Download);
    }

    void testEnqueueUploadAddsItemToState()
    {
        // Create a real file so fileSize() doesn't fail
        QString localPath = tempDir.path() + "/upload.txt";
        QFile f(localPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("data");
        f.close();

        QString remotePath = "/remote/upload.txt";

        orchestrator->enqueueUpload(localPath, remotePath);

        QCOMPARE(orchestrator->state().items.size(), 1);
        QCOMPARE(orchestrator->state().items[0].localPath, localPath);
        QCOMPARE(orchestrator->state().items[0].remotePath, remotePath);
        QCOMPARE(orchestrator->state().items[0].operationType, OperationType::Upload);
    }

    void testProcessNextDispatchesDownloadWhenIdle()
    {
        QString remotePath = "/test/file.txt";
        QString localPath = tempDir.path() + "/file.txt";
        QByteArray content = "Hello";

        mockFtp->mockSetDownloadData(remotePath, content);

        orchestrator->enqueueDownload(remotePath, localPath);

        // After enqueue, processNext is scheduled
        orchestrator->flushEventQueue();

        // FTP download should have been requested
        QCOMPARE(mockFtp->mockGetDownloadRequests().size(), 1);
        QCOMPARE(mockFtp->mockGetDownloadRequests().first(), remotePath);
    }

    void testCancelAllMarksCancelledAndEmitsSignal()
    {
        QString remotePath = "/test/file.txt";
        QString localPath = tempDir.path() + "/file.txt";

        orchestrator->enqueueDownload(remotePath, localPath);

        QSignalSpy spy(orchestrator, &TransferManager::operationsCancelled);

        orchestrator->cancelAll();

        QCOMPARE(spy.count(), 1);

        // All items should be cancelled (no pending items)
        QCOMPARE(orchestrator->pendingCount(), 0);
    }

    void testSetFtpClientPropagatesToSubComponents()
    {
        // setFtpClient should not crash and should propagate to all sub-components
        MockFtpClient *anotherMock = new MockFtpClient(this);
        anotherMock->mockSetConnected(true);

        orchestrator->setFtpClient(anotherMock);

        // Verify the new client is used by enqueuing and processing
        QString remotePath = "/test/file.txt";
        QString localPath = tempDir.path() + "/file.txt";
        mockFtp->mockSetDownloadData(remotePath, QByteArray("data"));
        anotherMock->mockSetDownloadData(remotePath, QByteArray("data"));

        orchestrator->enqueueDownload(remotePath, localPath);
        orchestrator->flushEventQueue();

        // The new mock should have received the download request
        QCOMPARE(anotherMock->mockGetDownloadRequests().size(), 1);
        QCOMPARE(mockFtp->mockGetDownloadRequests().size(), 0);

        delete anotherMock;
    }

    void testStateTransitionsToTransferringOnStartTransfer()
    {
        QString remotePath = "/test/file.txt";
        QString localPath = tempDir.path() + "/file.txt";
        mockFtp->mockSetDownloadData(remotePath, QByteArray("data"));

        orchestrator->enqueueDownload(remotePath, localPath);
        orchestrator->flushEventQueue();

        // After processNext, state should be Transferring
        QCOMPARE(orchestrator->state().queueState, QueueState::Transferring);
    }

    void testEnqueueRecursiveUpload_NotConnected_EmitsOperationFailed()
    {
        mockFtp->mockSetConnected(false);
        QSignalSpy spy(orchestrator, &TransferManager::operationFailed);

        orchestrator->enqueueRecursiveUpload(tempDir.path(), "/remote/dir");

        QCOMPARE(spy.count(), 1);
    }

    void testEnqueueRecursiveDownload_NotConnected_EmitsOperationFailed()
    {
        mockFtp->mockSetConnected(false);
        QSignalSpy spy(orchestrator, &TransferManager::operationFailed);

        orchestrator->enqueueRecursiveDownload("/remote/dir", tempDir.path());

        QCOMPARE(spy.count(), 1);
    }

    void testEnqueueRecursiveDelete_NotConnected_EmitsOperationFailed()
    {
        mockFtp->mockSetConnected(false);
        QSignalSpy spy(orchestrator, &TransferManager::operationFailed);

        orchestrator->enqueueRecursiveDelete("/remote/dir");

        QCOMPARE(spy.count(), 1);
    }

    void testProcessNext_NoFtpClient_EmitsOperationFailed()
    {
        // Enqueue a download while connected so it enters the queue
        QString remotePath = "/test/file.txt";
        QString localPath = tempDir.path() + "/file.txt";
        orchestrator->enqueueDownload(remotePath, localPath);

        // Disconnect FTP before processNext runs
        mockFtp->mockSetConnected(false);

        QSignalSpy spy(orchestrator, &TransferManager::operationFailed);

        // Flush causes processNext to run, which now sees FTP not connected
        orchestrator->flushEventQueue();

        QCOMPARE(spy.count(), 1);
        // Verify the error is "not connected" (second argument of operationFailed)
        QCOMPARE(spy.first().at(1).toString(), tr("Not connected to device"));
    }

    void testProcessNextDelete_NoFtpClient_EmitsOperationFailed()
    {
        // Set up a recursive delete (connected), which starts a directory scan.
        // Provide an empty listing so the scan can complete.
        // Disconnect FTP before the scan result is processed, so that
        // processNextDelete() runs with no FTP client and emits operationFailed.
        QString remotePath = "/remote/dir";
        mockFtp->mockSetDirectoryListing(remotePath, {});  // empty directory

        orchestrator->enqueueRecursiveDelete(remotePath);

        // Disconnect FTP before the scan result is processed
        mockFtp->mockSetConnected(false);

        QSignalSpy spy(orchestrator, &TransferManager::operationFailed);

        // Processing the list response triggers deleteScanComplete -> processNextDelete
        // which now sees FTP disconnected and emits operationFailed
        mockFtp->mockProcessNextOperation();  // processes the list
        orchestrator->flushEventQueue();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(1).toString(), tr("Not connected to device"));
    }

    void testRespondToOverwrite_Skip_lastItemInBatch_emitsBatchCompleted()
    {
        // Arrange: disable auto-overwrite so the overwrite dialog is triggered,
        // then enqueue a single download whose local destination already exists.
        orchestrator->setAutoOverwrite(false);
        QString remotePath = "/remote/file.txt";
        QString localPath = tempDir.path() + "/file.txt";

        // Create the local file so the "file exists" check triggers the overwrite dialog
        QFile existingFile(localPath);
        QVERIFY(existingFile.open(QIODevice::WriteOnly));
        existingFile.write("existing content");
        existingFile.close();

        orchestrator->enqueueDownload(remotePath, localPath);

        // Flush so processNext runs and reaches AwaitingFileConfirm
        orchestrator->flushEventQueue();

        QCOMPARE(orchestrator->state().queueState, transfer::QueueState::AwaitingFileConfirm);

        QSignalSpy batchCompletedSpy(orchestrator, &TransferManager::batchCompleted);

        // Act: skip the only item in the batch — this should complete the batch
        orchestrator->respondToOverwrite(OverwriteResponse::Skip);

        // Assert: batchCompleted was emitted for the batch that contained the skipped item
        QCOMPARE(batchCompletedSpy.count(), 1);
    }

    // =========================================================================
    // The FTP client is shared: only react to the queue's own operations
    // =========================================================================

    void testForeignOperationFailure_DuringTransfer_DoesNotFailTheItem()
    {
        enqueueDownloads({"a", "b"});
        orchestrator->flushEventQueue();  // a in flight
        QSignalSpy failedSpy(orchestrator, &TransferManager::operationFailed);

        // A file preview on the same client fails
        const QString message = "Download failed for '/preview.sid': 550";
        emit mockFtp->error(message);
        emit mockFtp->operationFailed(IFtpClient::Operation::DownloadToMemory, "/preview.sid",
                                      QString(), message);
        orchestrator->flushEventQueue();

        QCOMPARE(failedSpy.count(), 0);
        QCOMPARE(itemStatus(0), Status::InProgress);
        QCOMPARE(orchestrator->state().queueState, QueueState::Transferring);
        QCOMPARE(mockFtp->mockGetDownloadRequests(), QStringList{"/r/a"});
    }

    void testForeignOperationFailure_DuringTransfer_BatchCompletesOnceWithAllItems()
    {
        enqueueDownloads({"a", "b", "c"});
        orchestrator->flushEventQueue();
        QSignalSpy batchCompletedSpy(orchestrator, &TransferManager::batchCompleted);
        QSignalSpy failedSpy(orchestrator, &TransferManager::operationFailed);

        emit mockFtp->operationFailed(IFtpClient::Operation::DownloadToMemory, "/preview.sid",
                                      QString(), "550");
        flushAndProcess();

        QCOMPARE(itemStatus(0), Status::Completed);
        QCOMPARE(itemStatus(1), Status::Completed);
        QCOMPARE(itemStatus(2), Status::Completed);
        QCOMPARE(batchCompletedSpy.count(), 1);
        QCOMPARE(failedSpy.count(), 0);
    }

    void testOwnOperationFailure_FailsTheInFlightItem()
    {
        enqueueDownloads({"a", "b"});
        orchestrator->flushEventQueue();
        QSignalSpy failedSpy(orchestrator, &TransferManager::operationFailed);

        mockFtp->mockSetNextOperationFails("550 No such file");
        mockFtp->mockProcessNextOperation();

        QCOMPARE(failedSpy.count(), 1);
        QCOMPARE(itemStatus(0), Status::Failed);
        QCOMPARE(orchestrator->state().items.at(0).errorMessage, QString("550 No such file"));
    }

    void testForeignDownloadProgress_WhileIdle_DoesNotArmTimeout()
    {
        QVERIFY(!timeoutArmed());

        emit mockFtp->downloadProgress("/preview.sid", 10, 100);

        QVERIFY(!timeoutArmed());
    }

    void testForeignDownloadProgress_DuringTransfer_DoesNotUpdateTheItem()
    {
        enqueueDownloads({"a"});
        orchestrator->flushEventQueue();
        QSignalSpy dataChangedSpy(orchestrator, &TransferManager::itemDataChanged);

        emit mockFtp->downloadProgress("/preview.sid", 10, 100);

        QCOMPARE(dataChangedSpy.count(), 0);
        QCOMPARE(orchestrator->state().items.at(0).bytesTransferred, qint64(0));
    }

    void testForeignDownloadFinished_DuringTransfer_KeepsTheQueueBusy()
    {
        enqueueDownloads({"a", "b"});
        orchestrator->flushEventQueue();

        emit mockFtp->downloadFinished("/elsewhere/x.prg", "/tmp/x.prg");
        orchestrator->flushEventQueue();

        QCOMPARE(orchestrator->state().queueState, QueueState::Transferring);
        QCOMPARE(mockFtp->mockGetDownloadRequests(), QStringList{"/r/a"});
    }

    // =========================================================================
    // Completions are matched to the item in flight, not the first path match
    // =========================================================================

    void testRedownloadAfterCancelAll_CompletesTheNewRow()
    {
        enqueueDownloads({"a"});
        orchestrator->flushEventQueue();
        orchestrator->cancelAll();
        QSignalSpy batchCompletedSpy(orchestrator, &TransferManager::batchCompleted);

        enqueueDownloads({"a"});
        flushAndProcess();

        QCOMPARE(orchestrator->state().items.size(), 2);
        QCOMPARE(itemStatus(0), Status::Failed);
        QCOMPARE(orchestrator->state().items.at(0).errorMessage, QString("Cancelled"));
        QCOMPARE(itemStatus(1), Status::Completed);
        QCOMPARE(batchCompletedSpy.count(), 1);
    }

    void testSameFileQueuedTwice_EachRowCompletesOnce()
    {
        QSignalSpy progressSpy(orchestrator, &TransferManager::batchProgressUpdate);
        enqueueDownloads({"a", "a"});

        flushAndProcess();

        QCOMPARE(itemStatus(0), Status::Completed);
        QCOMPARE(itemStatus(1), Status::Completed);
        QVERIFY(!progressSpy.isEmpty());
        QCOMPARE(progressSpy.last().at(1).toInt(), 2);  // completed
        QCOMPARE(progressSpy.last().at(2).toInt(), 2);  // total
    }

    // =========================================================================
    // Operation timeout
    // =========================================================================

    void testTimeout_CountsTheFailureAgainstTheBatch()
    {
        enqueueDownloads({"a", "b"});
        orchestrator->flushEventQueue();  // a in flight
        QSignalSpy batchCompletedSpy(orchestrator, &TransferManager::batchCompleted);
        const int batchId = orchestrator->state().batches.first().batchId;

        fireOperationTimeout();

        QCOMPARE(itemStatus(0), Status::Failed);
        QCOMPARE(orchestrator->batchProgress(batchId).failedItems, 1);

        flushAndProcess();

        QCOMPARE(itemStatus(1), Status::Completed);
        QCOMPARE(batchCompletedSpy.count(), 1);
    }

    void testTimeout_OnLastItem_CompletesTheBatchSoLaterDownloadsRun()
    {
        enqueueDownloads({"timeout-last"});
        orchestrator->flushEventQueue();
        QSignalSpy batchCompletedSpy(orchestrator, &TransferManager::batchCompleted);

        fireOperationTimeout();
        flushAndProcess();
        QCOMPARE(batchCompletedSpy.count(), 1);

        enqueueDownloads({"after-timeout"});
        flushAndProcess();

        const auto &items = orchestrator->state().items;
        QCOMPARE(items.last().remotePath, QString("/r/after-timeout"));
        QCOMPARE(items.last().status, Status::Completed);
    }

    void testTimeout_ReportsTheFailureOnce()
    {
        enqueueDownloads({"a", "b"});
        orchestrator->flushEventQueue();
        QSignalSpy failedSpy(orchestrator, &TransferManager::operationFailed);

        fireOperationTimeout();
        flushAndProcess();

        QCOMPARE(failedSpy.count(), 1);
        QVERIFY(failedSpy.first().at(1).toString().contains("timed out"));
    }

    void testTimeout_ReportsTheFailureOnce_WhenAbortReportsTheRequestSynchronously()
    {
        // A client that (unlike the contract) reports the aborted request
        class ReportingAbortFtpClient : public MockFtpClient
        {
        public:
            using MockFtpClient::MockFtpClient;
            QString remotePath;
            QString localPath;
            void abort() override
            {
                MockFtpClient::abort();
                emit error("Aborted");
                emit operationFailed(Operation::Download, remotePath, localPath, "Aborted");
            }
        };
        ReportingAbortFtpClient reportingFtp;
        reportingFtp.mockSetConnected(true);
        orchestrator->setFtpClient(&reportingFtp);
        reportingFtp.remotePath = "/r/a";
        reportingFtp.localPath = tempDir.path() + "/a";
        orchestrator->enqueueDownload(reportingFtp.remotePath, reportingFtp.localPath);
        orchestrator->flushEventQueue();
        QSignalSpy failedSpy(orchestrator, &TransferManager::operationFailed);

        fireOperationTimeout();
        orchestrator->flushEventQueue();

        QCOMPARE(failedSpy.count(), 1);
        QCOMPARE(itemStatus(0), Status::Failed);
        orchestrator->setFtpClient(mockFtp);
    }

    void testCancelAll_StopsTheTimeout()
    {
        enqueueDownloads({"a"});
        orchestrator->flushEventQueue();
        QVERIFY(timeoutArmed());

        orchestrator->cancelAll();

        QVERIFY(!timeoutArmed());
    }

    void testCancelBatch_OfTheRunningBatch_StopsTheTimeout()
    {
        enqueueDownloads({"a"});
        orchestrator->flushEventQueue();
        QVERIFY(timeoutArmed());

        orchestrator->cancelBatch(orchestrator->state().batches.first().batchId);

        QVERIFY(!timeoutArmed());
    }

    void testUploadProgress_OfTheInFlightUpload_RefreshesTheTimeout()
    {
        const QString localPath = tempDir.path() + "/progress-upload.prg";
        QFile file(localPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("data");
        file.close();
        orchestrator->enqueueUpload(localPath, "/r/progress-upload.prg");
        orchestrator->flushEventQueue();
        QVERIFY(timeoutArmed());
        timeoutTimer()->stop();

        emit mockFtp->uploadProgress(tempDir.path() + "/other.prg", 1, 4);
        QVERIFY(!timeoutArmed());

        emit mockFtp->uploadProgress(localPath, 2, 4);
        QVERIFY(timeoutArmed());
    }

    // =========================================================================
    // Disconnect / reconnect
    // =========================================================================

    void testDisconnect_FailsTheInFlightItemOnceAndGoesIdle()
    {
        enqueueDownloads({"a", "b"});
        orchestrator->flushEventQueue();  // a in flight
        QSignalSpy failedSpy(orchestrator, &TransferManager::operationFailed);

        mockFtp->mockSimulateDisconnect();
        orchestrator->flushEventQueue();

        QCOMPARE(itemStatus(0), Status::Failed);
        QCOMPARE(itemStatus(1), Status::Pending);
        QCOMPARE(orchestrator->state().queueState, QueueState::Idle);
        QCOMPARE(failedSpy.count(), 1);
        QVERIFY(!timeoutArmed());
    }

    void testReconnect_ResumesPendingItems()
    {
        enqueueDownloads({"a", "b"});
        orchestrator->flushEventQueue();
        mockFtp->mockSimulateDisconnect();
        orchestrator->flushEventQueue();
        mockFtp->mockReset();  // the real client drops its queue on disconnect
        mockFtp->mockSetDownloadData("/r/b", "x");

        mockFtp->mockSimulateConnect();
        flushAndProcess();

        QCOMPARE(itemStatus(1), Status::Completed);
    }

    void testEnqueue_AfterReconnect_RunsDespiteStaleActiveBatch()
    {
        enqueueDownloads({"a", "b"});
        orchestrator->flushEventQueue();
        mockFtp->mockSetNextOperationFails("Connection lost");
        mockFtp->mockSimulateDisconnect();
        mockFtp->mockProcessNextOperation();
        orchestrator->flushEventQueue();
        mockFtp->mockSetConnected(true);

        enqueueDownloads({"c"});
        flushAndProcess();

        QCOMPARE(itemStatus(1), Status::Completed);
        QCOMPARE(itemStatus(2), Status::Completed);
    }

    void testEnqueue_IntoActiveBatchWithNothingRunning_StartsTheQueue()
    {
        // The download cannot start while the client is not connected
        mockFtp->mockSetConnected(false);
        enqueueDownloads({"a"});
        orchestrator->flushEventQueue();
        QCOMPARE(itemStatus(0), Status::Pending);
        QVERIFY(orchestrator->hasActiveBatch());
        {
            // Connected again, but without the queue hearing about it
            const QSignalBlocker blocker(mockFtp);
            mockFtp->mockSetConnected(true);
        }

        enqueueDownloads({"b"});
        flushAndProcess();

        QCOMPARE(itemStatus(0), Status::Completed);
        QCOMPARE(itemStatus(1), Status::Completed);
    }

    // =========================================================================
    // A folder batch ending in a failure or skip hands over to the next batch
    // =========================================================================

    void testFolderBatch_LastItemFails_QueuedUploadStillRuns()
    {
        mockFtp->mockSetDirectoryListing("/r/fail-folder", {remoteFile("a")});
        const QString upload = createLocalFile("queued-after-fail.prg");
        orchestrator->enqueueRecursiveDownload("/r/fail-folder", tempDir.path());
        orchestrator->flushEventQueue();
        mockFtp->mockProcessNextOperation();  // listing
        orchestrator->flushEventQueue();      // download of a dispatched
        orchestrator->enqueueUpload(upload, "/r/queued-after-fail.prg");

        mockFtp->mockSetNextOperationFails("550 Permission denied");
        mockFtp->mockProcessNextOperation();
        flushAndProcess();

        QCOMPARE(mockFtp->mockGetUploadRequests(), QStringList{upload});
    }

    void testFolderBatch_LastItemSkipped_QueuedUploadStillRuns()
    {
        orchestrator->setAutoOverwrite(false);
        mockFtp->mockSetDirectoryListing("/r/skip-folder", {remoteFile("s")});
        createLocalFile("skip-folder/s");  // triggers the overwrite prompt
        const QString upload = createLocalFile("queued-after-skip.prg");
        orchestrator->enqueueRecursiveDownload("/r/skip-folder", tempDir.path());
        orchestrator->flushEventQueue();
        mockFtp->mockProcessNextOperation();  // listing
        orchestrator->flushEventQueue();
        QCOMPARE(orchestrator->state().queueState, QueueState::AwaitingFileConfirm);
        orchestrator->enqueueUpload(upload, "/r/queued-after-skip.prg");

        orchestrator->respondToOverwrite(OverwriteResponse::Skip);
        flushAndProcess();

        QCOMPARE(mockFtp->mockGetUploadRequests(), QStringList{upload});
    }

    void testFolderBatch_LastItemFails_AllOperationsCompletedOnlyAfterQueuedUpload()
    {
        mockFtp->mockSetDirectoryListing("/r/fail-folder-2", {remoteFile("a")});
        const QString upload = createLocalFile("queued-after-fail-2.prg");
        orchestrator->enqueueRecursiveDownload("/r/fail-folder-2", tempDir.path());
        orchestrator->flushEventQueue();
        mockFtp->mockProcessNextOperation();
        orchestrator->flushEventQueue();
        orchestrator->enqueueUpload(upload, "/r/queued-after-fail-2.prg");
        QSignalSpy allDoneSpy(orchestrator, &TransferManager::allOperationsCompleted);

        mockFtp->mockSetNextOperationFails("550 Permission denied");
        mockFtp->mockProcessNextOperation();
        orchestrator->flushEventQueue();

        QCOMPARE(allDoneSpy.count(), 0);
    }

    void testBatchProgress_CountsFailedItemsAsProcessed()
    {
        enqueueDownloads({"p1", "p2", "p3"});
        QSignalSpy progressSpy(orchestrator, &TransferManager::batchProgressUpdate);
        orchestrator->flushEventQueue();
        mockFtp->mockSetNextOperationFails("550 No such file");

        flushAndProcess();

        QList<int> processed;
        for (const auto &args : progressSpy) {
            processed << args.at(1).toInt();
        }
        QCOMPARE(processed, (QList<int>{1, 2, 3}));
    }

    // =========================================================================
    // Upload "file exists?" check
    // =========================================================================

    void testUploadExistsCheck_WhileListing_QueueDispatchesNothingElse()
    {
        orchestrator->setAutoOverwrite(false);
        const QString upload = createLocalFile("checked-upload.prg");
        orchestrator->enqueueUpload(upload, "/r/checked-upload.prg");
        orchestrator->flushEventQueue();
        QCOMPARE(mockFtp->mockGetListRequests(), QStringList{"/r"});

        enqueueDownloads({"while-checking"});
        orchestrator->flushEventQueue();

        QCOMPARE(mockFtp->mockGetListRequests(), QStringList{"/r"});
        QVERIFY(mockFtp->mockGetDownloadRequests().isEmpty());

        flushAndProcess();

        QCOMPARE(mockFtp->mockGetUploadRequests(), QStringList{upload});
        QCOMPARE(mockFtp->mockGetDownloadRequests(), QStringList{"/r/while-checking"});
    }

    void testUploadExistsCheck_CancelAll_IgnoresTheLateListing()
    {
        orchestrator->setAutoOverwrite(false);
        mockFtp->mockSetDirectoryListing("/r", {remoteFile("cancelled-check.prg")});
        const QString upload = createLocalFile("cancelled-check.prg");
        orchestrator->enqueueUpload(upload, "/r/cancelled-check.prg");
        orchestrator->flushEventQueue();
        QSignalSpy confirmSpy(orchestrator, &TransferManager::overwriteConfirmationNeeded);

        orchestrator->cancelAll();
        flushAndProcess();

        QCOMPARE(confirmSpy.count(), 0);
        QCOMPARE(orchestrator->state().queueState, QueueState::Idle);
        QVERIFY(mockFtp->mockGetUploadRequests().isEmpty());
    }

    void testUploadExistsCheck_Disconnect_KeepsTheItemForReconnect()
    {
        orchestrator->setAutoOverwrite(false);
        const QString upload = createLocalFile("check-then-disconnect.prg");
        orchestrator->enqueueUpload(upload, "/r/check-then-disconnect.prg");
        orchestrator->flushEventQueue();

        mockFtp->mockSimulateDisconnect();
        mockFtp->mockReset();  // the real client drops its queue on disconnect
        orchestrator->flushEventQueue();
        QCOMPARE(orchestrator->state().queueState, QueueState::Idle);
        QCOMPARE(itemStatus(0), Status::Pending);

        mockFtp->mockSimulateConnect();
        flushAndProcess();

        QCOMPARE(mockFtp->mockGetUploadRequests(), QStringList{upload});
    }

    // =========================================================================
    // "Overwrite All" applies to the batch it was chosen in, nothing later
    // =========================================================================

    void testOverwriteAll_InFolderDownload_CoversTheRestOfThatFolder()
    {
        orchestrator->setAutoOverwrite(false);
        mockFtp->mockSetDirectoryListing("/r/ow-all", {remoteFile("a"), remoteFile("b")});
        createLocalFile("ow-all/a");
        createLocalFile("ow-all/b");
        QSignalSpy confirmSpy(orchestrator, &TransferManager::overwriteConfirmationNeeded);

        answerOverwriteAllForFolder("/r/ow-all");

        QCOMPARE(confirmSpy.count(), 1);
        QCOMPARE(mockFtp->mockGetDownloadRequests(), (QStringList{"/r/ow-all/a", "/r/ow-all/b"}));
    }

    void testOverwriteAll_InFolderDownload_LaterSingleDownloadStillAsks()
    {
        orchestrator->setAutoOverwrite(false);
        mockFtp->mockSetDirectoryListing("/r/ow-folder", {remoteFile("a")});
        createLocalFile("ow-folder/a");
        answerOverwriteAllForFolder("/r/ow-folder");
        const QString existing = createLocalFile("later.prg");
        QSignalSpy confirmSpy(orchestrator, &TransferManager::overwriteConfirmationNeeded);

        orchestrator->enqueueDownload("/r/later.prg", existing);
        orchestrator->flushEventQueue();

        QCOMPARE(confirmSpy.count(), 1);
        QCOMPARE(orchestrator->state().queueState, QueueState::AwaitingFileConfirm);
        QVERIFY(!mockFtp->mockGetDownloadRequests().contains("/r/later.prg"));
    }

    void testOverwriteAll_InFolderDownload_LaterUploadStillChecksTheRemoteFile()
    {
        orchestrator->setAutoOverwrite(false);
        mockFtp->mockSetDirectoryListing("/r/ow-folder-2", {remoteFile("a")});
        createLocalFile("ow-folder-2/a");
        answerOverwriteAllForFolder("/r/ow-folder-2");
        const QString upload = createLocalFile("later-upload.prg");

        orchestrator->enqueueUpload(upload, "/r/later-upload.prg");
        orchestrator->flushEventQueue();

        QCOMPARE(orchestrator->state().queueState, QueueState::CheckingUploadTarget);
        QVERIFY(mockFtp->mockGetListRequests().contains("/r"));
        QVERIFY(mockFtp->mockGetUploadRequests().isEmpty());
    }

    void testOverwriteAll_InRunningFolderDownload_SingleDownloadQueuedMeanwhileStillAsks()
    {
        orchestrator->setAutoOverwrite(false);
        mockFtp->mockSetDirectoryListing("/r/ow-busy", {remoteFile("a"), remoteFile("b")});
        createLocalFile("ow-busy/a");
        createLocalFile("ow-busy/b");
        orchestrator->enqueueRecursiveDownload("/r/ow-busy", tempDir.path());
        flushAndProcessNext();  // listing
        orchestrator->respondToOverwrite(OverwriteResponse::OverwriteAll);
        orchestrator->flushEventQueue();  // a in flight
        const QString existing = createLocalFile("meanwhile.prg");
        QSignalSpy confirmSpy(orchestrator, &TransferManager::overwriteConfirmationNeeded);

        orchestrator->enqueueDownload("/r/meanwhile.prg", existing);
        flushAndProcess();

        QCOMPARE(confirmSpy.count(), 1);
        QVERIFY(!mockFtp->mockGetDownloadRequests().contains("/r/meanwhile.prg"));
    }

    // =========================================================================
    // Empty folder downloads complete and let queued folder operations run
    // =========================================================================

    void testEmptyFolderDownload_Completes()
    {
        mockFtp->mockSetDirectoryListing("/r/empty-only", {});
        QSignalSpy batchCompletedSpy(orchestrator, &TransferManager::batchCompleted);
        QSignalSpy allDoneSpy(orchestrator, &TransferManager::allOperationsCompleted);

        orchestrator->enqueueRecursiveDownload("/r/empty-only", tempDir.path());
        flushAndProcess();

        QCOMPARE(batchCompletedSpy.count(), 1);
        QCOMPARE(allDoneSpy.count(), 1);
        QCOMPARE(orchestrator->queuedBatchCount(), 0);
        QCOMPARE(orchestrator->state().queueState, QueueState::Idle);
        QVERIFY(!orchestrator->isPathBeingTransferred("/r/empty-only", OperationType::Download));
    }

    void testEmptyFolderDownload_FolderQueuedBehindItStillRuns()
    {
        mockFtp->mockSetDirectoryListing("/r/empty", {});
        mockFtp->mockSetDirectoryListing("/r/full", {remoteFile("f.prg")});
        mockFtp->mockSetDownloadData("/r/full/f.prg", "x");
        QSignalSpy batchCompletedSpy(orchestrator, &TransferManager::batchCompleted);
        QSignalSpy allDoneSpy(orchestrator, &TransferManager::allOperationsCompleted);

        orchestrator->enqueueRecursiveDownload("/r/empty", tempDir.path());
        orchestrator->enqueueRecursiveDownload("/r/full", tempDir.path());
        flushAndProcess();

        QVERIFY(QFile::exists(tempDir.path() + "/full/f.prg"));
        QCOMPARE(batchCompletedSpy.count(), 2);
        QCOMPARE(allDoneSpy.count(), 1);
        QCOMPARE(orchestrator->queuedBatchCount(), 0);
    }

    // =========================================================================
    // A folder that cannot be listed during a scan is reported, the rest is kept
    // =========================================================================

    void testScanListingFails_ReportsOnceAndDownloadsTheRestOfTheTree()
    {
        mockFtp->mockSetDirectoryListing(
            "/r/scan", {remoteDir("bad"), remoteDir("good"), remoteFile("top.prg")});
        mockFtp->mockSetDirectoryListing("/r/scan/good", {remoteFile("g.prg")});
        QSignalSpy failedSpy(orchestrator, &TransferManager::operationFailed);
        QSignalSpy batchCompletedSpy(orchestrator, &TransferManager::batchCompleted);
        orchestrator->enqueueRecursiveDownload("/r/scan", tempDir.path());
        flushAndProcessNext();  // root listing

        mockFtp->mockSetNextOperationFails("550 Permission denied");
        flushAndProcessNext();  // listing of bad fails
        flushAndProcess();

        QCOMPARE(failedSpy.count(), 1);
        QCOMPARE(failedSpy.first().at(0).toString(), QString("bad"));
        QCOMPARE(failedSpy.first().at(1).toString(), QString("550 Permission denied"));
        QVERIFY(mockFtp->mockGetDownloadRequests().contains("/r/scan/top.prg"));
        QVERIFY(mockFtp->mockGetDownloadRequests().contains("/r/scan/good/g.prg"));
        QCOMPARE(batchCompletedSpy.count(), 1);
        QCOMPARE(orchestrator->queuedBatchCount(), 0);
        QCOMPARE(orchestrator->state().queueState, QueueState::Idle);
    }

    void testScanListingFails_FailureIsCountedAgainstTheBatch()
    {
        mockFtp->mockSetDirectoryListing("/r/scan-count", {remoteDir("bad"), remoteFile("a")});
        QSignalSpy progressSpy(orchestrator, &TransferManager::batchProgressUpdate);
        orchestrator->enqueueRecursiveDownload("/r/scan-count", tempDir.path());
        flushAndProcessNext();  // root listing

        mockFtp->mockSetNextOperationFails("550 Permission denied");
        flushAndProcessNext();
        flushAndProcess();

        QVERIFY(!progressSpy.isEmpty());
        QCOMPARE(progressSpy.last().at(1).toInt(), 2);  // processed: the folder and a
        QCOMPARE(progressSpy.last().at(2).toInt(), 2);  // total
    }

    void testScanListingFails_OnlyFolderFails_QueuedFolderStillRuns()
    {
        mockFtp->mockSetDirectoryListing("/r/after-fail", {remoteFile("f.prg")});
        QSignalSpy allDoneSpy(orchestrator, &TransferManager::allOperationsCompleted);
        orchestrator->enqueueRecursiveDownload("/r/unlistable", tempDir.path());
        orchestrator->enqueueRecursiveDownload("/r/after-fail", tempDir.path());

        mockFtp->mockSetNextOperationFails("550 No such directory");
        flushAndProcessNext();
        flushAndProcess();

        QVERIFY(mockFtp->mockGetDownloadRequests().contains("/r/after-fail/f.prg"));
        QCOMPARE(allDoneSpy.count(), 1);
        QCOMPARE(orchestrator->queuedBatchCount(), 0);
    }

    void testOverwriteAll_ThenCancelAll_NextDownloadStillAsks()
    {
        orchestrator->setAutoOverwrite(false);
        const QString first = createLocalFile("first.prg");
        const QString second = createLocalFile("second.prg");
        orchestrator->enqueueDownload("/r/first.prg", first);
        orchestrator->enqueueDownload("/r/second.prg", second);
        orchestrator->flushEventQueue();
        orchestrator->respondToOverwrite(OverwriteResponse::OverwriteAll);
        orchestrator->flushEventQueue();  // first in flight
        orchestrator->cancelAll();
        QSignalSpy confirmSpy(orchestrator, &TransferManager::overwriteConfirmationNeeded);

        orchestrator->enqueueDownload("/r/second.prg", second);
        orchestrator->flushEventQueue();

        QCOMPARE(confirmSpy.count(), 1);
    }
};

QTEST_MAIN(TestTransferManager)
#include "test_transfermanager.moc"
