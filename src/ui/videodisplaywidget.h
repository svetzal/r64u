#ifndef VIDEODISPLAYWIDGET_H
#define VIDEODISPLAYWIDGET_H

#include "services/videostreamreceiverservice.h"

#include <QElapsedTimer>
#include <QImage>
#include <QQueue>
#include <QRgb>
#include <QStringList>
#include <QTimer>
#include <QWidget>

class QPainter;

#include <functional>

/**
 * @brief Widget for rendering VIC-II video frames.
 *
 * This widget converts 4-bit VIC-II color indexed frames to RGB and
 * displays them scaled to fit the widget while maintaining aspect ratio.
 *
 * The widget supports both PAL (384x272) and NTSC (384x240) formats and
 * can switch between them dynamically based on detected video format.
 *
 * @par Example usage:
 * @code
 * VideoDisplayWidget *display = new VideoDisplayWidget(this);
 *
 * // Connect to video receiver
 * connect(receiver, &VideoStreamReceiverService::frameReady,
 *         display, &VideoDisplayWidget::displayFrame);
 * @endcode
 */
class VideoDisplayWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Video scaling mode for upscaling the display.
     */
    enum class ScalingMode {
        Sharp,   ///< Nearest-neighbor (no interpolation) - crisp pixels
        Smooth,  ///< Bilinear interpolation - smooth but fuzzy
        Integer  ///< Integer scaling with letterboxing - pixel-perfect
    };
    Q_ENUM(ScalingMode)

    /// Frame width in pixels
    static constexpr int FrameWidth = 384;

    /// PAL frame height
    static constexpr int PalHeight = 272;

    /// NTSC frame height
    static constexpr int NtscHeight = 240;

    /// Bytes per line (384 pixels at 4 bits = 192 bytes)
    static constexpr int BytesPerLine = 192;

    /// Default frame buffer size
    static constexpr int DefaultFrameBufferSize = 3;

    /// PAL frame rate (Hz)
    static constexpr double PalFrameRate = 50.0;

    /// NTSC frame rate (Hz)
    static constexpr double NtscFrameRate = 60.0;

    /**
     * @brief Constructs a video display widget.
     * @param parent Optional parent widget.
     */
    explicit VideoDisplayWidget(QWidget *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~VideoDisplayWidget() override;

    /**
     * @brief Returns the current video format.
     * @return The video format (PAL, NTSC, or Unknown).
     */
    [[nodiscard]] VideoStreamReceiverService::VideoFormat videoFormat() const
    {
        return videoFormat_;
    }

    /**
     * @brief Returns the current scaling mode.
     * @return The scaling mode.
     */
    [[nodiscard]] ScalingMode scalingMode() const { return scalingMode_; }

    /**
     * @brief Returns the current displayed frame as a QImage.
     * @return The current frame, or a null QImage if no frame is being displayed.
     */
    [[nodiscard]] QImage currentFrame() const;

    /**
     * @brief Sets the scaling mode.
     * @param mode The scaling mode to use.
     */
    void setScalingMode(ScalingMode mode);

    /**
     * @brief Enables or disables frame pacing.
     * @param enabled true to enable pacing (smoother), false for low latency.
     *
     * When enabled, frames are buffered and displayed at a steady rate.
     * When disabled, frames are displayed immediately on arrival.
     */
    void setFramePacingEnabled(bool enabled);

    /**
     * @brief Returns whether frame pacing is enabled.
     * @return true if pacing is enabled.
     */
    [[nodiscard]] bool isFramePacingEnabled() const;

    /**
     * @brief Returns the current frame buffer level.
     * @return Number of frames in the buffer.
     */
    [[nodiscard]] int bufferedFrames() const;

    /**
     * @brief Callback interface for diagnostics timing data.
     */
    struct DiagnosticsCallback
    {
        std::function<void(qint64 displayTimeUs)> onFrameDisplayed;
        std::function<void()> onDisplayUnderrun;
        std::function<void(int bufferLevel)> onBufferLevelChanged;
    };

    /**
     * @brief Sets the diagnostics callback for timing data.
     * @param callback Callback structure with timing functions.
     */
    void setDiagnosticsCallback(const DiagnosticsCallback &callback);

    /**
     * @brief Sets the lines shown on the C64 screen painted while no frame is available.
     * @param lines Screen lines, one per row, in the C64's uppercase set.
     */
    void setPlaceholderLines(const QStringList &lines);

    /**
     * @brief Returns the lines shown while no frame is available.
     */
    [[nodiscard]] const QStringList &placeholderLines() const { return placeholderLines_; }

    /**
     * @brief Returns the recommended size for the widget.
     * @return The size hint based on current video format.
     */
    [[nodiscard]] QSize sizeHint() const override;

    /**
     * @brief Returns the minimum size for the widget.
     * @return The minimum size (quarter of native resolution).
     */
    [[nodiscard]] QSize minimumSizeHint() const override;

public slots:
    /**
     * @brief Displays a video frame.
     * @param frameData Raw 4-bit indexed color data (packed, 2 pixels per byte).
     * @param frameNumber The frame sequence number.
     * @param format The video format (PAL or NTSC).
     */
    void displayFrame(const QByteArray &frameData, quint16 frameNumber,
                      VideoStreamReceiverService::VideoFormat format);

    /**
     * @brief Clears the display to black.
     */
    void clear();

signals:
    /**
     * @brief Emitted when the video format changes.
     * @param format The new video format.
     */
    void formatChanged(VideoStreamReceiverService::VideoFormat format);

    /**
     * @brief Emitted when a key is pressed while the widget has focus.
     * @param event The key event.
     */
    void keyPressed(QKeyEvent *event);

    /**
     * @brief Emitted when the scaling mode changes.
     * @param mode The new scaling mode.
     */
    void scalingModeChanged(ScalingMode mode);

    /**
     * @brief Emitted when the widget gains or loses keyboard focus.
     * @param focused true while key presses go to the C64.
     */
    void keyboardFocusChanged(bool focused);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private slots:
    void onDisplayTimer();

private:
    struct BufferedFrame
    {
        QByteArray frameData;
        quint16 frameNumber;
        VideoStreamReceiverService::VideoFormat format;
    };

    void convertFrameToRgb(const QByteArray &frameData, int height);
    [[nodiscard]] QRect calculateDisplayRect() const;
    void paintKeyboardIndicator(QPainter &painter) const;
    void displayBufferedFrame(const BufferedFrame &frame);
    void startDisplayTimer();
    void stopDisplayTimer();

    // Display state
    QImage displayImage_;
    VideoStreamReceiverService::VideoFormat videoFormat_ =
        VideoStreamReceiverService::VideoFormat::Unknown;
    ScalingMode scalingMode_ = ScalingMode::Integer;
    bool hasFrame_ = false;
    QStringList placeholderLines_;

    // Frame pacing
    bool framePacingEnabled_ = true;
    QQueue<BufferedFrame> frameBuffer_;
    QTimer *displayTimer_ = nullptr;
    int frameBufferSize_ = DefaultFrameBufferSize;
    bool bufferPrimed_ = false;

    // Diagnostics
    DiagnosticsCallback diagnosticsCallback_;
    QElapsedTimer diagnosticsTimer_;
};

#endif  // VIDEODISPLAYWIDGET_H
