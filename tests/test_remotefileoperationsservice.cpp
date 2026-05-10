#include "services/remotefileoperationsservice.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

class TestRemoteFileOperationsService : public QObject
{
    Q_OBJECT

private slots:
    void testCreateFolder_NullFtpClient_EmitsOperationFailed();
    void testRenameItem_NullFtpClient_EmitsOperationFailed();
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

QTEST_MAIN(TestRemoteFileOperationsService)
#include "test_remotefileoperationsservice.moc"
