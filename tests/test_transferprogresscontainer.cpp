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
#include "services/deviceconnection.h"
#include "services/transferservice.h"
#include "ui/transferprogresscontainer.h"

#include <QtTest>

class TestTransferProgressContainer : public QObject
{
    Q_OBJECT

private:
    MockRestClient *mockRest_ = nullptr;
    MockFtpClient *mockFtp_ = nullptr;
    DeviceConnection *connection_ = nullptr;
    TransferQueue *queue_ = nullptr;
    TransferService *service_ = nullptr;

    void makeService()
    {
        mockRest_ = new MockRestClient(this);
        mockFtp_ = new MockFtpClient(this);
        connection_ = new DeviceConnection(mockRest_, mockFtp_, this);
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
};

QTEST_MAIN(TestTransferProgressContainer)
#include "test_transferprogresscontainer.moc"
