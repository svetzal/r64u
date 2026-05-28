#include "mainwindow.h"

#include <QMenuBar>
#include <QSettings>
#include <QTabWidget>
#include <QToolBar>
#include <QtTest>

class TestMainWindow : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QCoreApplication::setOrganizationName("r64utest");
        QCoreApplication::setApplicationName("test_mainwindow");
        QSettings settings;
        settings.clear();
        settings.sync();
    }

    void cleanup()
    {
        QSettings settings;
        settings.clear();
        settings.sync();
    }

    void testConstruct_doesNotCrash()
    {
        MainWindow window;
        QVERIFY(true);
    }

    void testTabWidget_hasFourTabs()
    {
        MainWindow window;
        QTabWidget *tabs = window.findChild<QTabWidget *>();
        QVERIFY(tabs != nullptr);
        QCOMPARE(tabs->count(), 4);
    }

    void testMenuBar_exists()
    {
        MainWindow window;
        QVERIFY(window.menuBar() != nullptr);
        QVERIFY(!window.menuBar()->actions().isEmpty());
    }

    void testSystemToolBar_exists()
    {
        MainWindow window;
        const auto toolbars = window.findChildren<QToolBar *>();
        QVERIFY(!toolbars.isEmpty());
    }

    void testWindowTitle_containsModeName()
    {
        MainWindow window;
        QVERIFY(window.windowTitle().contains("Explore/Run"));
    }

    void testTabWidget_firstTabLabelIsExploreRun()
    {
        MainWindow window;
        QTabWidget *tabs = window.findChild<QTabWidget *>();
        QVERIFY(tabs != nullptr);
        QCOMPARE(tabs->tabText(0), QString("Explore/Run"));
        QCOMPARE(tabs->tabText(1), QString("Transfer"));
        QCOMPARE(tabs->tabText(2), QString("View"));
        QCOMPARE(tabs->tabText(3), QString("Config"));
    }
};

QTEST_MAIN(TestMainWindow)
#include "test_mainwindow.moc"
