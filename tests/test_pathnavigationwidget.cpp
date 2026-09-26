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
 * - setStyleBlue()/setStyleGreen() colour the path badge from the theme tokens
 */

#include "core/themecore.h"
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

    void testSetStyleGreen_usesAccentGreenOnInset()
    {
        PathNavigationWidget widget("Local:");
        widget.setStyleGreen();

        auto *label = widget.findChild<QLabel *>();
        QVERIFY(label != nullptr);
        const auto tokens = themecore::currentTokens();
        QVERIFY(label->styleSheet().contains(tokens.accentGreen.name()));
        QVERIFY(label->styleSheet().contains(tokens.bgInset.name()));
    }

    void testSetStyleBlue_usesAccentBlueOnInset()
    {
        PathNavigationWidget widget("Remote:");
        widget.setStyleGreen();
        widget.setStyleBlue();

        auto *label = widget.findChild<QLabel *>();
        QVERIFY(label != nullptr);
        const auto tokens = themecore::currentTokens();
        QVERIFY(label->styleSheet().contains(tokens.accentBlue.name()));
        QVERIFY(!label->styleSheet().contains(tokens.accentGreen.name()));
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
