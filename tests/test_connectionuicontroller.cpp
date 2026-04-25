#include "mocks/mockconnectionstatusview.h"
#include "mocks/mockftpclient.h"
#include "mocks/mockrestclient.h"
#include "services/deviceconnection.h"
#include "ui/connectionuicontroller.h"

#include <QAction>
#include <QSignalSpy>
#include <QtTest>

class TestConnectionUIController : public QObject
{
    Q_OBJECT

private:
    MockRestClient *mockRest_ = nullptr;
    MockFtpClient *mockFtp_ = nullptr;
    DeviceConnection *dc_ = nullptr;
    MockConnectionStatusView mockView_;
    ConnectionUIController *ctrl_ = nullptr;

private slots:
    void init()
    {
        mockRest_ = new MockRestClient(this);
        mockFtp_ = new MockFtpClient(this);
        dc_ = new DeviceConnection(mockRest_, mockFtp_, this);
        ctrl_ = new ConnectionUIController(dc_, &mockView_);
    }

    void cleanup()
    {
        delete ctrl_;
        ctrl_ = nullptr;
        mockView_.reset();
        // dc_, mockRest_, mockFtp_ are parented to this — Qt cleans them up
    }

    // =========================================================================
    // updateAll — null view guard
    // =========================================================================

    void testUpdateAll_NullStatusView_NoOp()
    {
        // A controller constructed with nullptr as the view must not crash on updateAll()
        ConnectionUIController ctrl(dc_, nullptr);
        ctrl.updateAll();
        // If we reach here without crashing, the guard works
        QVERIFY(true);
    }

    // =========================================================================
    // updateAll — disconnected state
    // =========================================================================

    void testUpdateAll_Disconnected_SetsNotConnected()
    {
        // dc_ starts disconnected, so setConnected(false) should be called once
        ctrl_->updateAll();

        QCOMPARE(mockView_.connectedHistory.size(), 1);
        QVERIFY(!mockView_.connectedHistory.first());
    }

    // =========================================================================
    // updateAll — connect action text
    // =========================================================================

    void testUpdateAll_ConnectActionText_WhenDisconnected()
    {
        QAction connectAction;
        ctrl_->setManagedActions({}, &connectAction, nullptr);

        ctrl_->updateAll();

        QCOMPARE(connectAction.text(), QString("Connect"));
    }

    // =========================================================================
    // updateAll — system actions disabled when not REST-connected
    // =========================================================================

    void testUpdateAll_SystemActionsDisabled_WhenNotRestConnected()
    {
        QAction systemAction;
        systemAction.setEnabled(true);  // start enabled so we can verify the change
        ctrl_->setManagedActions({&systemAction}, nullptr, nullptr);

        ctrl_->updateAll();

        QVERIFY(!systemAction.isEnabled());
    }

    // =========================================================================
    // updateAll — refresh action disabled when disconnected
    // =========================================================================

    void testUpdateAll_RefreshActionDisabled_WhenDisconnected()
    {
        QAction refreshAction;
        refreshAction.setEnabled(true);
        ctrl_->setManagedActions({}, nullptr, &refreshAction);

        ctrl_->updateAll();

        QVERIFY(!refreshAction.isEnabled());
    }

    // =========================================================================
    // slot — onConnectionStateChanged emits windowTitleUpdateNeeded
    // =========================================================================

    void testOnConnectionStateChanged_EmitsWindowTitleUpdateNeeded()
    {
        QSignalSpy spy(ctrl_, &ConnectionUIController::windowTitleUpdateNeeded);

        QMetaObject::invokeMethod(ctrl_, "onConnectionStateChanged");

        QCOMPARE(spy.count(), 1);
    }

    // =========================================================================
    // slot — onDeviceInfoUpdated emits windowTitleUpdateNeeded
    // =========================================================================

    void testOnDeviceInfoUpdated_EmitsWindowTitleUpdateNeeded()
    {
        QSignalSpy spy(ctrl_, &ConnectionUIController::windowTitleUpdateNeeded);

        QMetaObject::invokeMethod(ctrl_, "onDeviceInfoUpdated");

        QCOMPARE(spy.count(), 1);
    }

    // =========================================================================
    // slot — onDriveInfoUpdated emits windowTitleUpdateNeeded
    // =========================================================================

    void testOnDriveInfoUpdated_EmitsWindowTitleUpdateNeeded()
    {
        QSignalSpy spy(ctrl_, &ConnectionUIController::windowTitleUpdateNeeded);

        QMetaObject::invokeMethod(ctrl_, "onDriveInfoUpdated");

        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TestConnectionUIController)
#include "test_connectionuicontroller.moc"
