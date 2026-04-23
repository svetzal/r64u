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
};

QTEST_MAIN(TestTransferOrchestrator)
#include "test_transferorchestrator.moc"
