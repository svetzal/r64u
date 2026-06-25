/**
 * @file test_menubarbuilder.cpp
 * @brief Unit tests for menubar::Builder menu construction.
 *
 * Tests verify:
 * - build() returns a non-null refresh QAction
 * - Menu bar contains File, View, Machine, and Help menus
 */

#include "mocks/mockrestclient.h"
#include "services/statusmessageservice.h"
#include "ui/menubarbuilder.h"
#include "ui/systemcommandcontroller.h"

#include <QMainWindow>
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
};

QTEST_MAIN(TestMenuBarBuilder)
#include "test_menubarbuilder.moc"
