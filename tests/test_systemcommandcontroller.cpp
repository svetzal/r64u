#include "mocks/mockrestclient.h"
#include "services/statusmessageservice.h"
#include "services/systemcommandcontroller.h"

#include <QtTest>

class TestSystemCommandController : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        restClient_ = new MockRestClient();
        statusService_ = new StatusMessageService();
        controller_ = new SystemCommandController(restClient_, statusService_);
    }

    void cleanup()
    {
        delete controller_;
        delete statusService_;
        delete restClient_;
    }

    void testReset_callsResetMachine()
    {
        controller_->onReset();
        QCOMPARE(restClient_->mockResetMachineCallCount(), 1);
    }

    void testReboot_callsRebootMachine()
    {
        controller_->onReboot();
        QCOMPARE(restClient_->mockRebootMachineCallCount(), 1);
    }

    void testPause_callsPauseMachine()
    {
        controller_->onPause();
        QCOMPARE(restClient_->mockPauseMachineCallCount(), 1);
    }

    void testResume_callsResumeMachine()
    {
        controller_->onResume();
        QCOMPARE(restClient_->mockResumeMachineCallCount(), 1);
    }

    void testMenuButton_callsPressMenuButton()
    {
        controller_->onMenuButton();
        QCOMPARE(restClient_->mockPressMenuButtonCallCount(), 1);
    }

    void testPowerOff_callsPowerOffMachine()
    {
        controller_->powerOff();
        QCOMPARE(restClient_->mockPowerOffMachineCallCount(), 1);
    }

    void testEjectDriveA_callsUnmountImageA()
    {
        controller_->onEjectDriveA();
        QCOMPARE(restClient_->mockUnmountImageCallCount(), 1);
        QCOMPARE(restClient_->mockLastUnmountDrive(), QStringLiteral("a"));
    }

    void testEjectDriveB_callsUnmountImageB()
    {
        controller_->onEjectDriveB();
        QCOMPARE(restClient_->mockUnmountImageCallCount(), 1);
        QCOMPARE(restClient_->mockLastUnmountDrive(), QStringLiteral("b"));
    }

    void testReset_emitsStatusMessage()
    {
        QString capturedMessage;
        connect(statusService_, &StatusMessageService::displayMessage, this,
                [&capturedMessage](const QString &msg, int) { capturedMessage = msg; });

        controller_->onReset();

        QVERIFY(!capturedMessage.isEmpty());
    }

    void testEjectDriveA_emitsStatusMessage()
    {
        QString capturedMessage;
        connect(statusService_, &StatusMessageService::displayMessage, this,
                [&capturedMessage](const QString &msg, int) { capturedMessage = msg; });

        controller_->onEjectDriveA();

        QVERIFY(!capturedMessage.isEmpty());
    }

private:
    MockRestClient *restClient_ = nullptr;
    StatusMessageService *statusService_ = nullptr;
    SystemCommandController *controller_ = nullptr;
};

QTEST_MAIN(TestSystemCommandController)
#include "test_systemcommandcontroller.moc"
