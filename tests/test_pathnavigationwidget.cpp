/**
 * @file test_pathnavigationwidget.cpp
 * @brief Unit tests for PathNavigationWidget path tracking and signal emission.
 *
 * Tests verify:
 * - Construction does not crash
 * - Default path is "/"
 * - setPath() updates path() and label text
 * - setUpEnabled() controls the up button enabled state
 * - upClicked() signal emitted when up button clicked
 */

#include "ui/pathnavigationwidget.h"

#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest>

class TestPathNavigationWidget : public QObject
{
    Q_OBJECT

private slots:

    void testConstruct_doesNotCrash()
    {
        PathNavigationWidget widget("Remote:");
        QVERIFY(true);
    }

    void testConstruct_defaultPath_isRoot()
    {
        PathNavigationWidget widget("Remote:");
        QCOMPARE(widget.path(), QString("/"));
    }

    void testSetPath_updatesPath()
    {
        PathNavigationWidget widget("Remote:");
        widget.setPath("/SD");
        QCOMPARE(widget.path(), QString("/SD"));
    }

    void testSetPath_updatesLabelText()
    {
        PathNavigationWidget widget("Remote:");
        widget.setPath("/SD");

        bool found = false;
        const auto labels = widget.findChildren<QLabel *>();
        for (auto *label : labels) {
            if (label->text().contains("/SD")) {
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }

    void testPath_returnsCurrentPath()
    {
        PathNavigationWidget widget("Remote:");
        widget.setPath("/remote/games");
        QCOMPARE(widget.path(), QString("/remote/games"));
    }

    void testSetUpEnabled_false_disablesButton()
    {
        PathNavigationWidget widget("Remote:");
        widget.setUpEnabled(false);

        auto *button = widget.findChild<QPushButton *>();
        QVERIFY(button != nullptr);
        QVERIFY(!button->isEnabled());
    }

    void testSetUpEnabled_true_enablesButton()
    {
        PathNavigationWidget widget("Remote:");
        widget.setUpEnabled(false);
        widget.setUpEnabled(true);

        auto *button = widget.findChild<QPushButton *>();
        QVERIFY(button != nullptr);
        QVERIFY(button->isEnabled());
    }

    void testUpClicked_emitsSignal()
    {
        PathNavigationWidget widget("Remote:");

        QSignalSpy spy(&widget, &PathNavigationWidget::upClicked);

        auto *button = widget.findChild<QPushButton *>();
        QVERIFY(button != nullptr);
        button->click();

        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TestPathNavigationWidget)
#include "test_pathnavigationwidget.moc"
