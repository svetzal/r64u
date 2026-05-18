#include "mocks/mockftpclient.h"
#include "models/transferorchestrator.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class TestTransferOrchestrator : public QObject
{
    Q_OBJECT

private:
    MockFtpClient *mockFtp = nullptr;
    TransferOrchestrator *orchestrator = nullptr;
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
        orchestrator = new TransferOrchestrator(this);
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

        QSignalSpy spy(orchestrator, &TransferOrchestrator::operationsCancelled);

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
        QSignalSpy spy(orchestrator, &TransferOrchestrator::operationFailed);

        orchestrator->enqueueRecursiveUpload(tempDir.path(), "/remote/dir");

        QCOMPARE(spy.count(), 1);
    }

    void testEnqueueRecursiveDownload_NotConnected_EmitsOperationFailed()
    {
        mockFtp->mockSetConnected(false);
        QSignalSpy spy(orchestrator, &TransferOrchestrator::operationFailed);

        orchestrator->enqueueRecursiveDownload("/remote/dir", tempDir.path());

        QCOMPARE(spy.count(), 1);
    }

    void testEnqueueRecursiveDelete_NotConnected_EmitsOperationFailed()
    {
        mockFtp->mockSetConnected(false);
        QSignalSpy spy(orchestrator, &TransferOrchestrator::operationFailed);

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

        QSignalSpy spy(orchestrator, &TransferOrchestrator::operationFailed);

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

        QSignalSpy spy(orchestrator, &TransferOrchestrator::operationFailed);

        // Processing the list response triggers deleteScanComplete -> processNextDelete
        // which now sees FTP disconnected and emits operationFailed
        mockFtp->mockProcessNextOperation();  // processes the list
        orchestrator->flushEventQueue();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(1).toString(), tr("Not connected to device"));
    }
};

QTEST_MAIN(TestTransferOrchestrator)
#include "test_transferorchestrator.moc"
