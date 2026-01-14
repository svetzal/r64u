/**
 * @file test_transferservice.cpp
 * @brief Unit tests for TransferService.
 *
 * Tests verify:
 * - Operations are rejected when not connected
 * - Signals are properly forwarded from TransferQueue
 * - Queue state queries work correctly
 *
 * Note: "When connected" tests are skipped because DeviceConnection has
 * a complex state machine that requires both REST and FTP to be connected,
 * and we don't have a way to mock that state without significant refactoring.
 * The actual queueing behavior is tested in test_transferqueue.cpp.
 */

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "mocks/mockftpclient.h"
#include "services/transferservice.h"
#include "services/deviceconnection.h"
#include "models/transferqueue.h"

class TestTransferService : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // Connection state tests - verify operations fail when not connected
    void testUploadFailsWhenNotConnected();
    void testDownloadFailsWhenNotConnected();
    void testDeleteFailsWhenNotConnected();
    void testDeleteRecursiveFailsWhenNotConnected();

    // Signal forwarding tests - verify signals from queue are forwarded
    void testOperationStartedForwarded();
    void testOperationCompletedForwarded();
    void testOperationFailedForwarded();
    void testAllOperationsCompletedForwarded();
    void testOperationsCancelledForwarded();
    void testQueueChangedForwarded();
    void testDeleteProgressUpdateForwarded();
    void testOverwriteConfirmationForwarded();
    void testFolderExistsConfirmationForwarded();

    // Queue state query tests
    void testStateQueriesInitialState();
    void testQueueAccessor();

    // Queue management tests
    void testCancelAllEmitsSignal();

private:
    MockFtpClient *mockFtp_ = nullptr;
    DeviceConnection *connection_ = nullptr;
    TransferQueue *queue_ = nullptr;
    TransferService *service_ = nullptr;
    QTemporaryDir *tempDir_ = nullptr;
};

void TestTransferService::init()
{
    // Create temp dir for test files
    tempDir_ = new QTemporaryDir();
    QVERIFY(tempDir_->isValid());

    // Create a test file
    QFile testFile(tempDir_->filePath("testfile.txt"));
    QVERIFY(testFile.open(QIODevice::WriteOnly));
    testFile.write("test content");
    testFile.close();

    // Create mock and real objects
    mockFtp_ = new MockFtpClient(this);
    connection_ = new DeviceConnection(this);
    queue_ = new TransferQueue(this);

    // Inject mock FTP client into queue
    queue_->setFtpClient(mockFtp_);

    // Create service
    service_ = new TransferService(connection_, queue_, this);
}

void TestTransferService::cleanup()
{
    delete service_;
    service_ = nullptr;
    delete queue_;
    queue_ = nullptr;
    delete connection_;
    connection_ = nullptr;
    delete mockFtp_;
    mockFtp_ = nullptr;
    delete tempDir_;
    tempDir_ = nullptr;
}

// Connection state tests

void TestTransferService::testUploadFailsWhenNotConnected()
{
    // Not connected - operation should fail
    bool result = service_->uploadFile(tempDir_->filePath("testfile.txt"), "/remote/");
    QVERIFY(!result);

    // Also test directory upload
    result = service_->uploadDirectory(tempDir_->path(), "/remote/");
    QVERIFY(!result);
}

void TestTransferService::testDownloadFailsWhenNotConnected()
{
    bool result = service_->downloadFile("/remote/file.txt", tempDir_->path());
    QVERIFY(!result);

    // Also test directory download
    result = service_->downloadDirectory("/remote/folder", tempDir_->path());
    QVERIFY(!result);
}

void TestTransferService::testDeleteFailsWhenNotConnected()
{
    bool result = service_->deleteRemote("/remote/file.txt", false);
    QVERIFY(!result);

    // Also test directory delete
    result = service_->deleteRemote("/remote/folder", true);
    QVERIFY(!result);
}

void TestTransferService::testDeleteRecursiveFailsWhenNotConnected()
{
    bool result = service_->deleteRecursive("/remote/folder");
    QVERIFY(!result);
}

// Signal forwarding tests

void TestTransferService::testOperationStartedForwarded()
{
    QSignalSpy spy(service_, &TransferService::operationStarted);

    // Emit from queue
    emit queue_->operationStarted("file.txt", OperationType::Upload);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QString("file.txt"));
    QCOMPARE(spy.at(0).at(1).value<OperationType>(), OperationType::Upload);
}

void TestTransferService::testOperationCompletedForwarded()
{
    QSignalSpy spy(service_, &TransferService::operationCompleted);

    emit queue_->operationCompleted("file.txt");

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QString("file.txt"));
}

void TestTransferService::testOperationFailedForwarded()
{
    QSignalSpy spy(service_, &TransferService::operationFailed);

    emit queue_->operationFailed("file.txt", "Error message");

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QString("file.txt"));
    QCOMPARE(spy.at(0).at(1).toString(), QString("Error message"));
}

void TestTransferService::testAllOperationsCompletedForwarded()
{
    QSignalSpy spy(service_, &TransferService::allOperationsCompleted);

    emit queue_->allOperationsCompleted();

    QCOMPARE(spy.count(), 1);
}

void TestTransferService::testOperationsCancelledForwarded()
{
    QSignalSpy spy(service_, &TransferService::operationsCancelled);

    emit queue_->operationsCancelled();

    QCOMPARE(spy.count(), 1);
}

void TestTransferService::testQueueChangedForwarded()
{
    QSignalSpy spy(service_, &TransferService::queueChanged);

    emit queue_->queueChanged();

    QCOMPARE(spy.count(), 1);
}

void TestTransferService::testDeleteProgressUpdateForwarded()
{
    QSignalSpy spy(service_, &TransferService::deleteProgressUpdate);

    emit queue_->deleteProgressUpdate("file.txt", 5, 10);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QString("file.txt"));
    QCOMPARE(spy.at(0).at(1).toInt(), 5);
    QCOMPARE(spy.at(0).at(2).toInt(), 10);
}

void TestTransferService::testOverwriteConfirmationForwarded()
{
    QSignalSpy spy(service_, &TransferService::overwriteConfirmationNeeded);

    emit queue_->overwriteConfirmationNeeded("file.txt", OperationType::Download);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QString("file.txt"));
    QCOMPARE(spy.at(0).at(1).value<OperationType>(), OperationType::Download);
}

void TestTransferService::testFolderExistsConfirmationForwarded()
{
    QSignalSpy spy(service_, &TransferService::folderExistsConfirmationNeeded);

    emit queue_->folderExistsConfirmationNeeded(QStringList{"myfolder"});

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toStringList(), QStringList{"myfolder"});
}

// Queue state query tests

void TestTransferService::testStateQueriesInitialState()
{
    // Initial state - nothing processing
    QVERIFY(!service_->isProcessing());
    QVERIFY(!service_->isScanning());
    QVERIFY(!service_->isProcessingDelete());
    QVERIFY(!service_->isCreatingDirectories());
    QVERIFY(!service_->isScanningForDelete());
    QCOMPARE(service_->pendingCount(), 0);
    QCOMPARE(service_->activeCount(), 0);
    QCOMPARE(service_->totalCount(), 0);
    QCOMPARE(service_->deleteProgress(), 0);
    QCOMPARE(service_->deleteTotalCount(), 0);
}

void TestTransferService::testQueueAccessor()
{
    // Verify queue accessor returns the injected queue
    QCOMPARE(service_->queue(), queue_);
}

// Queue management tests

void TestTransferService::testCancelAllEmitsSignal()
{
    QSignalSpy spy(service_, &TransferService::operationsCancelled);

    // Cancel all (even with empty queue should emit signal)
    service_->cancelAll();

    // Queue should emit cancelled signal
    QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(TestTransferService)
#include "test_transferservice.moc"
