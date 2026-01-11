#include <QtTest/QtTest>
#include <QSignalSpy>

#include "mocks/mockftpclient.h"
#include "services/filepreviewservice.h"

class TestFilePreviewService : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // Basic functionality
    void testRequestPreview();
    void testPreviewReady();
    void testPreviewFailed();
    void testNotConnected();
    void testCancelRequest();
    void testIsLoading();
    void testPendingPath();

    // Edge cases
    void testMultipleRequests();
    void testIgnoresUnrelatedDownloads();
    void testErrorDuringLoad();

private:
    MockFtpClient *mockFtp_ = nullptr;
    FilePreviewService *service_ = nullptr;
};

void TestFilePreviewService::init()
{
    mockFtp_ = new MockFtpClient(this);
    service_ = new FilePreviewService(mockFtp_, this);
}

void TestFilePreviewService::cleanup()
{
    delete service_;
    service_ = nullptr;
    delete mockFtp_;
    mockFtp_ = nullptr;
}

void TestFilePreviewService::testRequestPreview()
{
    mockFtp_->mockSetConnected(true);

    QSignalSpy startedSpy(service_, &FilePreviewService::previewStarted);

    service_->requestPreview("/test/file.txt");

    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(startedSpy.at(0).at(0).toString(), QString("/test/file.txt"));
    QVERIFY(service_->isLoading());
    QCOMPARE(service_->pendingPath(), QString("/test/file.txt"));
}

void TestFilePreviewService::testPreviewReady()
{
    mockFtp_->mockSetConnected(true);
    mockFtp_->mockSetDownloadData("/test/file.txt", QByteArray("Test content"));

    QSignalSpy readySpy(service_, &FilePreviewService::previewReady);

    service_->requestPreview("/test/file.txt");
    mockFtp_->mockProcessNextOperation();

    QCOMPARE(readySpy.count(), 1);
    QCOMPARE(readySpy.at(0).at(0).toString(), QString("/test/file.txt"));
    QCOMPARE(readySpy.at(0).at(1).toByteArray(), QByteArray("Test content"));
    QVERIFY(!service_->isLoading());
    QVERIFY(service_->pendingPath().isEmpty());
}

void TestFilePreviewService::testPreviewFailed()
{
    mockFtp_->mockSetConnected(true);
    mockFtp_->mockSetNextOperationFails("Download failed");

    QSignalSpy failedSpy(service_, &FilePreviewService::previewFailed);

    service_->requestPreview("/test/file.txt");
    mockFtp_->mockProcessNextOperation();

    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(failedSpy.at(0).at(0).toString(), QString("/test/file.txt"));
    QCOMPARE(failedSpy.at(0).at(1).toString(), QString("Download failed"));
    QVERIFY(!service_->isLoading());
}

void TestFilePreviewService::testNotConnected()
{
    // Don't set connected
    QSignalSpy failedSpy(service_, &FilePreviewService::previewFailed);

    service_->requestPreview("/test/file.txt");

    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(failedSpy.at(0).at(0).toString(), QString("/test/file.txt"));
    QVERIFY(failedSpy.at(0).at(1).toString().contains("Not connected"));
    QVERIFY(!service_->isLoading());
}

void TestFilePreviewService::testCancelRequest()
{
    mockFtp_->mockSetConnected(true);

    service_->requestPreview("/test/file.txt");
    QVERIFY(service_->isLoading());

    service_->cancelRequest();

    QVERIFY(!service_->isLoading());
    QVERIFY(service_->pendingPath().isEmpty());
}

void TestFilePreviewService::testIsLoading()
{
    mockFtp_->mockSetConnected(true);

    QVERIFY(!service_->isLoading());

    service_->requestPreview("/test/file.txt");
    QVERIFY(service_->isLoading());

    mockFtp_->mockSetDownloadData("/test/file.txt", QByteArray("data"));
    mockFtp_->mockProcessNextOperation();
    QVERIFY(!service_->isLoading());
}

void TestFilePreviewService::testPendingPath()
{
    mockFtp_->mockSetConnected(true);

    QVERIFY(service_->pendingPath().isEmpty());

    service_->requestPreview("/path/to/file.sid");
    QCOMPARE(service_->pendingPath(), QString("/path/to/file.sid"));

    mockFtp_->mockSetDownloadData("/path/to/file.sid", QByteArray("SID data"));
    mockFtp_->mockProcessNextOperation();
    QVERIFY(service_->pendingPath().isEmpty());
}

void TestFilePreviewService::testMultipleRequests()
{
    mockFtp_->mockSetConnected(true);
    mockFtp_->mockSetDownloadData("/file1.txt", QByteArray("content1"));
    mockFtp_->mockSetDownloadData("/file2.txt", QByteArray("content2"));

    QSignalSpy readySpy(service_, &FilePreviewService::previewReady);

    // First request
    service_->requestPreview("/file1.txt");
    mockFtp_->mockProcessNextOperation();

    // Second request
    service_->requestPreview("/file2.txt");
    mockFtp_->mockProcessNextOperation();

    QCOMPARE(readySpy.count(), 2);
    QCOMPARE(readySpy.at(0).at(0).toString(), QString("/file1.txt"));
    QCOMPARE(readySpy.at(1).at(0).toString(), QString("/file2.txt"));
}

void TestFilePreviewService::testIgnoresUnrelatedDownloads()
{
    mockFtp_->mockSetConnected(true);
    mockFtp_->mockSetDownloadData("/test/file.txt", QByteArray("test"));

    QSignalSpy readySpy(service_, &FilePreviewService::previewReady);

    // Request one file
    service_->requestPreview("/test/file.txt");

    // Simulate a download finishing for a different file (from another component)
    // This shouldn't trigger previewReady since it's not the pending path
    emit mockFtp_->downloadToMemoryFinished("/other/file.txt", QByteArray("other"));

    QCOMPARE(readySpy.count(), 0);
    QVERIFY(service_->isLoading());

    // Now process the actual request
    mockFtp_->mockProcessNextOperation();
    QCOMPARE(readySpy.count(), 1);
    QCOMPARE(readySpy.at(0).at(0).toString(), QString("/test/file.txt"));
}

void TestFilePreviewService::testErrorDuringLoad()
{
    mockFtp_->mockSetConnected(true);

    QSignalSpy failedSpy(service_, &FilePreviewService::previewFailed);

    service_->requestPreview("/test/file.txt");
    QVERIFY(service_->isLoading());

    // Simulate an FTP error
    emit mockFtp_->error("Connection lost");

    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(failedSpy.at(0).at(0).toString(), QString("/test/file.txt"));
    QCOMPARE(failedSpy.at(0).at(1).toString(), QString("Connection lost"));
    QVERIFY(!service_->isLoading());
}

QTEST_MAIN(TestFilePreviewService)
#include "test_filepreviewservice.moc"
