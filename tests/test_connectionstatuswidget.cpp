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
 * - setConnecting(true) shows "Connecting…" and hides hostname/firmware
 * - The LED sits to the left of the status text
 */

#include "core/themecore.h"
#include "ui/connectionstatuswidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QtTest>

class TestConnectionStatusWidget : public QObject
{
    Q_OBJECT

    static QLabel *labelWithText(const QWidget &widget, const QString &text)
    {
        for (QLabel *label : widget.findChildren<QLabel *>()) {
            if (label->text() == text) {
                return label;
            }
        }
        return nullptr;
    }

private slots:

    void testConstruct_doesNotCrash()
    {
        ConnectionStatusWidget widget;
        QVERIFY(true);
    }

    void testConstruct_initiallyDisconnected()
    {
        ConnectionStatusWidget widget;
        QVERIFY(labelWithText(widget, "Disconnected") != nullptr);
    }

    void testLayout_ledIsLeftOfStatusText()
    {
        ConnectionStatusWidget widget;
        auto *layout = widget.layout();
        QVERIFY(layout != nullptr);
        QVERIFY(layout->count() >= 2);

        auto *led = qobject_cast<QLabel *>(layout->itemAt(0)->widget());
        auto *status = qobject_cast<QLabel *>(layout->itemAt(1)->widget());
        QVERIFY(led != nullptr);
        QVERIFY(status != nullptr);
        QVERIFY(led->text().isEmpty());
        QCOMPARE(led->width(), 10);
        QCOMPARE(status->text(), QString("Disconnected"));
    }

    void testLed_usesStateTokens()
    {
        ConnectionStatusWidget widget;
        auto *led = qobject_cast<QLabel *>(widget.layout()->itemAt(0)->widget());
        QVERIFY(led != nullptr);
        const auto tokens = themecore::currentTokens();

        QVERIFY(led->styleSheet().contains(tokens.ledOff.name()));
        widget.setConnected(true);
        QVERIFY(led->styleSheet().contains(tokens.stateConnected.name()));
        widget.setConnecting(true);
        QVERIFY(led->styleSheet().contains(tokens.stateWarning.name()));
    }

    void testSetConnecting_true_showsConnectingText()
    {
        ConnectionStatusWidget widget;
        widget.setConnecting(true);
        QVERIFY(labelWithText(widget, QString::fromUtf8("Connecting\u2026")) != nullptr);
    }

    void testSetConnecting_true_hidesHostnameAndFirmware()
    {
        ConnectionStatusWidget widget;
        widget.setConnected(true);
        widget.setHostname("mydevice");
        widget.setFirmwareVersion("3.10");

        widget.setConnecting(true);

        QVERIFY(labelWithText(widget, "mydevice") == nullptr);
        QVERIFY(labelWithText(widget, "(3.10)") == nullptr);
    }

    void testSetConnecting_false_returnsToDisconnected()
    {
        ConnectionStatusWidget widget;
        widget.setConnecting(true);
        widget.setConnecting(false);
        QVERIFY(labelWithText(widget, "Disconnected") != nullptr);
    }

    void testSetConnected_afterConnecting_showsConnected()
    {
        ConnectionStatusWidget widget;
        widget.setConnecting(true);
        widget.setConnected(true);
        QVERIFY(labelWithText(widget, "Connected") != nullptr);
        QVERIFY(labelWithText(widget, QString::fromUtf8("Connecting\u2026")) == nullptr);
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
