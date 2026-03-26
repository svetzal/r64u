/**
 * @file test_avicore.cpp
 * @brief Unit tests for pure AVI RIFF container data structure functions.
 *
 * Tests the avi:: namespace functions which are pure (no I/O, no file handles).
 * Exercises all byte-level encoding paths including the RIFF chunk structure,
 * idx1 index, fps calculation, and header construction.
 */

#include <QtTest>

#include <services/avicore.h>

class TestAviCore : public QObject
{
    Q_OBJECT

private slots:
    // =========================================================
    // writeLittleEndian32
    // =========================================================

    void writeLittleEndian32_zeroValue()
    {
        auto bytes = avi::writeLittleEndian32(0);
        QCOMPARE(bytes.size(), 4);
        QCOMPARE(static_cast<quint8>(bytes[0]), static_cast<quint8>(0x00));
        QCOMPARE(static_cast<quint8>(bytes[1]), static_cast<quint8>(0x00));
        QCOMPARE(static_cast<quint8>(bytes[2]), static_cast<quint8>(0x00));
        QCOMPARE(static_cast<quint8>(bytes[3]), static_cast<quint8>(0x00));
    }

    void writeLittleEndian32_maxValue()
    {
        auto bytes = avi::writeLittleEndian32(0xFFFFFFFF);
        QCOMPARE(static_cast<quint8>(bytes[0]), static_cast<quint8>(0xFF));
        QCOMPARE(static_cast<quint8>(bytes[1]), static_cast<quint8>(0xFF));
        QCOMPARE(static_cast<quint8>(bytes[2]), static_cast<quint8>(0xFF));
        QCOMPARE(static_cast<quint8>(bytes[3]), static_cast<quint8>(0xFF));
    }

    void writeLittleEndian32_knownValue()
    {
        // 256 = 0x00000100 → LE: 0x00, 0x01, 0x00, 0x00
        auto bytes = avi::writeLittleEndian32(256);
        QCOMPARE(static_cast<quint8>(bytes[0]), static_cast<quint8>(0x00));
        QCOMPARE(static_cast<quint8>(bytes[1]), static_cast<quint8>(0x01));
        QCOMPARE(static_cast<quint8>(bytes[2]), static_cast<quint8>(0x00));
        QCOMPARE(static_cast<quint8>(bytes[3]), static_cast<quint8>(0x00));
    }

    // =========================================================
    // buildChunk
    // =========================================================

    void buildChunk_correctFormat()
    {
        QByteArray data("hello", 5);
        QByteArray chunk = avi::buildChunk("aaa1", data);

        // fourCC
        QCOMPARE(chunk.left(4), QByteArray("aaa1"));
        // LE size (5 bytes)
        QCOMPARE(static_cast<quint8>(chunk[4]), static_cast<quint8>(5));
        QCOMPARE(static_cast<quint8>(chunk[5]), static_cast<quint8>(0));
        QCOMPARE(static_cast<quint8>(chunk[6]), static_cast<quint8>(0));
        QCOMPARE(static_cast<quint8>(chunk[7]), static_cast<quint8>(0));
        // Payload
        QCOMPARE(chunk.mid(8), data);
    }

    void buildChunk_emptyData()
    {
        QByteArray chunk = avi::buildChunk("RIFF", QByteArray());
        QCOMPARE(chunk.size(), 8);
        // Size should be 0
        QCOMPARE(static_cast<quint8>(chunk[4]), static_cast<quint8>(0));
    }

    void buildChunk_totalSize()
    {
        QByteArray data(100, 'X');
        QByteArray chunk = avi::buildChunk("00dc", data);
        // 4 (fourCC) + 4 (size) + 100 (data) = 108
        QCOMPARE(chunk.size(), 108);
    }

    // =========================================================
    // padToEven
    // =========================================================

    void padToEven_oddSize()
    {
        QByteArray odd("abc", 3);  // 3 bytes
        QByteArray padded = avi::padToEven(odd);
        QCOMPARE(padded.size(), 4);
        QCOMPARE(static_cast<quint8>(padded[3]), static_cast<quint8>(0));
    }

    void padToEven_evenSize()
    {
        QByteArray even("abcd", 4);  // 4 bytes
        QByteArray padded = avi::padToEven(even);
        QCOMPARE(padded.size(), 4);  // Unchanged
        QCOMPARE(padded, even);
    }

    void padToEven_emptyData()
    {
        QByteArray empty;
        QByteArray padded = avi::padToEven(empty);
        QVERIFY(padded.isEmpty());
    }

    // =========================================================
    // calculateFps
    // =========================================================

    void calculateFps_normalCase()
    {
        // 100 frames, first frame at 0ms, last at 3300ms → ~30fps
        double fps = avi::calculateFps(100, 3300);
        QVERIFY(fps >= 28.0);
        QVERIFY(fps <= 32.0);
    }

    void calculateFps_singleFrame()
    {
        // Single frame → default 30fps
        QCOMPARE(avi::calculateFps(1, 0), 30.0);
    }

    void calculateFps_zeroDuration()
    {
        // Duration ≤ 0 → clamp duration to 1ms (prevents division by zero)
        double fps = avi::calculateFps(60, 0);
        QCOMPARE(fps, 60.0);  // 60*1000/1 = 60000, clamped to 60
    }

    void calculateFps_clampsTo60()
    {
        // Very short duration → should clamp at 60
        double fps = avi::calculateFps(1000, 100);
        QCOMPARE(fps, 60.0);
    }

    void calculateFps_clampsTo1()
    {
        // Very long duration → should clamp at 1
        double fps = avi::calculateFps(2, 100000);
        QCOMPARE(fps, 1.0);
    }

    // =========================================================
    // calculateMicroSecPerFrame
    // =========================================================

    void calculateMicroSecPerFrame_30fps()
    {
        quint32 usec = avi::calculateMicroSecPerFrame(30.0);
        // 1000000 / 30 = 33333
        QVERIFY(usec >= 33330);
        QVERIFY(usec <= 33340);
    }

    // =========================================================
    // buildIdx1
    // =========================================================

    void buildIdx1_videoKeyframes()
    {
        avi::ChunkInfo chunk;
        chunk.fourCC = "00dc";
        chunk.offset = 0;
        chunk.size = 100;

        QByteArray idx1 = avi::buildIdx1({chunk});

        // Each entry is 16 bytes: ckid(4) + flags(4) + offset(4) + size(4)
        QCOMPARE(idx1.size(), 16);
        // Flag for video = 0x10
        QCOMPARE(static_cast<quint8>(idx1[4]), static_cast<quint8>(0x10));
        QCOMPARE(static_cast<quint8>(idx1[5]), static_cast<quint8>(0x00));
        QCOMPARE(static_cast<quint8>(idx1[6]), static_cast<quint8>(0x00));
        QCOMPARE(static_cast<quint8>(idx1[7]), static_cast<quint8>(0x00));
    }

    void buildIdx1_audioChunks()
    {
        avi::ChunkInfo chunk;
        chunk.fourCC = "01wb";
        chunk.offset = 0;
        chunk.size = 50;

        QByteArray idx1 = avi::buildIdx1({chunk});

        // Flag for audio = 0x00
        QCOMPARE(static_cast<quint8>(idx1[4]), static_cast<quint8>(0x00));
    }

    void buildIdx1_mixedChunks()
    {
        avi::ChunkInfo video;
        video.fourCC = "00dc";
        video.offset = 0;
        video.size = 100;

        avi::ChunkInfo audio;
        audio.fourCC = "01wb";
        audio.offset = 108;
        audio.size = 50;

        QByteArray idx1 = avi::buildIdx1({video, audio});
        // 2 entries × 16 bytes = 32
        QCOMPARE(idx1.size(), 32);
    }

    void buildIdx1_empty()
    {
        QByteArray idx1 = avi::buildIdx1({});
        QVERIFY(idx1.isEmpty());
    }

    // =========================================================
    // buildInitialHeader
    // =========================================================

    void buildInitialHeader_startsWithRIFF()
    {
        QByteArray header = avi::buildInitialHeader();
        QCOMPARE(header.left(4), QByteArray("RIFF"));
    }

    void buildInitialHeader_containsAVIMarker()
    {
        QByteArray header = avi::buildInitialHeader();
        QCOMPARE(header.mid(8, 4), QByteArray("AVI "));
    }

    void buildInitialHeader_endsWithMovi()
    {
        QByteArray header = avi::buildInitialHeader();
        // Last 4 bytes should be "movi"
        QCOMPARE(header.right(4), QByteArray("movi"));
    }

    void buildInitialHeader_containsMJPG()
    {
        QByteArray header = avi::buildInitialHeader();
        QVERIFY(header.contains("MJPG"));
    }

    // =========================================================
    // buildFinalizedHeader
    // =========================================================

    void buildFinalizedHeader_sameSize()
    {
        avi::StreamParams params;
        params.width = 384;
        params.height = 272;
        params.frameCount = 100;
        params.fps = 30.0;
        params.audioSampleCount = 48000;

        QByteArray initial = avi::buildInitialHeader();
        QByteArray finalized = avi::buildFinalizedHeader(params);

        // Must be exactly the same size so we can overwrite at offset 0
        QCOMPARE(finalized.size(), initial.size());
    }

    void buildFinalizedHeader_startsWithRIFF()
    {
        avi::StreamParams params;
        QByteArray header = avi::buildFinalizedHeader(params);
        QCOMPARE(header.left(4), QByteArray("RIFF"));
    }
};

QTEST_MAIN(TestAviCore)
#include "test_avicore.moc"
