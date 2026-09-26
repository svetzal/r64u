/**
 * @file test_menubarbuilder.cpp
 * @brief Unit tests for menubar::Builder menu construction.
 *
 * Tests verify:
 * - build() returns a non-null refresh QAction
 * - Menu bar contains File, View, Machine, and Help menus
 * - Mode actions stop driving the tab widget once it is destroyed
 */

#include "mocks/mockrestclient.h"
#include "services/statusmessageservice.h"
#include "ui/menubarbuilder.h"
#include "ui/systemcommandcontroller.h"

#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QTabWidget>
#include <QtTest>

class TestMenuBarBuilder : public QObject
{
    Q_OBJECT

private slots:

    void testBuild_returnsNonNullRefreshAction()
    {
        QMainWindow window;
        QTabWidget tabs;
        tabs.addTab(new QWidget, "Explore");
        tabs.addTab(new QWidget, "Transfer");
        tabs.addTab(new QWidget, "View");
        tabs.addTab(new QWidget, "Config");

        MockRestClient rest;
        StatusMessageService status;
        SystemCommandController sysCtrl(&rest, &status);

        QAction *refresh = menubar::Builder::build(&window, &sysCtrl, &tabs);
        QVERIFY(refresh != nullptr);
    }

    void testBuild_menuBarHasFileMenu()
    {
        QMainWindow window;
        QTabWidget tabs;

        MockRestClient rest;
        StatusMessageService status;
        SystemCommandController sysCtrl(&rest, &status);

        menubar::Builder::build(&window, &sysCtrl, &tabs);

        bool found = false;
        const auto actions = window.menuBar()->actions();
        for (auto *action : actions) {
            if (action->text().contains("File", Qt::CaseInsensitive)) {
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }

    void testBuild_menuBarHasViewMenu()
    {
        QMainWindow window;
        QTabWidget tabs;

        MockRestClient rest;
        StatusMessageService status;
        SystemCommandController sysCtrl(&rest, &status);

        menubar::Builder::build(&window, &sysCtrl, &tabs);

        bool found = false;
        const auto actions = window.menuBar()->actions();
        for (auto *action : actions) {
            if (action->text().contains("View", Qt::CaseInsensitive)) {
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }

    void testBuild_menuBarHasMachineMenu()
    {
        QMainWindow window;
        QTabWidget tabs;

        MockRestClient rest;
        StatusMessageService status;
        SystemCommandController sysCtrl(&rest, &status);

        menubar::Builder::build(&window, &sysCtrl, &tabs);

        bool found = false;
        const auto actions = window.menuBar()->actions();
        for (auto *action : actions) {
            if (action->text().contains("Machine", Qt::CaseInsensitive)) {
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }

    void testBuild_menuBarHasHelpMenu()
    {
        QMainWindow window;
        QTabWidget tabs;

        MockRestClient rest;
        StatusMessageService status;
        SystemCommandController sysCtrl(&rest, &status);

        menubar::Builder::build(&window, &sysCtrl, &tabs);

        bool found = false;
        const auto actions = window.menuBar()->actions();
        for (auto *action : actions) {
            if (action->text().contains("Help", Qt::CaseInsensitive)) {
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }

    void testModeActions_afterTabWidgetDestroyed_dropTheirTabConnections()
    {
        // MainWindow destroys its central widget (holding the tab widget) before
        // the menu bar; the mode actions must not keep pointing at it.
        QMainWindow window;
        auto *tabs = new QTabWidget();
        MockRestClient rest;
        StatusMessageService status;
        SystemCommandController sysCtrl(&rest, &status);
        menubar::Builder::build(&window, &sysCtrl, tabs);
        QList<QAction *> modeActions;
        QList<int> receiversBefore;
        for (QAction *action : window.findChildren<QAction *>()) {
            if (action->text().contains(QStringLiteral("Mode"))) {
                modeActions.append(action);
                receiversBefore.append(triggeredReceiverCount(action));
            }
        }
        QCOMPARE(modeActions.size(), 4);

        delete tabs;

        for (qsizetype i = 0; i < modeActions.size(); ++i) {
            QVERIFY2(triggeredReceiverCount(modeActions[i]) == receiversBefore[i] - 1,
                     qPrintable(modeActions[i]->text()));
        }
    }

private:
    /// QObject::receivers() is protected; reach it through a pointer to member.
    struct ReceiverCount : QObject
    {
        static int of(const QObject *object, const char *signal)
        {
            return (object->*&ReceiverCount::receivers)(signal);
        }
    };

    static int triggeredReceiverCount(const QAction *action)
    {
        return ReceiverCount::of(action, SIGNAL(triggered(bool)));
    }
};

QTEST_MAIN(TestMenuBarBuilder)
#include "test_menubarbuilder.moc"
