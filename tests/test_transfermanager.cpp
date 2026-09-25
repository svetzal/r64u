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
};

QTEST_MAIN(TestTransferManager)
#include "test_transfermanager.moc"
