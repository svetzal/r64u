/**
 * @file test_streamingdiagnosticswidget.cpp
 * @brief Unit tests for StreamingDiagnosticsWidget display mode switching.
 *
 * Tests verify:
 * - Construction does not crash
 * - Default display mode is Compact
 * - setDisplayMode(Detailed) switches mode
 * - setDisplayMode(Compact) switches back
 * - toggleDisplayMode() flips between modes
 * - expand button is present as a QPushButton child
 */

#include "ui/streamingdiagnosticswidget.h"

#include <QPushButton>
#include <QtTest>

class TestStreamingDiagnosticsWidget : public QObject
{
    Q_OBJECT

private slots:

    void testConstruct_doesNotCrash()
    {
        StreamingDiagnosticsWidget widget;
        QVERIFY(true);
    }

    void testConstruct_defaultMode_isCompact()
    {
        StreamingDiagnosticsWidget widget;
        QCOMPARE(widget.displayMode(), StreamingDiagnosticsWidget::DisplayMode::Compact);
    }

    void testSetDisplayMode_Detailed_switchesMode()
    {
        StreamingDiagnosticsWidget widget;
        widget.setDisplayMode(StreamingDiagnosticsWidget::DisplayMode::Detailed);
        QCOMPARE(widget.displayMode(), StreamingDiagnosticsWidget::DisplayMode::Detailed);
    }

    void testSetDisplayMode_Compact_switchesMode()
    {
        StreamingDiagnosticsWidget widget;
        widget.setDisplayMode(StreamingDiagnosticsWidget::DisplayMode::Detailed);
        widget.setDisplayMode(StreamingDiagnosticsWidget::DisplayMode::Compact);
        QCOMPARE(widget.displayMode(), StreamingDiagnosticsWidget::DisplayMode::Compact);
    }

    void testToggleDisplayMode_fromCompact_givesDetailed()
    {
        StreamingDiagnosticsWidget widget;
        widget.toggleDisplayMode();
        QCOMPARE(widget.displayMode(), StreamingDiagnosticsWidget::DisplayMode::Detailed);
    }

    void testToggleDisplayMode_fromDetailed_givesCompact()
    {
        StreamingDiagnosticsWidget widget;
        widget.setDisplayMode(StreamingDiagnosticsWidget::DisplayMode::Detailed);
        widget.toggleDisplayMode();
        QCOMPARE(widget.displayMode(), StreamingDiagnosticsWidget::DisplayMode::Compact);
    }

    void testToggleButton_exists()
    {
        StreamingDiagnosticsWidget widget;
        auto *button = widget.findChild<QPushButton *>();
        QVERIFY(button != nullptr);
    }

    void testClear_doesNotCrash()
    {
        StreamingDiagnosticsWidget widget;
        widget.clear();
        QVERIFY(true);
    }
};

QTEST_MAIN(TestStreamingDiagnosticsWidget)
#include "test_streamingdiagnosticswidget.moc"
