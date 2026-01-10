#include <QtTest>
#include <QSignalSpy>
#include <QUdpSocket>

#include "services/videostreamreceiver.h"

class TestVideoStreamReceiver : public QObject
{
    Q_OBJECT

private:
    // Helper to create a video packet with specific header values
    QByteArray createVideoPacket(quint16 seqNum, quint16 frameNum, quint16 lineNum,
                                  bool isLast = false, int linesPerPacket = 4)
    {
        QByteArray packet(VideoStreamReceiver::PacketSize, '\0');

        // Set actual line number with optional last packet flag
        quint16 lineField = lineNum;
        if (isLast) {
            lineField |= 0x8000;  // Set bit 15 for last packet
        }

        // Header (12 bytes, little-endian)
        packet[0] = static_cast<char>(seqNum & 0xFF);
        packet[1] = static_cast<char>((seqNum >> 8) & 0xFF);
        packet[2] = static_cast<char>(frameNum & 0xFF);
        packet[3] = static_cast<char>((frameNum >> 8) & 0xFF);
        packet[4] = static_cast<char>(lineField & 0xFF);
        packet[5] = static_cast<char>((lineField >> 8) & 0xFF);
        packet[6] = static_cast<char>(VideoStreamReceiver::PixelsPerLine & 0xFF);
        packet[7] = static_cast<char>((VideoStreamReceiver::PixelsPerLine >> 8) & 0xFF);
        packet[8] = static_cast<char>(linesPerPacket);  // lines per packet
        packet[9] = static_cast<char>(VideoStreamReceiver::BitsPerPixel);  // bits per pixel
        packet[10] = 0;  // encoding type low
        packet[11] = 0;  // encoding type high

        // Fill payload with test pattern
        for (int i = VideoStreamReceiver::HeaderSize; i < VideoStreamReceiver::PacketSize; i++) {
            packet[i] = static_cast<char>(i % 256);
        }

        return packet;
    }

    // Create a complete PAL frame's worth of packets
    QVector<QByteArray> createPalFrame(quint16 frameNum, quint16 startSeq)
    {
        QVector<QByteArray> packets;
        // PAL: 68 packets (272 lines / 4 lines per packet)
        for (int i = 0; i < VideoStreamReceiver::PalPacketsPerFrame; i++) {
            quint16 lineNum = static_cast<quint16>(i * 4);
            bool isLast = (i == VideoStreamReceiver::PalPacketsPerFrame - 1);
            packets.append(createVideoPacket(startSeq + i, frameNum, lineNum, isLast));
        }
        return packets;
    }

    // Create a complete NTSC frame's worth of packets
    QVector<QByteArray> createNtscFrame(quint16 frameNum, quint16 startSeq)
    {
        QVector<QByteArray> packets;
        // NTSC: 60 packets (240 lines / 4 lines per packet)
        for (int i = 0; i < VideoStreamReceiver::NtscPacketsPerFrame; i++) {
            quint16 lineNum = static_cast<quint16>(i * 4);
            bool isLast = (i == VideoStreamReceiver::NtscPacketsPerFrame - 1);
            packets.append(createVideoPacket(startSeq + i, frameNum, lineNum, isLast));
        }
        return packets;
    }

private slots:
    // ========== Constructor and basic state ==========

    void testConstructor()
    {
        VideoStreamReceiver receiver;
        QVERIFY(!receiver.isActive());
        QCOMPARE(receiver.port(), static_cast<quint16>(0));
        QCOMPARE(receiver.videoFormat(), VideoStreamReceiver::VideoFormat::Unknown);
        QCOMPARE(receiver.currentFrameNumber(), static_cast<quint16>(0));
    }

    // ========== Constants ==========

    void testConstants()
    {
        QCOMPARE(VideoStreamReceiver::DefaultPort, static_cast<quint16>(21000));
        QCOMPARE(VideoStreamReceiver::PacketSize, 780);
        QCOMPARE(VideoStreamReceiver::HeaderSize, 12);
        QCOMPARE(VideoStreamReceiver::PayloadSize, 768);
        QCOMPARE(VideoStreamReceiver::PixelsPerLine, 384);
        QCOMPARE(VideoStreamReceiver::LinesPerPacket, 4);
        QCOMPARE(VideoStreamReceiver::BitsPerPixel, 4);
        QCOMPARE(VideoStreamReceiver::BytesPerLine, 192);
        QCOMPARE(VideoStreamReceiver::MaxFrameHeight, 272);
        QCOMPARE(VideoStreamReceiver::PalHeight, 272);
        QCOMPARE(VideoStreamReceiver::NtscHeight, 240);
        QCOMPARE(VideoStreamReceiver::PalPacketsPerFrame, 68);
        QCOMPARE(VideoStreamReceiver::NtscPacketsPerFrame, 60);
    }

    // ========== bind() and close() ==========

    void testBindSuccess()
    {
        VideoStreamReceiver receiver;
        // Use a high port that's likely to be free
        bool bound = receiver.bind(44444);
        QVERIFY(bound);
        QVERIFY(receiver.isActive());
        QCOMPARE(receiver.port(), static_cast<quint16>(44444));

        receiver.close();
        QVERIFY(!receiver.isActive());
    }

    void testBindMultipleTimesClosesPrevious()
    {
        VideoStreamReceiver receiver;
        QVERIFY(receiver.bind(44445));
        QCOMPARE(receiver.port(), static_cast<quint16>(44445));

        QVERIFY(receiver.bind(44446));
        QCOMPARE(receiver.port(), static_cast<quint16>(44446));

        receiver.close();
    }

    void testBindFailureEmitsError()
    {
        // First bind a socket to a port
        QUdpSocket blocker;
        QVERIFY(blocker.bind(44447));

        VideoStreamReceiver receiver;
        QSignalSpy errorSpy(&receiver, &VideoStreamReceiver::socketError);

        // This should fail since the port is already in use
        bool bound = receiver.bind(44447);
        QVERIFY(!bound);
        QVERIFY(errorSpy.count() >= 1);
        // Check that at least one error contains the port number
        bool foundPortInError = false;
        for (const auto &signal : errorSpy) {
            if (signal.first().toString().contains("44447")) {
                foundPortInError = true;
                break;
            }
        }
        QVERIFY(foundPortInError);

        blocker.close();
    }

    void testCloseWhenNotBound()
    {
        VideoStreamReceiver receiver;
        // Should not crash when closing without binding
        receiver.close();
        QVERIFY(!receiver.isActive());
    }

    // ========== Packet reception via UDP ==========

    void testReceiveSinglePacket()
    {
        VideoStreamReceiver receiver;
        QVERIFY(receiver.bind(44448));

        QUdpSocket sender;
        QByteArray packet = createVideoPacket(0, 1, 0);
        sender.writeDatagram(packet, QHostAddress::LocalHost, 44448);

        // Wait for packet to be received
        QTest::qWait(50);

        // Frame number should be updated
        QCOMPARE(receiver.currentFrameNumber(), static_cast<quint16>(1));

        receiver.close();
    }

    void testReceiveCompletePalFrame()
    {
        VideoStreamReceiver receiver;
        QVERIFY(receiver.bind(44449));

        QSignalSpy frameSpy(&receiver, &VideoStreamReceiver::frameReady);
        QSignalSpy formatSpy(&receiver, &VideoStreamReceiver::formatDetected);

        QUdpSocket sender;
        auto packets = createPalFrame(1, 0);

        for (const auto &packet : packets) {
            sender.writeDatagram(packet, QHostAddress::LocalHost, 44449);
        }

        // Wait for packets to be processed
        QTest::qWait(100);

        QCOMPARE(frameSpy.count(), 1);
        QCOMPARE(formatSpy.count(), 1);

        // Check format detection
        auto format = formatSpy.first().first().value<VideoStreamReceiver::VideoFormat>();
        QCOMPARE(format, VideoStreamReceiver::VideoFormat::PAL);

        // Check frame data
        auto args = frameSpy.first();
        QByteArray frameData = args.at(0).toByteArray();
        quint16 frameNum = args.at(1).value<quint16>();
        auto frameFormat = args.at(2).value<VideoStreamReceiver::VideoFormat>();

        QCOMPARE(frameNum, static_cast<quint16>(1));
        QCOMPARE(frameFormat, VideoStreamReceiver::VideoFormat::PAL);
        QCOMPARE(frameData.size(), VideoStreamReceiver::BytesPerLine * VideoStreamReceiver::PalHeight);

        receiver.close();
    }

    void testReceiveCompleteNtscFrame()
    {
        VideoStreamReceiver receiver;
        QVERIFY(receiver.bind(44450));

        QSignalSpy frameSpy(&receiver, &VideoStreamReceiver::frameReady);
        QSignalSpy formatSpy(&receiver, &VideoStreamReceiver::formatDetected);

        QUdpSocket sender;
        auto packets = createNtscFrame(1, 0);

        for (const auto &packet : packets) {
            sender.writeDatagram(packet, QHostAddress::LocalHost, 44450);
        }

        QTest::qWait(100);

        QCOMPARE(frameSpy.count(), 1);
        QCOMPARE(formatSpy.count(), 1);

        auto format = formatSpy.first().first().value<VideoStreamReceiver::VideoFormat>();
        QCOMPARE(format, VideoStreamReceiver::VideoFormat::NTSC);

        auto args = frameSpy.first();
        QByteArray frameData = args.at(0).toByteArray();
        QCOMPARE(frameData.size(), VideoStreamReceiver::BytesPerLine * VideoStreamReceiver::NtscHeight);

        receiver.close();
    }

    void testIgnoreMalformedPackets()
    {
        VideoStreamReceiver receiver;
        QVERIFY(receiver.bind(44451));

        QSignalSpy frameSpy(&receiver, &VideoStreamReceiver::frameReady);

        QUdpSocket sender;

        // Send a too-small packet
        QByteArray smallPacket(100, '\0');
        sender.writeDatagram(smallPacket, QHostAddress::LocalHost, 44451);

        // Send a too-large packet
        QByteArray largePacket(1000, '\0');
        sender.writeDatagram(largePacket, QHostAddress::LocalHost, 44451);

        QTest::qWait(50);

        // No frames should be ready
        QCOMPARE(frameSpy.count(), 0);

        receiver.close();
    }

    void testMultipleFrames()
    {
        VideoStreamReceiver receiver;
        QVERIFY(receiver.bind(44452));

        QSignalSpy frameSpy(&receiver, &VideoStreamReceiver::frameReady);

        QUdpSocket sender;

        // Send first frame
        auto frame1 = createPalFrame(1, 0);
        for (const auto &packet : frame1) {
            sender.writeDatagram(packet, QHostAddress::LocalHost, 44452);
        }

        // Send second frame
        auto frame2 = createPalFrame(2, 68);
        for (const auto &packet : frame2) {
            sender.writeDatagram(packet, QHostAddress::LocalHost, 44452);
        }

        QTest::qWait(150);

        QCOMPARE(frameSpy.count(), 2);

        // Check frame numbers
        QCOMPARE(frameSpy.at(0).at(1).value<quint16>(), static_cast<quint16>(1));
        QCOMPARE(frameSpy.at(1).at(1).value<quint16>(), static_cast<quint16>(2));

        receiver.close();
    }

    // ========== Statistics ==========

    void testStatsUpdated()
    {
        VideoStreamReceiver receiver;
        QVERIFY(receiver.bind(44453));

        QSignalSpy statsSpy(&receiver, &VideoStreamReceiver::statsUpdated);

        QUdpSocket sender;

        // Send 50 PAL frames to trigger stats update (every 50 frames)
        for (int f = 0; f < 50; f++) {
            auto frame = createPalFrame(static_cast<quint16>(f + 1), static_cast<quint16>(f * 68));
            for (const auto &packet : frame) {
                sender.writeDatagram(packet, QHostAddress::LocalHost, 44453);
            }
            // Small delay between frames
            QTest::qWait(2);
        }

        // Wait for processing
        QTest::qWait(200);

        // Stats should have been emitted at least once
        QVERIFY(statsSpy.count() >= 1);

        // Check stats values
        auto args = statsSpy.last();
        quint64 packetsReceived = args.at(0).toULongLong();
        quint64 framesCompleted = args.at(1).toULongLong();

        QVERIFY(packetsReceived > 0);
        QVERIFY(framesCompleted >= 50);

        receiver.close();
    }

    // ========== Edge cases ==========

    void testBindResetsState()
    {
        VideoStreamReceiver receiver;
        QVERIFY(receiver.bind(44454));

        QUdpSocket sender;
        auto frame = createPalFrame(100, 0);
        for (const auto &packet : frame) {
            sender.writeDatagram(packet, QHostAddress::LocalHost, 44454);
        }
        QTest::qWait(50);

        QCOMPARE(receiver.currentFrameNumber(), static_cast<quint16>(100));

        // Rebind should reset state
        QVERIFY(receiver.bind(44455));
        QCOMPARE(receiver.currentFrameNumber(), static_cast<quint16>(0));
        QCOMPARE(receiver.videoFormat(), VideoStreamReceiver::VideoFormat::Unknown);

        receiver.close();
    }

    void testDestructorClosesSocket()
    {
        quint16 testPort = 44456;

        // Create and bind receiver
        {
            VideoStreamReceiver receiver;
            QVERIFY(receiver.bind(testPort));
            // Receiver goes out of scope here
        }

        // Port should be available again
        QUdpSocket socket;
        QVERIFY(socket.bind(testPort));
        socket.close();
    }
};

QTEST_MAIN(TestVideoStreamReceiver)
#include "test_videostreamreceiver.moc"
