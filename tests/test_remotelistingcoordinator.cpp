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

    void testFtpErrorClearsPendingAndEmitsFailure()
    {
        QSignalSpy failedSpy(coordinator, &RemoteListingCoordinator::listingFailed);

        coordinator->requestListing("/SD");

        // Simulate FTP error
        emit mockFtp->error("Connection lost");

        QCOMPARE(failedSpy.count(), 1);
        QCOMPARE(failedSpy.first().first().toString(), QString("Connection lost"));

        // After error, same path can be requested again (not deduplicated)
        bool canRequest = coordinator->requestListing("/SD");
        QVERIFY(canRequest);
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
