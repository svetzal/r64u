/**
 * @file test_transferprogresscontainer.cpp
 * @brief Unit tests for TransferProgressContainer initialization and service wiring.
 *
 * Tests verify:
 * - Construction does not crash
 * - Widget is initially hidden
 * - setTransferService(nullptr) does not crash
 * - setTransferService() with a real service does not crash
 */

#include "mocks/mockftpclient.h"
#include "mocks/mockrestclient.h"
#include "models/transferqueue.h"
#include "services/deviceconnectionmanager.h"
#include "services/transferservice.h"
#include "ui/transferprogresscontainer.h"

#include <QtTest>

class TestTransferProgressContainer : public QObject
{
    Q_OBJECT

private:
    MockRestClient *mockRest_ = nullptr;
    MockFtpClient *mockFtp_ = nullptr;
    DeviceConnectionManager *connection_ = nullptr;
    TransferQueue *queue_ = nullptr;
    TransferService *service_ = nullptr;

    void makeService()
    {
        mockRest_ = new MockRestClient(this);
        mockFtp_ = new MockFtpClient(this);
        connection_ = new DeviceConnectionManager(mockRest_, mockFtp_, this);
        queue_ = new TransferQueue(this);
        service_ = new TransferService(connection_, queue_, this);
    }

private slots:

    void testConstruct_doesNotCrash()
    {
        TransferProgressContainer container;
        QVERIFY(true);
    }

    void testConstruct_initiallyHidden()
    {
        TransferProgressContainer container;
        QVERIFY(!container.isVisible());
    }

    void testSetTransferService_nullptr_doesNotCrash()
    {
        TransferProgressContainer container;
        container.setTransferService(nullptr);
        QVERIFY(true);
    }

    void testSetTransferService_called_doesNotCrash()
    {
        makeService();
        TransferProgressContainer container;
        container.setTransferService(service_);
        QVERIFY(true);
    }

    void testOnOperationCompleted_EmitsStatusMessage()
    {
        TransferProgressContainer container;
        QSignalSpy spy(&container, &TransferProgressContainer::statusMessage);

        QMetaObject::invokeMethod(&container, "onOperationStarted", Q_ARG(QString, "test.sid"),
                                  Q_ARG(OperationType, OperationType::Download));
        QMetaObject::invokeMethod(&container, "onOperationCompleted", Q_ARG(QString, "test.sid"));

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains("test.sid"));
    }

    void testOnOperationFailed_EmitsStatusMessage()
    {
        TransferProgressContainer container;
        QSignalSpy spy(&container, &TransferProgressContainer::statusMessage);

        QMetaObject::invokeMethod(&container, "onOperationFailed", Q_ARG(QString, "broken.d64"),
                                  Q_ARG(QString, "Connection reset"));

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(1).toInt() > 0);
    }

    void testOnAllOperationsCompleted_EmitsClearAndStatus()
    {
        TransferProgressContainer container;
        QSignalSpy clearSpy(&container, &TransferProgressContainer::clearStatusMessages);
        QSignalSpy statusSpy(&container, &TransferProgressContainer::statusMessage);

        QMetaObject::invokeMethod(&container, "onAllOperationsCompleted");

        QCOMPARE(clearSpy.count(), 1);
        QCOMPARE(statusSpy.count(), 1);
    }

    void testOnOperationsCancelled_EmitsClearAndStatus()
    {
        TransferProgressContainer container;
        QSignalSpy clearSpy(&container, &TransferProgressContainer::clearStatusMessages);
        QSignalSpy statusSpy(&container, &TransferProgressContainer::statusMessage);

        QMetaObject::invokeMethod(&container, "onOperationsCancelled");

        QCOMPARE(clearSpy.count(), 1);
        QCOMPARE(statusSpy.count(), 1);
    }
};

QTEST_MAIN(TestTransferProgressContainer)
#include "test_transferprogresscontainer.moc"
