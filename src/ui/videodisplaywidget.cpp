/**
 * @file videodisplaywidget.cpp
 * @brief Implementation of the VIC-II video display widget.
 */

#include "videodisplaywidget.h"
#include <QPainter>

// Standard VIC-II color palette
// Values from: https://www.pepto.de/projects/colorvic/
const std::array<QRgb, 16> VideoDisplayWidget::VicPalette = {{
    qRgb(0x00, 0x00, 0x00),  // 0: Black
    qRgb(0xFF, 0xFF, 0xFF),  // 1: White
    qRgb(0x9F, 0x4E, 0x44),  // 2: Red
    qRgb(0x6A, 0xBF, 0xC6),  // 3: Cyan
    qRgb(0xA0, 0x57, 0xA3),  // 4: Purple
    qRgb(0x5C, 0xAB, 0x5E),  // 5: Green
    qRgb(0x50, 0x45, 0x9B),  // 6: Blue
    qRgb(0xC9, 0xD4, 0x87),  // 7: Yellow
    qRgb(0xA1, 0x68, 0x3C),  // 8: Orange
    qRgb(0x6D, 0x54, 0x12),  // 9: Brown
    qRgb(0xCB, 0x7E, 0x75),  // 10: Light Red
    qRgb(0x62, 0x62, 0x62),  // 11: Dark Grey
    qRgb(0x89, 0x89, 0x89),  // 12: Medium Grey
    qRgb(0x9A, 0xE2, 0x9B),  // 13: Light Green
    qRgb(0x88, 0x7E, 0xCB),  // 14: Light Blue
    qRgb(0xAD, 0xAD, 0xAD)   // 15: Light Grey
}};

VideoDisplayWidget::VideoDisplayWidget(QWidget *parent)
    : QWidget(parent)
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
}

QSize VideoDisplayWidget::sizeHint() const
{
    // Return native resolution based on format
    int height = (videoFormat_ == VideoStreamReceiver::VideoFormat::NTSC)
                     ? NtscHeight : PalHeight;
    return {FrameWidth, height};
}

QSize VideoDisplayWidget::minimumSizeHint() const
{
    // Quarter resolution as minimum
    return {FrameWidth / 4, PalHeight / 4};
}

void VideoDisplayWidget::displayFrame(const QByteArray &frameData,
                                       quint16 /*frameNumber*/,
                                       VideoStreamReceiver::VideoFormat format)
{
    // Handle format change
    if (format != videoFormat_ && format != VideoStreamReceiver::VideoFormat::Unknown) {
        videoFormat_ = format;
        int height = (format == VideoStreamReceiver::VideoFormat::NTSC)
                         ? NtscHeight : PalHeight;
        displayImage_ = QImage(FrameWidth, height, QImage::Format_RGB32);
        emit formatChanged(format);
    }

    // Convert and display
    int height = (videoFormat_ == VideoStreamReceiver::VideoFormat::NTSC)
                     ? NtscHeight : PalHeight;
    convertFrameToRgb(frameData, height);
    hasFrame_ = true;

    update();
}

void VideoDisplayWidget::clear()
{
    displayImage_.fill(Qt::black);
    hasFrame_ = false;
    update();
}

void VideoDisplayWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);

    if (!hasFrame_) {
        // Just show black background
        painter.fillRect(rect(), Qt::black);
        return;
    }

    // Calculate display rectangle maintaining aspect ratio
    QRect displayRect = calculateDisplayRect();

    // Fill background outside video area
    painter.fillRect(rect(), Qt::black);

    // Draw scaled image with smooth transformation
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(displayRect, displayImage_);
}

void VideoDisplayWidget::convertFrameToRgb(const QByteArray &frameData, int height)
{
    const auto *src = reinterpret_cast<const quint8 *>(frameData.constData());
    int srcSize = frameData.size();

    for (int y = 0; y < height && y * BytesPerLine < srcSize; ++y) {
        auto *destLine = reinterpret_cast<QRgb *>(displayImage_.scanLine(y));
        const quint8 *srcLine = src + (static_cast<ptrdiff_t>(y) * BytesPerLine);

        for (int byteIdx = 0; byteIdx < BytesPerLine; ++byteIdx) {
            quint8 packedPixels = srcLine[byteIdx];

            // Lower 4 bits = first pixel
            quint8 pixel1 = packedPixels & 0x0F;
            // Upper 4 bits = second pixel
            quint8 pixel2 = (packedPixels >> 4) & 0x0F;

            int x = byteIdx * 2;
            if (x < FrameWidth) {
                destLine[x] = VicPalette[pixel1];
            }
            if (x + 1 < FrameWidth) {
                destLine[x + 1] = VicPalette[pixel2];
            }
        }
    }
}

QRect VideoDisplayWidget::calculateDisplayRect() const
{
    if (displayImage_.isNull()) {
        return rect();
    }

    // Calculate aspect ratio preserving rectangle
    qreal imageAspect = static_cast<qreal>(displayImage_.width()) /
                        static_cast<qreal>(displayImage_.height());
    qreal widgetAspect = static_cast<qreal>(width()) /
                         static_cast<qreal>(height());

    int displayWidth;
    int displayHeight;

    if (widgetAspect > imageAspect) {
        // Widget is wider than image - fit to height
        displayHeight = height();
        displayWidth = static_cast<int>(displayHeight * imageAspect);
    } else {
        // Widget is taller than image - fit to width
        displayWidth = width();
        displayHeight = static_cast<int>(displayWidth / imageAspect);
    }

    // Center the image
    int x = (width() - displayWidth) / 2;
    int y = (height() - displayHeight) / 2;

    return {x, y, displayWidth, displayHeight};
}
