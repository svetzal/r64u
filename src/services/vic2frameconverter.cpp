#include "vic2frameconverter.h"

namespace Vic2 {

static constexpr int kFrameWidth = 384;
static constexpr int kPalHeight = 272;
static constexpr int kNtscHeight = 240;
static constexpr int kBytesPerLine = 192;

QImage convertFrame(const QByteArray &frameData, IVideoStreamReceiver::VideoFormat format)
{
    int height = (format == IVideoStreamReceiver::VideoFormat::NTSC) ? kNtscHeight : kPalHeight;

    QImage frame(kFrameWidth, height, QImage::Format_RGB32);

    static const QRgb vicPalette[16] = {
        qRgb(0x00, 0x00, 0x00),  // Black
        qRgb(0xFF, 0xFF, 0xFF),  // White
        qRgb(0x9F, 0x4E, 0x44),  // Red
        qRgb(0x6A, 0xBF, 0xC6),  // Cyan
        qRgb(0xA0, 0x57, 0xA3),  // Purple
        qRgb(0x5C, 0xAB, 0x5E),  // Green
        qRgb(0x50, 0x45, 0x9B),  // Blue
        qRgb(0xC9, 0xD4, 0x87),  // Yellow
        qRgb(0xA1, 0x68, 0x3C),  // Orange
        qRgb(0x6D, 0x54, 0x12),  // Brown
        qRgb(0xCB, 0x7E, 0x75),  // Light Red
        qRgb(0x62, 0x62, 0x62),  // Dark Grey
        qRgb(0x89, 0x89, 0x89),  // Medium Grey
        qRgb(0x9A, 0xE2, 0x9B),  // Light Green
        qRgb(0x88, 0x7E, 0xCB),  // Light Blue
        qRgb(0xAD, 0xAD, 0xAD)   // Light Grey
    };

    const auto *src = reinterpret_cast<const quint8 *>(frameData.constData());
    int srcSize = frameData.size();

    for (int y = 0; y < height && y * kBytesPerLine < srcSize; ++y) {
        auto *destLine = reinterpret_cast<QRgb *>(frame.scanLine(y));
        const quint8 *srcLine = src + (static_cast<ptrdiff_t>(y) * kBytesPerLine);

        for (int byteIdx = 0; byteIdx < kBytesPerLine; ++byteIdx) {
            quint8 packedPixels = srcLine[byteIdx];

            quint8 pixel1 = packedPixels & 0x0F;
            quint8 pixel2 = (packedPixels >> 4) & 0x0F;

            int x = byteIdx * 2;
            if (x < kFrameWidth) {
                destLine[x] = vicPalette[pixel1];
            }
            if (x + 1 < kFrameWidth) {
                destLine[x + 1] = vicPalette[pixel2];
            }
        }
    }

    return frame;
}

}  // namespace Vic2
