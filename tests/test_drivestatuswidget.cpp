/**
 * @file test_drivestatuswidget.cpp
 * @brief Unit tests for DriveStatusWidget mount state and signal emission.
 *
 * Tests verify:
 * - Construction does not crash
 * - Initial mounted state is false
 * - setMounted() updates isMounted() and enables/disables eject button
 * - setImageName() shows "[empty]" for empty names and the name otherwise
 * - ejectClicked() signal emitted when eject button clicked (while mounted)
 */

#include "ui/drivestatuswidget.h"

#include <QLabel>
#include <QSignalSpy>
#include <QToolButton>
#include <QtTest>

class TestDriveStatusWidget : public QObject
{
    Q_OBJECT

private slots:

    void testConstruct_doesNotCrash()
    {
        DriveStatusWidget widget("Drive A");
        QVERIFY(true);
    }

    void testConstruct_isMounted_returnsFalse()
    {
        DriveStatusWidget widget("Drive A");
        QVERIFY(!widget.isMounted());
    }

    void testSetMounted_true_isMountedReturnsTrue()
    {
        DriveStatusWidget widget("Drive A");
        widget.setMounted(true);
        QVERIFY(widget.isMounted());
    }

    void testSetMounted_false_isMountedReturnsFalse()
    {
        DriveStatusWidget widget("Drive A");
        widget.setMounted(true);
        widget.setMounted(false);
        QVERIFY(!widget.isMounted());
    }

    void testSetMounted_true_enablesEjectButton()
    {
        DriveStatusWidget widget("Drive A");
        widget.setMounted(true);

        auto *button = widget.findChild<QToolButton *>();
        QVERIFY(button != nullptr);
        QVERIFY(button->isEnabled());
    }

    void testSetMounted_false_disablesEjectButton()
    {
        DriveStatusWidget widget("Drive A");
        widget.setMounted(true);
        widget.setMounted(false);

        auto *button = widget.findChild<QToolButton *>();
        QVERIFY(button != nullptr);
        QVERIFY(!button->isEnabled());
    }

    void testSetImageName_empty_showsEmptyLabel()
    {
        DriveStatusWidget widget("Drive A");
        widget.setImageName("");

        bool found = false;
        const auto labels = widget.findChildren<QLabel *>();
        for (auto *label : labels) {
            if (label->text() == "[empty]") {
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }

    void testSetImageName_nonEmpty_showsImageName()
    {
        DriveStatusWidget widget("Drive A");
        widget.setImageName("games.d64");

        bool found = false;
        const auto labels = widget.findChildren<QLabel *>();
        for (auto *label : labels) {
            if (label->text() == "games.d64") {
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }

    void testEjectClicked_emitsSignal()
    {
        DriveStatusWidget widget("Drive A");
        widget.setMounted(true);

        QSignalSpy spy(&widget, &DriveStatusWidget::ejectClicked);

        auto *button = widget.findChild<QToolButton *>();
        QVERIFY(button != nullptr);
        button->click();

        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TestDriveStatusWidget)
#include "test_drivestatuswidget.moc"
