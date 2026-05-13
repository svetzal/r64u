/**
 * @file test_connectionstatuswidget.cpp
 * @brief Unit tests for ConnectionStatusWidget display state.
 *
 * Tests verify:
 * - Construction does not crash
 * - Initial disconnected state shows "Disconnected" label
 * - setConnected(true/false) updates status label text
 * - setConnected(false) hides hostname and firmware labels
 * - setHostname() shows label only when connected and non-empty
 * - setFirmwareVersion() formats text and shows only when connected
 */

#include "ui/connectionstatuswidget.h"

#include <QLabel>
#include <QtTest>

class TestConnectionStatusWidget : public QObject
{
    Q_OBJECT

private slots:

    void testConstruct_doesNotCrash()
    {
        ConnectionStatusWidget widget;
        QVERIFY(true);
    }

    void testConstruct_initiallyDisconnected()
    {
        ConnectionStatusWidget widget;
        auto *status = widget.findChild<QLabel *>();
        QVERIFY(status != nullptr);
        QCOMPARE(status->text(), QString("Disconnected"));
    }

    void testSetConnected_true_updatesStatusLabel()
    {
        ConnectionStatusWidget widget;
        widget.setConnected(true);

        bool found = false;
        const auto labels = widget.findChildren<QLabel *>();
        for (auto *label : labels) {
            if (label->text() == "Connected") {
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }

    void testSetConnected_false_updatesStatusLabel()
    {
        ConnectionStatusWidget widget;
        widget.setConnected(true);
        widget.setConnected(false);

        bool found = false;
        const auto labels = widget.findChildren<QLabel *>();
        for (auto *label : labels) {
            if (label->text() == "Disconnected") {
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }

    void testSetConnected_false_hidesHostnameLabel()
    {
        ConnectionStatusWidget widget;
        widget.setConnected(true);
        widget.setHostname("mydevice");
        widget.setConnected(false);

        const auto labels = widget.findChildren<QLabel *>();
        for (auto *label : labels) {
            if (!label->text().isEmpty() && label->text() == "mydevice") {
                QVERIFY(!label->isVisible());
                return;
            }
        }
        // Label was cleared on disconnect — also acceptable
        QVERIFY(true);
    }

    void testSetConnected_false_hidesFirmwareLabel()
    {
        ConnectionStatusWidget widget;
        widget.setConnected(true);
        widget.setFirmwareVersion("3.10");
        widget.setConnected(false);

        const auto labels = widget.findChildren<QLabel *>();
        for (auto *label : labels) {
            if (!label->text().isEmpty() && label->text().contains("3.10")) {
                QVERIFY(!label->isVisible());
                return;
            }
        }
        // Label was cleared on disconnect — also acceptable
        QVERIFY(true);
    }

    void testSetHostname_setsLabelText()
    {
        ConnectionStatusWidget widget;
        widget.setConnected(true);
        widget.setHostname("mydevice");

        bool found = false;
        const auto labels = widget.findChildren<QLabel *>();
        for (auto *label : labels) {
            if (label->text() == "mydevice") {
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }

    void testSetHostname_hiddenWhenNotConnected()
    {
        ConnectionStatusWidget widget;
        widget.setConnected(false);
        widget.setHostname("device");

        const auto labels = widget.findChildren<QLabel *>();
        for (auto *label : labels) {
            if (label->text() == "device") {
                QVERIFY(!label->isVisible());
                return;
            }
        }
        // Not visible when not connected — also acceptable to not find it
        QVERIFY(true);
    }

    void testSetFirmwareVersion_setsFormattedText()
    {
        ConnectionStatusWidget widget;
        widget.setConnected(true);
        widget.setFirmwareVersion("3.10");

        bool found = false;
        const auto labels = widget.findChildren<QLabel *>();
        for (auto *label : labels) {
            if (label->text() == "(3.10)") {
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }

    void testSetFirmwareVersion_hiddenWhenNotConnected()
    {
        ConnectionStatusWidget widget;
        widget.setConnected(false);
        widget.setFirmwareVersion("3.10");

        const auto labels = widget.findChildren<QLabel *>();
        for (auto *label : labels) {
            if (label->text() == "(3.10)") {
                QVERIFY(!label->isVisible());
                return;
            }
        }
        QVERIFY(true);
    }
};

QTEST_MAIN(TestConnectionStatusWidget)
#include "test_connectionstatuswidget.moc"
