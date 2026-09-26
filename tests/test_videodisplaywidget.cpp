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
 * - placeholder lines are stored and the placeholder screen renders
 * - keyboardFocusChanged follows focus in/out
 * - Escape is left for the window; other keys are consumed
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
        QCOMPARE(widget.videoFormat(), VideoStreamReceiverService::VideoFormat::Unknown);
    }

    // =========================================================================
    // Placeholder C64 screen
    // =========================================================================

    void testPlaceholderLines_defaultEmpty()
    {
        VideoDisplayWidget widget;
        QVERIFY(widget.placeholderLines().isEmpty());
    }

    void testSetPlaceholderLines_storesLines()
    {
        VideoDisplayWidget widget;
        const QStringList lines{"    **** R64U VIDEO ****", "", "READY."};
        widget.setPlaceholderLines(lines);
        QCOMPARE(widget.placeholderLines(), lines);
    }

    void testGrab_withoutFrame_rendersPlaceholder()
    {
        VideoDisplayWidget widget;
        widget.resize(768, 544);
        widget.setPlaceholderLines({"NO DEVICE CONNECTED.", "", "READY."});
        const QPixmap pixmap = widget.grab();
        QVERIFY(!pixmap.isNull());
        QCOMPARE(pixmap.size(), QSize(768, 544));
    }

    // =========================================================================
    // Keyboard focus
    // =========================================================================

    void testFocusInAndOut_emitKeyboardFocusChanged()
    {
        VideoDisplayWidget widget;
        QSignalSpy spy(&widget, &VideoDisplayWidget::keyboardFocusChanged);

        QFocusEvent focusIn(QEvent::FocusIn);
        QCoreApplication::sendEvent(&widget, &focusIn);
        QFocusEvent focusOut(QEvent::FocusOut);
        QCoreApplication::sendEvent(&widget, &focusOut);

        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
        QCOMPARE(spy.at(1).at(0).toBool(), false);
    }

    void testKeyPress_escapeIsNotAcceptedAndNotForwarded()
    {
        VideoDisplayWidget widget;
        QSignalSpy spy(&widget, &VideoDisplayWidget::keyPressed);

        QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QCoreApplication::sendEvent(&widget, &escape);

        QVERIFY(!escape.isAccepted());
        QCOMPARE(spy.count(), 0);
    }

    void testKeyPress_otherKeysAreAcceptedAndForwarded()
    {
        VideoDisplayWidget widget;
        QSignalSpy spy(&widget, &VideoDisplayWidget::keyPressed);

        QKeyEvent key(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));
        QCoreApplication::sendEvent(&widget, &key);

        QVERIFY(key.isAccepted());
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TestVideoDisplayWidget)
#include "test_videodisplaywidget.moc"
