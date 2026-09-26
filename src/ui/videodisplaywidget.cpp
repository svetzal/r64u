/**
 * @file videodisplaywidget.cpp
 * @brief Implementation of the VIC-II video display widget.
 */

#include "videodisplaywidget.h"

#include "c64screenwidget.h"

#include "core/themecore.h"
#include "services/vic2frameconverter.h"
#include "utils/logging.h"

#include <QFontMetrics>
#include <QKeyEvent>
#include <QPainter>

namespace {

/// Layout of the keyboard badge in the bottom-right corner.
constexpr int BadgeInset = 8;
constexpr int BadgePaddingX = 6;
constexpr int BadgePaddingY = 4;
constexpr int BadgeRadius = 4;
constexpr int BadgeFontPx = 16;
constexpr int FocusFramePx = 2;

}  // namespace

VideoDisplayWidget::VideoDisplayWidget(QWidget *parent)
    : QWidget(parent), displayTimer_(new QTimer(this))
{
    // Set black background
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);

    // Initialize with PAL size (larger, to accommodate both formats)
    displayImage_ = QImage(FrameWidth, PalHeight, QImage::Format_RGB32);
    displayImage_.fill(Qt::black);

    // Enable smooth scaling
    setAttribute(Qt::WA_OpaquePaintEvent);

    // Enable keyboard focus
    setFocusPolicy(Qt::StrongFocus);

    // Set up display timer for frame pacing
    displayTimer_->setTimerType(Qt::PreciseTimer);
    connect(displayTimer_, &QTimer::timeout, this, &VideoDisplayWidget::onDisplayTimer);
}

VideoDisplayWidget::~VideoDisplayWidget()
{
    stopDisplayTimer();
}

QSize VideoDisplayWidget::sizeHint() const
{
    // Return native resolution based on format
    int height =
        (videoFormat_ == VideoStreamReceiverService::VideoFormat::NTSC) ? NtscHeight : PalHeight;
    return {FrameWidth, height};
}

QSize VideoDisplayWidget::minimumSizeHint() const
{
    // Quarter resolution as minimum
    return {FrameWidth / 4, PalHeight / 4};
}

void VideoDisplayWidget::displayFrame(const QByteArray &frameData, quint16 frameNumber,
                                      VideoStreamReceiverService::VideoFormat format)
{
    static int frameCount = 0;
    frameCount++;
    if (frameCount <= 3 || frameCount % 50 == 0) {
        LOG_VERBOSE() << "VideoDisplayWidget::displayFrame: frame" << frameCount
                      << "data size:" << frameData.size() << "format:" << static_cast<int>(format);
    }

    // Handle format change - this affects timing so handle before buffering
    if (format != videoFormat_ && format != VideoStreamReceiverService::VideoFormat::Unknown) {
        LOG_VERBOSE() << "VideoDisplayWidget: Format changed to"
                      << (format == VideoStreamReceiverService::VideoFormat::PAL ? "PAL" : "NTSC");
        videoFormat_ = format;
        int height =
            (format == VideoStreamReceiverService::VideoFormat::NTSC) ? NtscHeight : PalHeight;
        displayImage_ = QImage(FrameWidth, height, QImage::Format_RGB32);

        // Restart timer with new frame rate if running
        if (displayTimer_->isActive()) {
            stopDisplayTimer();
            startDisplayTimer();
        }

        emit formatChanged(format);
    }

    if (framePacingEnabled_) {
        // Buffer the frame for paced display
        BufferedFrame bufferedFrame{frameData, frameNumber, format};
        frameBuffer_.enqueue(bufferedFrame);

        // Report buffer level change
        if (diagnosticsCallback_.onBufferLevelChanged) {
            diagnosticsCallback_.onBufferLevelChanged(frameBuffer_.size());
        }

        // Check if buffer is primed and start timer
        if (!bufferPrimed_ && frameBuffer_.size() >= frameBufferSize_ / 2) {
            bufferPrimed_ = true;
            startDisplayTimer();
        }

        // Prevent buffer overflow - drop oldest frames if too full
        while (frameBuffer_.size() > static_cast<qsizetype>(frameBufferSize_) * 2) {
            frameBuffer_.dequeue();
        }
    } else {
        // Display immediately (low latency mode)
        BufferedFrame frame{frameData, frameNumber, format};
        displayBufferedFrame(frame);
    }
}

void VideoDisplayWidget::clear()
{
    stopDisplayTimer();
    frameBuffer_.clear();
    bufferPrimed_ = false;
    displayImage_.fill(Qt::black);
    hasFrame_ = false;
    update();
}

void VideoDisplayWidget::setScalingMode(ScalingMode mode)
{
    if (scalingMode_ != mode) {
        scalingMode_ = mode;
        emit scalingModeChanged(mode);
        update();
    }
}

QImage VideoDisplayWidget::currentFrame() const
{
    if (!hasFrame_) {
        return {};
    }
    return displayImage_.copy();
}

void VideoDisplayWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);

    if (!hasFrame_) {
        // Stand in for the C64 with a C64 screen until frames arrive
        c64screen::paint(painter, rect(), placeholderLines_, themecore::effectiveScheme(), 0);
        paintKeyboardIndicator(painter);
        return;
    }

    // Calculate display rectangle based on scaling mode
    QRect displayRect = calculateDisplayRect();

    // Fill background outside video area
    painter.fillRect(rect(), Qt::black);

    // Apply scaling mode
    switch (scalingMode_) {
    case ScalingMode::Smooth:
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        break;
    case ScalingMode::Sharp:
    case ScalingMode::Integer:
    default:
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        break;
    }

    painter.drawImage(displayRect, displayImage_);
    paintKeyboardIndicator(painter);
}

void VideoDisplayWidget::paintKeyboardIndicator(QPainter &painter) const
{
    const bool focused = hasFocus();
    if (!focused && !hasFrame_) {
        return;
    }

    const themecore::Tokens tokens = themecore::currentTokens();
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (focused) {
        QPen pen(tokens.stateConnected, FocusFramePx);
        pen.setJoinStyle(Qt::MiterJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        const int half = FocusFramePx / 2;
        painter.drawRect(rect().adjusted(half, half, -half - 1, -half - 1));
    }

    QFont badgeFont(QStringLiteral("C64 Pro Mono"));
    badgeFont.setStyleHint(QFont::Monospace);
    badgeFont.setPixelSize(BadgeFontPx);
    painter.setFont(badgeFont);

    const QString text = focused ? tr("KEYS TO C64") : tr("CLICK TO TYPE");
    const QFontMetrics metrics(badgeFont);
    const QSize textSize(metrics.horizontalAdvance(text), metrics.height());
    const QRect badge(width() - BadgeInset - textSize.width() - 2 * BadgePaddingX,
                      height() - BadgeInset - textSize.height() - 2 * BadgePaddingY,
                      textSize.width() + 2 * BadgePaddingX, textSize.height() + 2 * BadgePaddingY);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 150));
    painter.drawRoundedRect(badge, BadgeRadius, BadgeRadius);

    painter.setPen(focused ? QColor(Qt::white) : QColor(255, 255, 255, 140));
    painter.drawText(badge, Qt::AlignCenter, text);
    painter.restore();
}

void VideoDisplayWidget::setPlaceholderLines(const QStringList &lines)
{
    if (placeholderLines_ == lines) {
        return;
    }
    placeholderLines_ = lines;
    if (!hasFrame_) {
        update();
    }
}

void VideoDisplayWidget::convertFrameToRgb(const QByteArray &frameData, int height)
{
    IVideoStreamReceiverService::VideoFormat fmt =
        (height == NtscHeight) ? IVideoStreamReceiverService::VideoFormat::NTSC
                               : IVideoStreamReceiverService::VideoFormat::PAL;
    displayImage_ = Vic2::convertFrame(frameData, fmt);
}

QRect VideoDisplayWidget::calculateDisplayRect() const
{
    if (displayImage_.isNull()) {
        return rect();
    }

    int imageWidth = displayImage_.width();
    int imageHeight = displayImage_.height();

    int displayWidth;
    int displayHeight;

    if (scalingMode_ == ScalingMode::Integer) {
        // Integer scaling: find largest integer multiplier that fits
        int scaleX = width() / imageWidth;
        int scaleY = height() / imageHeight;
        int scale = qMax(1, qMin(scaleX, scaleY));

        displayWidth = imageWidth * scale;
        displayHeight = imageHeight * scale;
    } else {
        // Aspect ratio preserving scaling (Sharp and Smooth modes)
        qreal imageAspect = static_cast<qreal>(imageWidth) / static_cast<qreal>(imageHeight);
        qreal widgetAspect = static_cast<qreal>(width()) / static_cast<qreal>(height());

        if (widgetAspect > imageAspect) {
            // Widget is wider than image - fit to height
            displayHeight = height();
            displayWidth = static_cast<int>(displayHeight * imageAspect);
        } else {
            // Widget is taller than image - fit to width
            displayWidth = width();
            displayHeight = static_cast<int>(displayWidth / imageAspect);
        }
    }

    // Center the image
    int x = (width() - displayWidth) / 2;
    int y = (height() - displayHeight) / 2;

    return {x, y, displayWidth, displayHeight};
}

void VideoDisplayWidget::keyPressEvent(QKeyEvent *event)
{
    // Escape belongs to the window (it leaves full screen), never to the C64
    if (event->key() == Qt::Key_Escape) {
        event->ignore();
        return;
    }

    // Emit signal for parent to handle
    emit keyPressed(event);

    // Don't call base implementation - we handle all other keys
    event->accept();
}

void VideoDisplayWidget::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    emit keyboardFocusChanged(true);
    update();
}

void VideoDisplayWidget::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);
    emit keyboardFocusChanged(false);
    update();
}

void VideoDisplayWidget::mousePressEvent(QMouseEvent *event)
{
    // Click to focus
    setFocus(Qt::MouseFocusReason);
    QWidget::mousePressEvent(event);
}

void VideoDisplayWidget::setFramePacingEnabled(bool enabled)
{
    if (framePacingEnabled_ == enabled) {
        return;
    }

    framePacingEnabled_ = enabled;

    if (!enabled) {
        // Switching to low latency mode - stop timer and clear buffer
        stopDisplayTimer();
        frameBuffer_.clear();
        bufferPrimed_ = false;
    }
    // If enabling, timer will start when buffer is primed
}

bool VideoDisplayWidget::isFramePacingEnabled() const
{
    return framePacingEnabled_;
}

int VideoDisplayWidget::bufferedFrames() const
{
    return frameBuffer_.size();
}

void VideoDisplayWidget::setDiagnosticsCallback(const DiagnosticsCallback &callback)
{
    diagnosticsCallback_ = callback;
    if (callback.onFrameDisplayed || callback.onDisplayUnderrun || callback.onBufferLevelChanged) {
        diagnosticsTimer_.start();
    }
}

void VideoDisplayWidget::onDisplayTimer()
{
    if (frameBuffer_.isEmpty()) {
        // Buffer underrun
        if (diagnosticsCallback_.onDisplayUnderrun) {
            diagnosticsCallback_.onDisplayUnderrun();
        }
        return;
    }

    BufferedFrame frame = frameBuffer_.dequeue();
    displayBufferedFrame(frame);

    // Report buffer level change
    if (diagnosticsCallback_.onBufferLevelChanged) {
        diagnosticsCallback_.onBufferLevelChanged(frameBuffer_.size());
    }
}

void VideoDisplayWidget::displayBufferedFrame(const BufferedFrame &frame)
{
    // Convert and display
    int height =
        (videoFormat_ == VideoStreamReceiverService::VideoFormat::NTSC) ? NtscHeight : PalHeight;
    convertFrameToRgb(frame.frameData, height);
    hasFrame_ = true;

    // Report frame display time
    if (diagnosticsCallback_.onFrameDisplayed && diagnosticsTimer_.isValid()) {
        qint64 timeUs = diagnosticsTimer_.nsecsElapsed() / 1000;
        diagnosticsCallback_.onFrameDisplayed(timeUs);
    }

    update();
}

void VideoDisplayWidget::startDisplayTimer()
{
    if (!displayTimer_->isActive()) {
        // Calculate interval based on video format
        // PAL: 50 Hz = 20ms, NTSC: 60 Hz ≈ 16.67ms
        double frameRate = (videoFormat_ == VideoStreamReceiverService::VideoFormat::NTSC)
                               ? NtscFrameRate
                               : PalFrameRate;
        int intervalMs = static_cast<int>(1000.0 / frameRate);
        displayTimer_->start(intervalMs);
    }
}

void VideoDisplayWidget::stopDisplayTimer()
{
    if (displayTimer_->isActive()) {
        displayTimer_->stop();
    }
}
