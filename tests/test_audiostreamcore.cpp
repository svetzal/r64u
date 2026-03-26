/**
 * @file test_audiostreamcore.cpp
 * @brief Unit tests for pure audio stream packet parsing and rate calculation functions.
 *
 * Tests the audiostream:: namespace functions which are pure (no I/O, no sockets).
 * Exercises packet parsing, sequence number analysis, and flush interval calculation.
 */

#include <QtTest>

#include <services/audiostreamcore.h>

class TestAudioStreamCore : public QObject
{
    Q_OBJECT

private:
    /// Build a 770-byte packet with the given sequence number and a zeroed payload.
    static QByteArray makePacket(quint16 seq)
    {
        QByteArray packet(audiostream::PacketSize, '\0');
        packet[0] = static_cast<char>(seq & 0xFF);
        packet[1] = static_cast<char>((seq >> 8) & 0xFF);
        return packet;
    }

private slots:
    // =========================================================
    // parsePacket
    // =========================================================

    void parsePacket_validPacket_returnsResult()
    {
        QByteArray packet = makePacket(42);
        auto result = audiostream::parsePacket(packet);
        QVERIFY(result.has_value());
    }

    void parsePacket_tooSmall_returnsNullopt()
    {
        QByteArray packet(audiostream::PacketSize - 1, '\0');
        QVERIFY(!audiostream::parsePacket(packet).has_value());
    }

    void parsePacket_tooLarge_returnsNullopt()
    {
        QByteArray packet(audiostream::PacketSize + 1, '\0');
        QVERIFY(!audiostream::parsePacket(packet).has_value());
    }

    void parsePacket_empty_returnsNullopt()
    {
        QVERIFY(!audiostream::parsePacket(QByteArray{}).has_value());
    }

    void parsePacket_sequenceNumber_littleEndian()
    {
        // 0x0100 → lo = 0x00, hi = 0x01 → sequence 256
        QByteArray packet(audiostream::PacketSize, '\0');
        packet[0] = static_cast<char>(0x00);
        packet[1] = static_cast<char>(0x01);

        auto result = audiostream::parsePacket(packet);
        QVERIFY(result.has_value());
        QCOMPARE(result->sequenceNumber, static_cast<quint16>(256));
    }

    void parsePacket_payloadExtractionSkipsHeader()
    {
        // Fill payload area (bytes 2–769) with 0xAB, header with 0
        QByteArray packet(audiostream::PacketSize, static_cast<char>(0xAB));
        packet[0] = 0;
        packet[1] = 0;

        auto result = audiostream::parsePacket(packet);
        QVERIFY(result.has_value());

        QCOMPARE(result->samples.size(), audiostream::PayloadSize);
        // Every byte in the payload should be 0xAB
        for (int i = 0; i < result->samples.size(); ++i) {
            QCOMPARE(static_cast<quint8>(result->samples[i]), static_cast<quint8>(0xAB));
        }
    }

    void parsePacket_sequenceNumber_loByteVariation()
    {
        // Sequence 1 → lo = 0x01, hi = 0x00
        auto result = audiostream::parsePacket(makePacket(1));
        QVERIFY(result.has_value());
        QCOMPARE(result->sequenceNumber, static_cast<quint16>(1));
    }

    // =========================================================
    // analyzeSequence — normal cases
    // =========================================================

    void analyzeSequence_consecutive_noDiscontinuity()
    {
        auto analysis = audiostream::analyzeSequence(6, 5);
        QVERIFY(!analysis.isDiscontinuity);
        QCOMPARE(analysis.gap, static_cast<quint16>(0));
    }

    void analyzeSequence_gap_isDiscontinuity()
    {
        // 5 → 8: expected 6, got 8, gap = 2
        auto analysis = audiostream::analyzeSequence(8, 5);
        QVERIFY(analysis.isDiscontinuity);
        QCOMPARE(analysis.gap, static_cast<quint16>(2));
    }

    void analyzeSequence_singleMissingPacket()
    {
        // 10 → 12: gap = 1
        auto analysis = audiostream::analyzeSequence(12, 10);
        QVERIFY(analysis.isDiscontinuity);
        QCOMPARE(analysis.gap, static_cast<quint16>(1));
    }

    // =========================================================
    // analyzeSequence — wraparound
    // =========================================================

    void analyzeSequence_wraparound_noDiscontinuity()
    {
        // Normal 0xFFFF → 0 wraparound
        auto analysis = audiostream::analyzeSequence(0, 0xFFFF);
        QVERIFY(!analysis.isDiscontinuity);
    }

    void analyzeSequence_wraparoundWithGap()
    {
        // 0xFFFE → 1: expected 0xFFFF, got 1, gap = 2
        auto analysis = audiostream::analyzeSequence(1, 0xFFFE);
        QVERIFY(analysis.isDiscontinuity);
        QCOMPARE(analysis.gap, static_cast<quint16>(2));
    }

    // =========================================================
    // analyzeSequence — large gap (treated as reset)
    // =========================================================

    void analyzeSequence_largeGap_notDiscontinuity()
    {
        // Gap > 1000 → treated as a counter reset, not a discontinuity
        auto analysis = audiostream::analyzeSequence(5000, 5);
        QVERIFY(!analysis.isDiscontinuity);
    }

    // =========================================================
    // calculateFlushIntervalUs
    // =========================================================

    void calculateFlushInterval_palRate()
    {
        // 192 / 47982.887 * 1e6 ≈ 4001 µs
        int interval = audiostream::calculateFlushIntervalUs(audiostream::PalSampleRate);
        QVERIFY(interval >= 3900);
        QVERIFY(interval <= 4100);
    }

    void calculateFlushInterval_ntscRate()
    {
        // 192 / 47940.341 * 1e6 ≈ 4005 µs
        int interval = audiostream::calculateFlushIntervalUs(audiostream::NtscSampleRate);
        QVERIFY(interval >= 3900);
        QVERIFY(interval <= 4100);
    }

    // =========================================================
    // sampleRateForFormat
    // =========================================================

    void sampleRateForFormat_pal()
    {
        QCOMPARE(audiostream::sampleRateForFormat(false), audiostream::PalSampleRate);
    }

    void sampleRateForFormat_ntsc()
    {
        QCOMPARE(audiostream::sampleRateForFormat(true), audiostream::NtscSampleRate);
    }
};

QTEST_MAIN(TestAudioStreamCore)
#include "test_audiostreamcore.moc"
