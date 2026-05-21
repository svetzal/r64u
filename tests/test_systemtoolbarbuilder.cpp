/**
 * @file test_systemtoolbarbuilder.cpp
 * @brief Unit tests for SystemToolBarBuilder result object population.
 *
 * Tests verify:
 * - build() returns non-null connectAction, resetAction
 * - build() returns non-null connectionStatus widget
 * - build() returns non-null connectionUiController
 * - connectAction text contains "Connect"
 */

#include "mocks/mockftpclient.h"
#include "mocks/mockrestclient.h"
#include "services/deviceconnectionmanager.h"
#include "services/statusmessageservice.h"
#include "services/systemcommandcontroller.h"
#include "ui/systemtoolbarbuilder.h"

#include <QAction>
#include <QMainWindow>
#include <QToolBar>
#include <QtTest>

class TestSystemToolBarBuilder : public QObject
{
    Q_OBJECT

private:
    SystemToolBarResult buildResult()
    {
        auto *window = new QMainWindow();
        auto *toolBar = new QToolBar(window);
        window->addToolBar(toolBar);

        auto *mockRest = new MockRestClient(window);
        auto *mockFtp = new MockFtpClient(window);
        auto *connection = new DeviceConnectionManager(mockRest, mockFtp, window);

        auto *status = new StatusMessageService(window);
        auto *sysCtrl = new SystemCommandController(mockRest, status, window);

        auto *refreshAction = new QAction("Refresh", window);

        return SystemToolBarBuilder::build(window, toolBar, connection, sysCtrl, refreshAction);
    }

private slots:

    void testBuild_returnsConnectAction()
    {
        auto result = buildResult();
        QVERIFY(result.connectAction != nullptr);
    }

    void testBuild_returnsResetAction()
    {
        auto result = buildResult();
        QVERIFY(result.resetAction != nullptr);
    }

    void testBuild_returnsConnectionStatusWidget()
    {
        auto result = buildResult();
        QVERIFY(result.connectionStatus != nullptr);
    }

    void testBuild_returnsConnectionUiController()
    {
        auto result = buildResult();
        QVERIFY(result.connectionUiController != nullptr);
    }

    void testBuild_connectActionText_containsConnect()
    {
        auto result = buildResult();
        QVERIFY(result.connectAction->text().contains("Connect", Qt::CaseInsensitive));
    }
};

QTEST_MAIN(TestSystemToolBarBuilder)
#include "test_systemtoolbarbuilder.moc"
