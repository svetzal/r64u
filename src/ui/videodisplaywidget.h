/**
 * @file videodisplaywidget.h
 * @brief Widget for displaying VIC-II video frames from the C64 Ultimate.
 *
 * Renders 4-bit VIC-II color indexed frames to RGB using the standard
 * VIC-II color palette and displays them with proper aspect ratio.
 */

#ifndef VIDEODISPLAYWIDGET_H
#define VIDEODISPLAYWIDGET_H

#include <QWidget>
#include <QImage>
#include <array>
#include "services/videostreamreceiver.h"

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
 * connect(receiver, &VideoStreamReceiver::frameReady,
 *         display, &VideoDisplayWidget::displayFrame);
 * @endcode
 */
class VideoDisplayWidget : public QWidget
{
    Q_OBJECT

public:
    /// Frame width in pixels
    static constexpr int FrameWidth = 384;

    /// PAL frame height
    static constexpr int PalHeight = 272;

    /// NTSC frame height
    static constexpr int NtscHeight = 240;

    /// Bytes per line (384 pixels at 4 bits = 192 bytes)
    static constexpr int BytesPerLine = 192;

    /**
     * @brief Constructs a video display widget.
     * @param parent Optional parent widget.
     */
    explicit VideoDisplayWidget(QWidget *parent = nullptr);

    /**
     * @brief Returns the current video format.
     * @return The video format (PAL, NTSC, or Unknown).
     */
    [[nodiscard]] VideoStreamReceiver::VideoFormat videoFormat() const { return videoFormat_; }

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
    void displayFrame(const QByteArray &frameData,
                      quint16 frameNumber,
                      VideoStreamReceiver::VideoFormat format);

    /**
     * @brief Clears the display to black.
     */
    void clear();

signals:
    /**
     * @brief Emitted when the video format changes.
     * @param format The new video format.
     */
    void formatChanged(VideoStreamReceiver::VideoFormat format);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void convertFrameToRgb(const QByteArray &frameData, int height);
    [[nodiscard]] QRect calculateDisplayRect() const;

    /// Standard VIC-II color palette (RGB values)
    static const std::array<QRgb, 16> VicPalette;

    // Display state
    QImage displayImage_;
    VideoStreamReceiver::VideoFormat videoFormat_ = VideoStreamReceiver::VideoFormat::Unknown;
    bool hasFrame_ = false;
};

#endif // VIDEODISPLAYWIDGET_H
