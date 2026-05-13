/**
 * @file test_videodisplaywidget.cpp
 * @brief Unit tests for VideoDisplayWidget property accessors and signal emission.
 *
 * Tests verify:
 * - Construction does not crash
 * - Default scaling mode is Integer
 * - setScalingMode() stores and returns mode, emits scalingModeChanged
 * - setFramePacingEnabled() stores and returns pacing state
 * - clear() resets hasFrame state (currentFrame() returns null)
 * - sizeHint() returns PAL dimensions by default
 * - videoFormat() defaults to Unknown after construction
 */

#include "ui/videodisplaywidget.h"

#include <QSignalSpy>
#include <QtTest>

class TestVideoDisplayWidget : public QObject
{
    Q_OBJECT

private slots:

    void testConstruct_doesNotCrash()
    {
        VideoDisplayWidget widget;
        QVERIFY(true);
    }

    void testConstruct_defaultScalingMode_isInteger()
    {
        VideoDisplayWidget widget;
        QCOMPARE(widget.scalingMode(), VideoDisplayWidget::ScalingMode::Integer);
    }

    void testSetScalingMode_Sharp_storesMode()
    {
        VideoDisplayWidget widget;
        widget.setScalingMode(VideoDisplayWidget::ScalingMode::Sharp);
        QCOMPARE(widget.scalingMode(), VideoDisplayWidget::ScalingMode::Sharp);
    }

    void testSetScalingMode_Smooth_storesMode()
    {
        VideoDisplayWidget widget;
        widget.setScalingMode(VideoDisplayWidget::ScalingMode::Smooth);
        QCOMPARE(widget.scalingMode(), VideoDisplayWidget::ScalingMode::Smooth);
    }

    void testSetScalingMode_Integer_storesMode()
    {
        VideoDisplayWidget widget;
        widget.setScalingMode(VideoDisplayWidget::ScalingMode::Sharp);
        widget.setScalingMode(VideoDisplayWidget::ScalingMode::Integer);
        QCOMPARE(widget.scalingMode(), VideoDisplayWidget::ScalingMode::Integer);
    }

    void testSetScalingMode_emitsScalingModeChanged()
    {
        VideoDisplayWidget widget;
        QSignalSpy spy(&widget, &VideoDisplayWidget::scalingModeChanged);

        widget.setScalingMode(VideoDisplayWidget::ScalingMode::Sharp);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<VideoDisplayWidget::ScalingMode>(),
                 VideoDisplayWidget::ScalingMode::Sharp);
    }

    void testSetFramePacingEnabled_true()
    {
        VideoDisplayWidget widget;
        widget.setFramePacingEnabled(true);
        QVERIFY(widget.isFramePacingEnabled());
    }

    void testSetFramePacingEnabled_false()
    {
        VideoDisplayWidget widget;
        widget.setFramePacingEnabled(false);
        QVERIFY(!widget.isFramePacingEnabled());
    }

    void testClear_doesNotCrash()
    {
        VideoDisplayWidget widget;
        widget.clear();
        QVERIFY(true);
    }

    void testClear_currentFrame_isNull()
    {
        VideoDisplayWidget widget;
        widget.clear();
        QVERIFY(widget.currentFrame().isNull());
    }

    void testSizeHint_PAL_returnsExpectedHeight()
    {
        VideoDisplayWidget widget;
        // Default format is Unknown, which falls back to PAL height
        QCOMPARE(widget.sizeHint().height(), VideoDisplayWidget::PalHeight);
    }

    void testVideoFormat_defaultIsUnknown()
    {
        VideoDisplayWidget widget;
        QCOMPARE(widget.videoFormat(), VideoStreamReceiver::VideoFormat::Unknown);
    }
};

QTEST_MAIN(TestVideoDisplayWidget)
#include "test_videodisplaywidget.moc"
