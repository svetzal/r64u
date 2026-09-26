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
 * - Long image names are elided with the full name in the tooltip
 * - The mount indicator is a 10px LED coloured from the theme tokens
 */

#include "core/themecore.h"
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

    void testSetImageName_long_isElidedWithFullNameInTooltip()
    {
        const QString longName = "a_very_long_disk_image_name_that_cannot_possibly_fit_in_a_"
                                 "narrow_status_row_0123456789.d64";
        DriveStatusWidget widget("Drive A (8)");
        widget.setFixedWidth(220);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        widget.setImageName(longName);
        QCoreApplication::processEvents();

        QLabel *imageLabel = nullptr;
        for (QLabel *label : widget.findChildren<QLabel *>()) {
            if (label->toolTip() == longName) {
                imageLabel = label;
                break;
            }
        }
        QVERIFY2(imageLabel != nullptr, "the full image name must be available as a tooltip");
        QVERIFY(imageLabel->text() != longName);
        QVERIFY(imageLabel->text().contains(QChar(0x2026)));
        QVERIFY(imageLabel->text().endsWith(".d64"));
    }

    void testSetImageName_empty_usesMutedText()
    {
        DriveStatusWidget widget("Drive A (8)");
        widget.setImageName("");
        bool found = false;
        for (QLabel *label : widget.findChildren<QLabel *>()) {
            if (label->text() == "[empty]") {
                found = label->styleSheet().contains(themecore::currentTokens().textMuted.name());
            }
        }
        QVERIFY(found);
    }

    void testIndicator_isTenPixelLedUsingStateTokens()
    {
        DriveStatusWidget widget("Drive A (8)");
        QLabel *led = nullptr;
        for (QLabel *label : widget.findChildren<QLabel *>()) {
            if (label->text().isEmpty() && label->width() == 10 && label->height() == 10) {
                led = label;
            }
        }
        QVERIFY(led != nullptr);
        const auto tokens = themecore::currentTokens();

        QVERIFY(led->styleSheet().contains(tokens.borderStrong.name()));
        widget.setMounted(true);
        QVERIFY(led->styleSheet().contains(tokens.stateConnected.name()));
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
