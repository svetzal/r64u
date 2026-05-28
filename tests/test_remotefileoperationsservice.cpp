#include "mocks/mockftpclient.h"
#include "services/remotefileoperationsservice.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

class TestRemoteFileOperationsService : public QObject
{
    Q_OBJECT

private slots:
    void testCreateFolder_NullFtpClient_EmitsOperationFailed();
    void testRenameItem_NullFtpClient_EmitsOperationFailed();
    void testCreateFolder_WithFtpClient_QueuesMkdirRequest();
    void testCreateFolder_WithFtpClient_ForwardsFolderCreated();
    void testRenameItem_WithFtpClient_ForwardsItemRenamed();
    void testItemRemoved_WhenFtpClientEmits_ForwardsSignal();
};

void TestRemoteFileOperationsService::testCreateFolder_NullFtpClient_EmitsOperationFailed()
{
    RemoteFileOperationsService rfo(nullptr);
    QSignalSpy spy(&rfo, &RemoteFileOperationsService::operationFailed);

    rfo.createFolder("/test/path");

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QString("Create folder"));
}

void TestRemoteFileOperationsService::testRenameItem_NullFtpClient_EmitsOperationFailed()
{
    RemoteFileOperationsService rfo(nullptr);
    QSignalSpy spy(&rfo, &RemoteFileOperationsService::operationFailed);

    rfo.renameItem("old", "new");

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QString("Rename"));
}

void TestRemoteFileOperationsService::testCreateFolder_WithFtpClient_QueuesMkdirRequest()
{
    MockFtpClient mock;
    mock.mockSetConnected(true);
    RemoteFileOperationsService rfo(&mock);

    rfo.createFolder("/test/dir");

    QVERIFY(mock.mockGetMkdirRequests().contains("/test/dir"));
}

void TestRemoteFileOperationsService::testCreateFolder_WithFtpClient_ForwardsFolderCreated()
{
    MockFtpClient mock;
    mock.mockSetConnected(true);
    RemoteFileOperationsService rfo(&mock);
    QSignalSpy spy(&rfo, &RemoteFileOperationsService::folderCreated);

    rfo.createFolder("/test/dir");
    mock.mockProcessNextOperation();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QString("/test/dir"));
}

void TestRemoteFileOperationsService::testRenameItem_WithFtpClient_ForwardsItemRenamed()
{
    MockFtpClient mock;
    mock.mockSetConnected(true);
    RemoteFileOperationsService rfo(&mock);
    QSignalSpy spy(&rfo, &RemoteFileOperationsService::itemRenamed);

    rfo.renameItem("/old/path", "/new/path");
    mock.mockProcessNextOperation();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QString("/old/path"));
    QCOMPARE(spy.at(0).at(1).toString(), QString("/new/path"));
}

void TestRemoteFileOperationsService::testItemRemoved_WhenFtpClientEmits_ForwardsSignal()
{
    MockFtpClient mock;
    mock.mockSetConnected(true);
    RemoteFileOperationsService rfo(&mock);
    QSignalSpy spy(&rfo, &RemoteFileOperationsService::itemRemoved);

    mock.remove("/some/file");
    mock.mockProcessNextOperation();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QString("/some/file"));
}

QTEST_MAIN(TestRemoteFileOperationsService)
#include "test_remotefileoperationsservice.moc"
