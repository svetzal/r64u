#include "ftp/remotelistingcoordinator.h"
#include "mocks/mockftpclient.h"

#include <QSignalSpy>
#include <QtTest>

Q_DECLARE_METATYPE(QList<FtpEntry>)

class TestRemoteListingCoordinator : public QObject
{
    Q_OBJECT

private:
    MockFtpClient *mockFtp;
    RemoteListingCoordinator *coordinator;

private slots:
    void init()
    {
        qRegisterMetaType<QList<FtpEntry>>();
        mockFtp = new MockFtpClient(this);
        coordinator = new RemoteListingCoordinator(this);
        mockFtp->mockSetConnected(true);
        coordinator->setFtpClient(mockFtp);
    }

    void cleanup()
    {
        delete coordinator;
        delete mockFtp;
        coordinator = nullptr;
        mockFtp = nullptr;
    }

    void testRequestListingIssuesFtpCall()
    {
        bool result = coordinator->requestListing("/SD");
        QVERIFY(result);
        QCOMPARE(mockFtp->mockGetListRequests().size(), 1);
        QCOMPARE(mockFtp->mockGetListRequests().first(), QString("/SD"));
    }

    void testRequestListingDeduplicated()
    {
        bool first = coordinator->requestListing("/SD");
        bool second = coordinator->requestListing("/SD");
        QVERIFY(first);
        QVERIFY(!second);
        // Only one list request should have been issued
        QCOMPARE(mockFtp->mockGetListRequests().size(), 1);
    }

    void testDifferentPathsAreNotDeduplicated()
    {
        bool first = coordinator->requestListing("/SD");
        bool second = coordinator->requestListing("/USB");
        QVERIFY(first);
        QVERIFY(second);
        QCOMPARE(mockFtp->mockGetListRequests().size(), 2);
    }

    void testListingReadyEmittedOnCompletion()
    {
        QList<FtpEntry> entries;
        FtpEntry file;
        file.name = "test.prg";
        file.isDirectory = false;
        entries << file;
        mockFtp->mockSetDirectoryListing("/SD", entries);

        QSignalSpy readySpy(coordinator, &RemoteListingCoordinator::listingReady);

        coordinator->requestListing("/SD");
        mockFtp->mockProcessAllOperations();

        QCOMPARE(readySpy.count(), 1);
        QCOMPARE(readySpy.first().first().toString(), QString("/SD"));
        QCOMPARE(readySpy.first().at(1).value<QList<FtpEntry>>().size(), 1);
    }

    void testUnrequestedListingIsIgnored()
    {
        QSignalSpy readySpy(coordinator, &RemoteListingCoordinator::listingReady);

        // Emit a listing for a path we never requested
        QList<FtpEntry> entries;
        emit mockFtp->directoryListed("/other", entries);

        QCOMPARE(readySpy.count(), 0);
    }

    void testOwnListingFails_OnlyThatPathFails()
    {
        QSignalSpy failedSpy(coordinator, &RemoteListingCoordinator::listingFailed);
        coordinator->requestListing("/SD");
        coordinator->requestListing("/USB");

        mockFtp->mockSetNextOperationFails("550 No such directory");
        mockFtp->mockProcessNextOperation();  // LIST /SD

        QCOMPARE(failedSpy.count(), 1);
        QCOMPARE(failedSpy.first().at(0).toString(), QString("/SD"));
        QCOMPARE(failedSpy.first().at(1).toString(), QString("550 No such directory"));
        QVERIFY(coordinator->requestListing("/SD"));    // may be requested again
        QVERIFY(!coordinator->requestListing("/USB"));  // still in flight
    }

    void testAnotherComponentsFailure_LeavesListingsPending()
    {
        QSignalSpy failedSpy(coordinator, &RemoteListingCoordinator::listingFailed);
        QSignalSpy abortedSpy(coordinator, &RemoteListingCoordinator::listingsAborted);
        coordinator->requestListing("/SD");

        // e.g. a transfer's RETR on the shared client
        const QString message = "Download failed for '/SD/x.prg': 550";
        emit mockFtp->error(message);
        emit mockFtp->operationFailed(IFtpClient::Operation::Download, "/SD/x.prg", "/tmp/x.prg",
                                      message);
        emit mockFtp->operationFailed(IFtpClient::Operation::List, "/elsewhere", QString(),
                                      message);

        QCOMPARE(failedSpy.count(), 0);
        QCOMPARE(abortedSpy.count(), 0);
        QVERIFY(!coordinator->requestListing("/SD"));  // still in flight
    }

    void testDisconnect_DropsAllPendingListings()
    {
        QSignalSpy abortedSpy(coordinator, &RemoteListingCoordinator::listingsAborted);
        coordinator->requestListing("/SD");
        coordinator->requestListing("/USB");

        mockFtp->mockSimulateDisconnect();

        QCOMPARE(abortedSpy.count(), 1);
        QVERIFY(coordinator->requestListing("/SD"));
        QVERIFY(coordinator->requestListing("/USB"));
    }

    void testConnectionLevelError_DropsAllPendingListings()
    {
        QSignalSpy abortedSpy(coordinator, &RemoteListingCoordinator::listingsAborted);
        coordinator->requestListing("/SD");
        {
            // A socket error or timeout ends the connection without disconnected()
            const QSignalBlocker blocker(mockFtp);
            mockFtp->mockSetConnected(false);
        }

        emit mockFtp->error("Connection refused");

        QCOMPARE(abortedSpy.count(), 1);
        QVERIFY(coordinator->requestListing("/SD"));
    }

    void testCancelPendingClearsAllState()
    {
        coordinator->requestListing("/SD");
        coordinator->requestListing("/USB");

        coordinator->cancelPending();

        // After cancel, paths are no longer pending — can request again
        bool sd = coordinator->requestListing("/SD");
        bool usb = coordinator->requestListing("/USB");
        QVERIFY(sd);
        QVERIFY(usb);
    }

    void testCancelPathClearsSinglePath()
    {
        coordinator->requestListing("/SD");
        coordinator->requestListing("/USB");

        coordinator->cancelPath("/SD");

        // /SD can be re-requested, /USB is still pending
        bool sdCanRequest = coordinator->requestListing("/SD");
        bool usbDeduplicated = coordinator->requestListing("/USB");
        QVERIFY(sdCanRequest);
        QVERIFY(!usbDeduplicated);
    }

    void testHasFtpClientReturnsFalseWhenNull()
    {
        RemoteListingCoordinator noClientCoordinator;
        QVERIFY(!noClientCoordinator.hasFtpClient());
    }

    void testHasFtpClientReturnsTrueWhenSet() { QVERIFY(coordinator->hasFtpClient()); }

    void testRequestListingReturnsFalseWithNoClient()
    {
        RemoteListingCoordinator noClientCoordinator;
        bool result = noClientCoordinator.requestListing("/SD");
        QVERIFY(!result);
    }

    void testListingReadyAllowsRepeatRequestAfterCompletion()
    {
        mockFtp->mockSetDirectoryListing("/SD", QList<FtpEntry>());

        QSignalSpy readySpy(coordinator, &RemoteListingCoordinator::listingReady);

        coordinator->requestListing("/SD");
        mockFtp->mockProcessAllOperations();

        QCOMPARE(readySpy.count(), 1);

        // After completion, the same path can be requested again
        bool second = coordinator->requestListing("/SD");
        QVERIFY(second);
        QCOMPARE(mockFtp->mockGetListRequests().size(), 2);
    }
};

QTEST_MAIN(TestRemoteListingCoordinator)
#include "test_remotelistingcoordinator.moc"
