#include "services/vic2frameconverter.h"

#include <QtTest>

class TestVic2FrameConverter : public QObject
{
    Q_OBJECT

private:
    static constexpr int kFrameWidth = 384;
    static constexpr int kPalHeight = 272;
    static constexpr int kNtscHeight = 240;
    static constexpr int kBytesPerLine = 192;

    static QByteArray makeFrameData(int height, quint8 fill = 0x00)
    {
        return QByteArray(height * kBytesPerLine, static_cast<char>(fill));
    }

private slots:

    void testPalFormat_producesCorrectDimensions()
    {
        auto data = makeFrameData(kPalHeight);
        auto frame = Vic2::convertFrame(data, IVideoStreamReceiver::VideoFormat::PAL);
        QCOMPARE(frame.width(), kFrameWidth);
        QCOMPARE(frame.height(), kPalHeight);
        QCOMPARE(frame.format(), QImage::Format_RGB32);
    }

    void testNtscFormat_producesCorrectDimensions()
    {
        auto data = makeFrameData(kNtscHeight);
        auto frame = Vic2::convertFrame(data, IVideoStreamReceiver::VideoFormat::NTSC);
        QCOMPARE(frame.width(), kFrameWidth);
        QCOMPARE(frame.height(), kNtscHeight);
        QCOMPARE(frame.format(), QImage::Format_RGB32);
    }

    void testUnknownFormat_fallsBackToPalHeight()
    {
        auto data = makeFrameData(kPalHeight);
        auto frame = Vic2::convertFrame(data, IVideoStreamReceiver::VideoFormat::Unknown);
        QCOMPARE(frame.height(), kPalHeight);
    }

    void testEmptyBuffer_returnsCorrectSizeWithoutCrash()
    {
        QByteArray empty;
        auto frame = Vic2::convertFrame(empty, IVideoStreamReceiver::VideoFormat::PAL);
        QCOMPARE(frame.width(), kFrameWidth);
        QCOMPARE(frame.height(), kPalHeight);
        QCOMPARE(frame.format(), QImage::Format_RGB32);
    }

    void testTruncatedBuffer_doesNotCrash()
    {
        QByteArray partial(kBytesPerLine * 10, '\x00');
        auto frame = Vic2::convertFrame(partial, IVideoStreamReceiver::VideoFormat::PAL);
        QCOMPARE(frame.width(), kFrameWidth);
        QCOMPARE(frame.height(), kPalHeight);
    }

    void testFirstByteAllZeros_pixel0And1AreBlack()
    {
        auto data = makeFrameData(kPalHeight, 0x00);
        auto frame = Vic2::convertFrame(data, IVideoStreamReceiver::VideoFormat::PAL);
        QCOMPARE(frame.pixel(0, 0), qRgb(0x00, 0x00, 0x00));
        QCOMPARE(frame.pixel(1, 0), qRgb(0x00, 0x00, 0x00));
    }

    void testFirstByte0x11_pixel0And1AreWhite()
    {
        auto data = makeFrameData(kPalHeight, 0x00);
        data[0] = static_cast<char>(0x11);
        auto frame = Vic2::convertFrame(data, IVideoStreamReceiver::VideoFormat::PAL);
        QCOMPARE(frame.pixel(0, 0), qRgb(0xFF, 0xFF, 0xFF));
        QCOMPARE(frame.pixel(1, 0), qRgb(0xFF, 0xFF, 0xFF));
    }

    void testPixelUnpacking_lowNibbleIsX0_highNibbleIsX1()
    {
        auto data = makeFrameData(kPalHeight, 0x00);
        data[0] = static_cast<char>(0x21);
        auto frame = Vic2::convertFrame(data, IVideoStreamReceiver::VideoFormat::PAL);
        QCOMPARE(frame.pixel(0, 0), qRgb(0xFF, 0xFF, 0xFF));
        QCOMPARE(frame.pixel(1, 0), qRgb(0x9F, 0x4E, 0x44));
    }
};

QTEST_MAIN(TestVic2FrameConverter)
#include "test_vic2frameconverter.moc"
