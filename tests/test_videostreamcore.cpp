/**
 * @file test_videostreamcore.cpp
 * @brief Unit tests for pure video stream packet parsing and format detection functions.
 *
 * Tests the videostream:: namespace functions which are pure (no I/O, no sockets).
 * Exercises header parsing, sequence number analysis, and frame line computation.
 */

#include <QtTest>

#include <services/videostreamcore.h>

class TestVideoStreamCore : public QObject
{
    Q_OBJECT

private:
    /// Build a VideoStreamReceiver-sized (780 byte) packet with the given header fields.
    static QByteArray makePacket(quint16 seqNum, quint16 frameNum, quint16 lineNum,
                                 quint8 linesPerPacket = 4, bool lastPacket = false)
    {
        QByteArray packet(videostream::PacketSize, '\0');
        if (lastPacket) {
            lineNum |= 0x8000;
        }
        packet[0] = static_cast<char>(seqNum & 0xFF);
        packet[1] = static_cast<char>((seqNum >> 8) & 0xFF);
        packet[2] = static_cast<char>(frameNum & 0xFF);
        packet[3] = static_cast<char>((frameNum >> 8) & 0xFF);
        packet[4] = static_cast<char>(lineNum & 0xFF);
        packet[5] = static_cast<char>((lineNum >> 8) & 0xFF);
        packet[6] = static_cast<char>(videostream::PixelsPerLine & 0xFF);
        packet[7] = static_cast<char>((videostream::PixelsPerLine >> 8) & 0xFF);
        packet[8] = linesPerPacket;
        packet[9] = static_cast<char>(videostream::BitsPerPixel);
        packet[10] = 0;
        packet[11] = 0;
        return packet;
    }

private slots:
    // =========================================================
    // Constants
    // =========================================================

    void constants_packetSize() { QCOMPARE(videostream::PacketSize, 780); }
    void constants_headerSize() { QCOMPARE(videostream::HeaderSize, 12); }
    void constants_payloadSize() { QCOMPARE(videostream::PayloadSize, 768); }
    void constants_pixelsPerLine() { QCOMPARE(videostream::PixelsPerLine, 384); }
    void constants_linesPerPacket() { QCOMPARE(videostream::LinesPerPacket, 4); }
    void constants_bitsPerPixel() { QCOMPARE(videostream::BitsPerPixel, 4); }
    void constants_bytesPerLine() { QCOMPARE(videostream::BytesPerLine, 192); }
    void constants_maxFrameHeight() { QCOMPARE(videostream::MaxFrameHeight, 272); }
    void constants_palHeight() { QCOMPARE(videostream::PalHeight, 272); }
    void constants_ntscHeight() { QCOMPARE(videostream::NtscHeight, 240); }
    void constants_palPacketsPerFrame() { QCOMPARE(videostream::PalPacketsPerFrame, 68); }
    void constants_ntscPacketsPerFrame() { QCOMPARE(videostream::NtscPacketsPerFrame, 60); }

    // =========================================================
    // parseHeader — field decoding
    // =========================================================

    void parseHeader_sequenceNumber_littleEndian()
    {
        QByteArray packet(videostream::PacketSize, '\0');
        packet[0] = static_cast<char>(0x34);
        packet[1] = static_cast<char>(0x12);

        auto header = videostream::parseHeader(packet);
        QCOMPARE(header.sequenceNumber, static_cast<quint16>(0x1234));
    }

    void parseHeader_frameNumber_littleEndian()
    {
        QByteArray packet(videostream::PacketSize, '\0');
        packet[2] = static_cast<char>(0xCD);
        packet[3] = static_cast<char>(0xAB);

        auto header = videostream::parseHeader(packet);
        QCOMPARE(header.frameNumber, static_cast<quint16>(0xABCD));
    }

    void parseHeader_lineNumber_littleEndian()
    {
        QByteArray packet(videostream::PacketSize, '\0');
        packet[4] = static_cast<char>(0x08);
        packet[5] = static_cast<char>(0x00);

        auto header = videostream::parseHeader(packet);
        // Bit 15 not set, so actualLineNumber == lineNumber
        QCOMPARE(header.actualLineNumber(), static_cast<quint16>(8));
        QVERIFY(!header.isLastPacket());
    }

    void parseHeader_lastPacketFlag_bit15Set()
    {
        QByteArray packet(videostream::PacketSize, '\0');
        // lineNum = 0x8010 → bit 15 set, actual line = 0x0010 = 16
        packet[4] = static_cast<char>(0x10);
        packet[5] = static_cast<char>(0x80);

        auto header = videostream::parseHeader(packet);
        QVERIFY(header.isLastPacket());
        QCOMPARE(header.actualLineNumber(), static_cast<quint16>(0x10));
    }

    void parseHeader_linesPerPacket()
    {
        QByteArray packet(videostream::PacketSize, '\0');
        packet[8] = static_cast<char>(4);

        auto header = videostream::parseHeader(packet);
        QCOMPARE(header.linesPerPacket, static_cast<quint8>(4));
    }

    void parseHeader_bitsPerPixel()
    {
        QByteArray packet(videostream::PacketSize, '\0');
        packet[9] = static_cast<char>(4);

        auto header = videostream::parseHeader(packet);
        QCOMPARE(header.bitsPerPixel, static_cast<quint8>(4));
    }

    void parseHeader_encodingType_littleEndian()
    {
        QByteArray packet(videostream::PacketSize, '\0');
        packet[10] = static_cast<char>(0x01);
        packet[11] = static_cast<char>(0x00);

        auto header = videostream::parseHeader(packet);
        QCOMPARE(header.encodingType, static_cast<quint16>(0x0001));
    }

    void parseHeader_roundTrip_knownValues()
    {
        // seq=100, frame=5, line=268 (last packet of PAL frame: 268 + 4 = 272)
        QByteArray packet = makePacket(100, 5, 268, 4, true);
        auto header = videostream::parseHeader(packet);

        QCOMPARE(header.sequenceNumber, static_cast<quint16>(100));
        QCOMPARE(header.frameNumber, static_cast<quint16>(5));
        QCOMPARE(header.actualLineNumber(), static_cast<quint16>(268));
        QVERIFY(header.isLastPacket());
        QCOMPARE(header.linesPerPacket, static_cast<quint8>(4));
        QCOMPARE(header.bitsPerPixel, static_cast<quint8>(videostream::BitsPerPixel));
    }

    // =========================================================
    // analyzeSequence — normal cases
    // =========================================================

    void analyzeSequence_consecutive_noEvents()
    {
        auto result = videostream::analyzeSequence(6, 5);
        QVERIFY(!result.isLoss);
        QVERIFY(!result.isOutOfOrder);
        QCOMPARE(result.gap, static_cast<quint16>(0));
    }

    void analyzeSequence_gap1_isLoss()
    {
        // 5 → 7: expected 6, got 7, gap = 1
        auto result = videostream::analyzeSequence(7, 5);
        QVERIFY(result.isLoss);
        QVERIFY(!result.isOutOfOrder);
        QCOMPARE(result.gap, static_cast<quint16>(1));
    }

    void analyzeSequence_gap5_isLoss()
    {
        // 10 → 16: gap = 5
        auto result = videostream::analyzeSequence(16, 10);
        QVERIFY(result.isLoss);
        QCOMPARE(result.gap, static_cast<quint16>(5));
    }

    void analyzeSequence_gap999_isLoss()
    {
        // Gap just inside the loss threshold
        auto result = videostream::analyzeSequence(1000, 0);
        QVERIFY(result.isLoss);
        QCOMPARE(result.gap, static_cast<quint16>(999));
    }

    // =========================================================
    // analyzeSequence — wraparound
    // =========================================================

    void analyzeSequence_wraparound_noEvents()
    {
        // Normal 0xFFFF → 0 wraparound
        auto result = videostream::analyzeSequence(0, 0xFFFF);
        QVERIFY(!result.isLoss);
        QVERIFY(!result.isOutOfOrder);
    }

    void analyzeSequence_wraparoundWithGap_isLoss()
    {
        // 0xFFFE → 1: expected 0xFFFF, got 1 → gap = 2
        auto result = videostream::analyzeSequence(1, 0xFFFE);
        QVERIFY(result.isLoss);
        QCOMPARE(result.gap, static_cast<quint16>(2));
    }

    // =========================================================
    // analyzeSequence — out-of-order (backwards sequence)
    // =========================================================

    void analyzeSequence_backwards_isOutOfOrder()
    {
        // 100 → 95: expected 101, gap = 0xFFFF - 5 = large backwards step
        auto result = videostream::analyzeSequence(95, 100);
        QVERIFY(!result.isLoss);
        QVERIFY(result.isOutOfOrder);
        QCOMPARE(result.gap, static_cast<quint16>(0));
    }

    // =========================================================
    // analyzeSequence — large gap (counter reset, not reported)
    // =========================================================

    void analyzeSequence_largeGap_notReported()
    {
        // Gap > 1000 but <= 0xF000 → counter reset, neither loss nor out-of-order
        auto result = videostream::analyzeSequence(5000, 5);
        QVERIFY(!result.isLoss);
        QVERIFY(!result.isOutOfOrder);
    }

    // =========================================================
    // computeFrameLines
    // =========================================================

    void computeFrameLines_pal()
    {
        // PAL last packet: actualLineNumber = 268, linesPerPacket = 4 → 272
        QByteArray packet = makePacket(67, 1, 268, 4, true);
        auto header = videostream::parseHeader(packet);
        QCOMPARE(videostream::computeFrameLines(header), videostream::PalHeight);
    }

    void computeFrameLines_ntsc()
    {
        // NTSC last packet: actualLineNumber = 236, linesPerPacket = 4 → 240
        QByteArray packet = makePacket(59, 1, 236, 4, true);
        auto header = videostream::parseHeader(packet);
        QCOMPARE(videostream::computeFrameLines(header), videostream::NtscHeight);
    }

    void computeFrameLines_unknown()
    {
        // Non-standard: actualLineNumber = 100, linesPerPacket = 4 → 104
        QByteArray packet = makePacket(25, 1, 100, 4, true);
        auto header = videostream::parseHeader(packet);
        int lines = videostream::computeFrameLines(header);
        QVERIFY(lines != videostream::PalHeight);
        QVERIFY(lines != videostream::NtscHeight);
    }

    void computeFrameLines_firstPacket_notLastPacket()
    {
        // First packet of a frame: line 0, 4 lines → total 4 (not PAL or NTSC)
        QByteArray packet = makePacket(0, 1, 0, 4, false);
        auto header = videostream::parseHeader(packet);
        QCOMPARE(videostream::computeFrameLines(header), 4);
    }
};

QTEST_MAIN(TestVideoStreamCore)
#include "test_videostreamcore.moc"
