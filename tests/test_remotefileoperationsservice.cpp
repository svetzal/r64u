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
    void testCreateFolder_Fails_ReportsTheFailureOnce();
    void testRenameItem_Fails_ReportsTheFailureOnce();
    void testAnotherComponentsFailedRequest_IsNotReported();
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

void TestRemoteFileOperationsService::testCreateFolder_Fails_ReportsTheFailureOnce()
{
    MockFtpClient mock;
    mock.mockSetConnected(true);
    RemoteFileOperationsService rfo(&mock);
    QSignalSpy failedSpy(&rfo, &RemoteFileOperationsService::operationFailed);
    QSignalSpy reportedSpy(&rfo, &IErrorEmitter::errorReported);
    rfo.createFolder("/SD/new");

    mock.mockSetNextOperationFails("Cannot create directory '/SD/new': 550");
    mock.mockProcessNextOperation();

    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(failedSpy.first().at(0).toString(), QString("Create folder"));
    QCOMPARE(reportedSpy.count(), 1);
    QVERIFY(reportedSpy.first().at(3).toString().contains("550"));
}

void TestRemoteFileOperationsService::testRenameItem_Fails_ReportsTheFailureOnce()
{
    MockFtpClient mock;
    mock.mockSetConnected(true);
    RemoteFileOperationsService rfo(&mock);
    QSignalSpy failedSpy(&rfo, &RemoteFileOperationsService::operationFailed);
    QSignalSpy reportedSpy(&rfo, &IErrorEmitter::errorReported);
    rfo.renameItem("/SD/old.prg", "/SD/new.prg");

    mock.mockSetNextOperationFails("Cannot rename '/SD/old.prg': 550");
    mock.mockProcessNextOperation();

    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(failedSpy.first().at(0).toString(), QString("Rename"));
    QCOMPARE(reportedSpy.count(), 1);
}

void TestRemoteFileOperationsService::testAnotherComponentsFailedRequest_IsNotReported()
{
    MockFtpClient mock;
    mock.mockSetConnected(true);
    RemoteFileOperationsService rfo(&mock);
    QSignalSpy reportedSpy(&rfo, &IErrorEmitter::errorReported);
    rfo.createFolder("/SD/new");

    // e.g. the transfer queue creating the folders of an upload on the shared client
    emit mock.operationFailed(IFtpClient::Operation::MakeDirectory, "/SD/upload/sub", QString(),
                              "550");
    emit mock.operationFailed(IFtpClient::Operation::Rename, "/SD/new", QString(), "550");

    QCOMPARE(reportedSpy.count(), 0);
}

QTEST_MAIN(TestRemoteFileOperationsService)
#include "test_remotefileoperationsservice.moc"
