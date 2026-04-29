#include "services/remotefileoperations.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

class TestRemoteFileOperations : public QObject
{
    Q_OBJECT

private slots:
    void testCreateFolder_NullFtpClient_EmitsOperationFailed();
    void testRenameItem_NullFtpClient_EmitsOperationFailed();
};

void TestRemoteFileOperations::testCreateFolder_NullFtpClient_EmitsOperationFailed()
{
    RemoteFileOperations rfo(nullptr);
    QSignalSpy spy(&rfo, &RemoteFileOperations::operationFailed);

    rfo.createFolder("/test/path");

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QString("Create folder"));
}

void TestRemoteFileOperations::testRenameItem_NullFtpClient_EmitsOperationFailed()
{
    RemoteFileOperations rfo(nullptr);
    QSignalSpy spy(&rfo, &RemoteFileOperations::operationFailed);

    rfo.renameItem("old", "new");

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QString("Rename"));
}

QTEST_MAIN(TestRemoteFileOperations)
#include "test_remotefileoperations.moc"
