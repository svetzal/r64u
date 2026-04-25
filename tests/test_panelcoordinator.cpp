#include "mocks/mockftpclient.h"
#include "mocks/mockrestclient.h"
#include "models/remotefilemodel.h"
#include "services/deviceconnection.h"
#include "services/errorhandler.h"
#include "services/statusmessageservice.h"
#include "ui/ipanel.h"
#include "ui/panelcoordinator.h"

#include <QSignalSpy>
#include <QTabWidget>
#include <QTest>
#include <QWidget>

// ---------------------------------------------------------------------------
// Mock panels
// ---------------------------------------------------------------------------

class MockPanel : public QObject, public IPanel
{
    Q_OBJECT
public:
    explicit MockPanel(QObject *parent = nullptr) : QObject(parent) {}
    QObject *asQObject() override { return this; }
    void emitStatusMessage(const QString &msg, int timeout = 0)
    {
        emit statusMessage(msg, timeout);
    }
    void emitClearStatusMessages() { emit clearStatusMessages(); }

signals:
    void statusMessage(const QString &msg, int timeout = 0);
    void clearStatusMessages();
};

class MockConfigPanel : public QObject, public IPanel
{
    Q_OBJECT
public:
    explicit MockConfigPanel(QObject *parent = nullptr) : QObject(parent) {}
    QObject *asQObject() override { return this; }
    void refreshIfEmpty() override { ++refreshCalls; }
    void emitStatusMessage(const QString &msg, int timeout = 0)
    {
        emit statusMessage(msg, timeout);
    }
    int refreshCalls = 0;

signals:
    void statusMessage(const QString &msg, int timeout = 0);
};

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

class TestPanelCoordinator : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void testStatusMessageFromExplorePanel_ForwardedToService();
    void testStatusMessageFromTransferPanel_ForwardedToService();
    void testStatusMessageFromViewPanel_ForwardedToService();
    void testStatusMessageFromConfigPanel_ForwardedToService();
    void testClearStatusMessages_ForwardedToService();
    void testOnModeChanged_Index3_CallsRefreshIfEmpty();
    void testOnModeChanged_OtherIndex_DoesNotCallRefreshIfEmpty();
    void testOnModeChanged_EmitsWindowTitleUpdateNeeded();
    void testOnModeChanged_EmitsActionsUpdateNeeded();
    void testOnOperationSucceeded_NonMount_ShowsSuccessMessage();
    void testOnOperationSucceeded_Mount_DoesNotCrash();

private:
    MockRestClient *mockRest_ = nullptr;
    MockFtpClient *mockFtp_ = nullptr;
    DeviceConnection *dc_ = nullptr;
    RemoteFileModel *remoteFileModel_ = nullptr;
    StatusMessageService *statusService_ = nullptr;
    ErrorHandler *errorHandler_ = nullptr;
    QWidget *errorParentWidget_ = nullptr;
    QTabWidget *tabWidget_ = nullptr;
    MockPanel *explorePanel_ = nullptr;
    MockPanel *transferPanel_ = nullptr;
    MockPanel *viewPanel_ = nullptr;
    MockConfigPanel *configPanel_ = nullptr;
    PanelCoordinator *coord_ = nullptr;
};

void TestPanelCoordinator::init()
{
    mockRest_ = new MockRestClient(this);
    mockFtp_ = new MockFtpClient(this);
    dc_ = new DeviceConnection(mockRest_, mockFtp_, this);
    remoteFileModel_ = new RemoteFileModel(this);
    statusService_ = new StatusMessageService(this);
    errorParentWidget_ = new QWidget();
    errorHandler_ = new ErrorHandler(errorParentWidget_, this);
    tabWidget_ = new QTabWidget();
    explorePanel_ = new MockPanel(this);
    transferPanel_ = new MockPanel(this);
    viewPanel_ = new MockPanel(this);
    configPanel_ = new MockConfigPanel(this);
    coord_ = new PanelCoordinator(explorePanel_, transferPanel_, viewPanel_, configPanel_, dc_,
                                  remoteFileModel_, nullptr /*transferService — DC disconnected*/,
                                  statusService_, errorHandler_, tabWidget_);
}

void TestPanelCoordinator::cleanup()
{
    delete coord_;
    coord_ = nullptr;
    delete tabWidget_;
    tabWidget_ = nullptr;
    delete errorParentWidget_;
    errorParentWidget_ = nullptr;
}

void TestPanelCoordinator::testStatusMessageFromExplorePanel_ForwardedToService()
{
    QSignalSpy spy(statusService_, &StatusMessageService::displayMessage);
    explorePanel_->emitStatusMessage("Explore status", 500);
    QCoreApplication::processEvents();
    QVERIFY(!spy.isEmpty());
    QCOMPARE(spy.first().at(0).toString(), QString("Explore status"));
}

void TestPanelCoordinator::testStatusMessageFromTransferPanel_ForwardedToService()
{
    QSignalSpy spy(statusService_, &StatusMessageService::displayMessage);
    transferPanel_->emitStatusMessage("Transfer status", 0);
    QCoreApplication::processEvents();
    QVERIFY(!spy.isEmpty());
    QCOMPARE(spy.first().at(0).toString(), QString("Transfer status"));
}

void TestPanelCoordinator::testStatusMessageFromViewPanel_ForwardedToService()
{
    QSignalSpy spy(statusService_, &StatusMessageService::displayMessage);
    viewPanel_->emitStatusMessage("View status", 0);
    QCoreApplication::processEvents();
    QVERIFY(!spy.isEmpty());
    QCOMPARE(spy.first().at(0).toString(), QString("View status"));
}

void TestPanelCoordinator::testStatusMessageFromConfigPanel_ForwardedToService()
{
    QSignalSpy spy(statusService_, &StatusMessageService::displayMessage);
    configPanel_->emitStatusMessage("Config status", 0);  // emit via signal directly
    QCoreApplication::processEvents();
    QVERIFY(!spy.isEmpty());
    QCOMPARE(spy.first().at(0).toString(), QString("Config status"));
}

void TestPanelCoordinator::testClearStatusMessages_ForwardedToService()
{
    // Show a message first so there is something to clear
    statusService_->showInfo("some message");
    QCoreApplication::processEvents();
    QVERIFY(statusService_->isDisplaying());

    transferPanel_->emitClearStatusMessages();
    QCoreApplication::processEvents();
    QVERIFY(!statusService_->isDisplaying());
}

void TestPanelCoordinator::testOnModeChanged_Index3_CallsRefreshIfEmpty()
{
    QCOMPARE(configPanel_->refreshCalls, 0);
    QMetaObject::invokeMethod(coord_, "onModeChanged", Q_ARG(int, 3));
    QCOMPARE(configPanel_->refreshCalls, 1);
}

void TestPanelCoordinator::testOnModeChanged_OtherIndex_DoesNotCallRefreshIfEmpty()
{
    QMetaObject::invokeMethod(coord_, "onModeChanged", Q_ARG(int, 0));
    QMetaObject::invokeMethod(coord_, "onModeChanged", Q_ARG(int, 1));
    QMetaObject::invokeMethod(coord_, "onModeChanged", Q_ARG(int, 2));
    QCOMPARE(configPanel_->refreshCalls, 0);
}

void TestPanelCoordinator::testOnModeChanged_EmitsWindowTitleUpdateNeeded()
{
    QSignalSpy spy(coord_, &PanelCoordinator::windowTitleUpdateNeeded);
    QMetaObject::invokeMethod(coord_, "onModeChanged", Q_ARG(int, 0));
    QCOMPARE(spy.count(), 1);
}

void TestPanelCoordinator::testOnModeChanged_EmitsActionsUpdateNeeded()
{
    QSignalSpy spy(coord_, &PanelCoordinator::actionsUpdateNeeded);
    QMetaObject::invokeMethod(coord_, "onModeChanged", Q_ARG(int, 0));
    QCOMPARE(spy.count(), 1);
}

void TestPanelCoordinator::testOnOperationSucceeded_NonMount_ShowsSuccessMessage()
{
    QSignalSpy spy(statusService_, &StatusMessageService::displayMessage);
    QMetaObject::invokeMethod(coord_, "onOperationSucceeded", Q_ARG(QString, QString("play")));
    QCoreApplication::processEvents();
    QVERIFY(!spy.isEmpty());
    const QString msg = spy.first().at(0).toString();
    QVERIFY(msg.contains("play"));
    QVERIFY(msg.contains("succeeded"));
}

void TestPanelCoordinator::testOnOperationSucceeded_Mount_DoesNotCrash()
{
    // DeviceConnection is disconnected — refreshDriveInfo() is a no-op
    // Verify the coordinator does not crash on a "mount" operation
    QMetaObject::invokeMethod(coord_, "onOperationSucceeded", Q_ARG(QString, QString("mount")));
    QCoreApplication::processEvents();
    QVERIFY(true);  // Reached without crash
}

QTEST_MAIN(TestPanelCoordinator)
#include "test_panelcoordinator.moc"
