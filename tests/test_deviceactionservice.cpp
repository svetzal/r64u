#include "mocks/mockftpclient.h"
#include "mocks/mockrestclient.h"
#include "services/deviceactionservice.h"
#include "services/deviceconnectionmanager.h"

#include <QSignalSpy>
#include <QtTest>

class TestDeviceActionService : public QObject
{
    Q_OBJECT

private:
    // Creates a connection with a non-null MockRestClient (production-like, but disconnected).
    DeviceConnectionManager *makeConnection()
    {
        auto *restClient = new MockRestClient(this);
        auto *ftpClient = new MockFtpClient(this);
        return new DeviceConnectionManager(restClient, ftpClient, this);
    }

private slots:

    // ==========================================================================
    // canPerformOperations()
    // ==========================================================================

    void testCanPerformOperations_NullConnection_ReturnsFalse()
    {
        DeviceActionService service(nullptr, this);
        QVERIFY(!service.canPerformOperations());
    }

    void testCanPerformOperations_ConnectionWithRestClient_ReturnsTrue()
    {
        // DeviceActionService only requires a non-null REST client, not Connected state,
        // matching the original FileActionController guard for play/run/mount operations.
        auto *connection = makeConnection();
        DeviceActionService service(connection, this);
        QVERIFY(service.canPerformOperations());
    }

    // ==========================================================================
    // playSid() — emits operationNotAvailable only when restClient is null
    // ==========================================================================

    void testPlaySid_NullConnection_EmitsOperationNotAvailable()
    {
        DeviceActionService service(nullptr, this);
        QSignalSpy spy(&service, &DeviceActionService::operationNotAvailable);

        service.playSid("/music/song.sid");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("Play SID"));
    }

    void testPlaySid_WithRestClient_DoesNotEmitOperationNotAvailable()
    {
        auto *connection = makeConnection();
        DeviceActionService service(connection, this);
        QSignalSpy spy(&service, &DeviceActionService::operationNotAvailable);

        service.playSid("/music/song.sid");

        QCOMPARE(spy.count(), 0);
    }

    // ==========================================================================
    // playMod() — emits operationNotAvailable only when restClient is null
    // ==========================================================================

    void testPlayMod_NullConnection_EmitsOperationNotAvailable()
    {
        DeviceActionService service(nullptr, this);
        QSignalSpy spy(&service, &DeviceActionService::operationNotAvailable);

        service.playMod("/music/tune.mod");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("Play MOD"));
    }

    void testPlayMod_WithRestClient_DoesNotEmitOperationNotAvailable()
    {
        auto *connection = makeConnection();
        DeviceActionService service(connection, this);
        QSignalSpy spy(&service, &DeviceActionService::operationNotAvailable);

        service.playMod("/music/tune.mod");

        QCOMPARE(spy.count(), 0);
    }

    // ==========================================================================
    // runPrg() — emits operationNotAvailable only when restClient is null
    // ==========================================================================

    void testRunPrg_NullConnection_EmitsOperationNotAvailable()
    {
        DeviceActionService service(nullptr, this);
        QSignalSpy spy(&service, &DeviceActionService::operationNotAvailable);

        service.runPrg("/games/game.prg");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("Run PRG"));
    }

    // ==========================================================================
    // runCrt() — emits operationNotAvailable only when restClient is null
    // ==========================================================================

    void testRunCrt_NullConnection_EmitsOperationNotAvailable()
    {
        DeviceActionService service(nullptr, this);
        QSignalSpy spy(&service, &DeviceActionService::operationNotAvailable);

        service.runCrt("/games/game.crt");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("Run CRT"));
    }

    // ==========================================================================
    // mountImage() — emits operationNotAvailable only when restClient is null
    // ==========================================================================

    void testMountImage_NullConnection_EmitsOperationNotAvailable()
    {
        DeviceActionService service(nullptr, this);
        QSignalSpy spy(&service, &DeviceActionService::operationNotAvailable);

        service.mountImage("A", "/disks/game.d64");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("Mount image"));
    }

    void testMountImage_WithRestClient_DoesNotEmitOperationNotAvailable()
    {
        auto *connection = makeConnection();
        DeviceActionService service(connection, this);
        QSignalSpy spy(&service, &DeviceActionService::operationNotAvailable);

        service.mountImage("A", "/disks/game.d64");

        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TestDeviceActionService)
#include "test_deviceactionservice.moc"
