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
#include "ui/connectionstatuswidget.h"
#include "ui/systemcommandcontroller.h"
#include "ui/systemtoolbarbuilder.h"

#include <QAction>
#include <QLayout>
#include <QMainWindow>
#include <QToolBar>
#include <QToolButton>
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

    /// The tool button the toolbar shows for @p action, wherever it sits.
    static QToolButton *buttonFor(QToolBar *toolBar, QAction *action)
    {
        for (QToolButton *button : toolBar->findChildren<QToolButton *>()) {
            if (button->defaultAction() == action) {
                return button;
            }
        }
        return nullptr;
    }

    void testBuild_frontPanelOrder_connectThenMachineThenDeviceThenStatus()
    {
        auto result = buildResult();
        QToolBar *toolBar = qobject_cast<QToolBar *>(result.connectionStatus->parentWidget());
        QVERIFY(toolBar != nullptr);
        const QList<QAction *> actions = toolBar->actions();

        auto *machineGroup = toolBar->findChild<QWidget *>(QStringLiteral("MachineControls"));
        QVERIFY(machineGroup != nullptr);
        QVERIFY(machineGroup->layout() != nullptr);
        QCOMPARE(machineGroup->layout()->spacing(), 0);
        QCOMPARE(buttonFor(toolBar, result.resetAction)->parentWidget(), machineGroup);
        QCOMPARE(buttonFor(toolBar, result.pauseAction)->parentWidget(), machineGroup);
        QCOMPARE(buttonFor(toolBar, result.resumeAction)->parentWidget(), machineGroup);
        QCOMPARE(buttonFor(toolBar, result.menuAction)->parentWidget(), machineGroup);

        // Connect | [Reset Pause Resume Menu] | Reboot Power Off | spacer status
        QCOMPARE(actions[0], result.connectAction);
        QVERIFY(actions[1]->isSeparator());
        QCOMPARE(toolBar->widgetForAction(actions[2]), machineGroup);
        QVERIFY(actions[3]->isSeparator());
        QCOMPARE(actions[4], result.rebootAction);
        QCOMPARE(actions[5], result.powerOffAction);
        QCOMPARE(toolBar->widgetForAction(actions.last()), result.connectionStatus);
    }

    void testBuild_machineButtonsFollowTheToolBarsStyle()
    {
        auto result = buildResult();
        QToolBar *toolBar = qobject_cast<QToolBar *>(result.connectionStatus->parentWidget());
        QToolButton *reset = buttonFor(toolBar, result.resetAction);
        QVERIFY(reset != nullptr);

        toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
        toolBar->setIconSize(QSize(24, 24));

        QCOMPARE(reset->toolButtonStyle(), Qt::ToolButtonIconOnly);
        QCOMPARE(reset->iconSize(), QSize(24, 24));
    }

    void testBuild_rebootAndPowerOffAreGuarded()
    {
        auto result = buildResult();
        QToolBar *toolBar = qobject_cast<QToolBar *>(result.connectionStatus->parentWidget());

        QCOMPARE(toolBar->widgetForAction(result.rebootAction)->objectName(),
                 QStringLiteral("DangerAction"));
        QCOMPARE(toolBar->widgetForAction(result.powerOffAction)->objectName(),
                 QStringLiteral("DangerAction"));
        QVERIFY(toolBar->widgetForAction(result.connectAction)->objectName().isEmpty());
    }

    void testBuild_preferencesIsOnTheToolBarOnlyWhereTheAppMenuLacksIt()
    {
        auto result = buildResult();
        QToolBar *toolBar = qobject_cast<QToolBar *>(result.connectionStatus->parentWidget());
        QVERIFY(result.prefsAction != nullptr);  // MainWindow wires it on every platform

#ifdef Q_OS_MACOS
        QVERIFY(!toolBar->actions().contains(result.prefsAction));
#else
        QVERIFY(toolBar->actions().contains(result.prefsAction));
#endif
    }

    void testBuild_everyActionHasAnIcon()
    {
        auto result = buildResult();
        for (QAction *action :
             {result.connectAction, result.resetAction, result.rebootAction, result.pauseAction,
              result.resumeAction, result.menuAction, result.powerOffAction, result.prefsAction}) {
            QVERIFY2(!action->icon().isNull(), qPrintable(action->text()));
        }
    }
};

QTEST_MAIN(TestSystemToolBarBuilder)
#include "test_systemtoolbarbuilder.moc"
