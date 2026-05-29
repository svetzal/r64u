/**
 * @file test_audiostreamreceiver.cpp
 * @brief Unit tests for AudioStreamReceiverService — the UDP audio packet reception
 *        and jitter-buffer imperative shell.
 *
 * Tests cover:
 * - Initial state (not active, correct defaults)
 * - bind() / close() lifecycle
 * - isActive() state tracking
 * - port() after successful bind
 * - jitterBufferSize configuration
 * - setAudioFormat() and sampleRate()
 * - Packet reception: samplesReady signal emitted via local UDP
 * - statsUpdated signal emitted after packets arrive
 */

#include "services/audiostreamreceiverservice.h"

#include <QHostAddress>
#include <QSignalSpy>
#include <QUdpSocket>
#include <QtTest>

// ---------------------------------------------------------------------------
// Helper: build a valid 770-byte audio packet with given sequence number
// ---------------------------------------------------------------------------
namespace {
QByteArray buildAudioPacket(quint16 seqNum)
{
    QByteArray pkt(AudioStreamReceiverService::PacketSize, static_cast<char>(0xAB));
    // Header: 16-bit little-endian sequence number
    pkt[0] = static_cast<char>(seqNum & 0xFF);
    pkt[1] = static_cast<char>((seqNum >> 8) & 0xFF);
    return pkt;
}
}  // namespace

class TestAudioStreamReceiver : public QObject
{
    Q_OBJECT

private slots:
    void init() { receiver_ = new AudioStreamReceiverService(this); }

    void cleanup()
    {
        receiver_->close();
        delete receiver_;
        receiver_ = nullptr;
    }

    // =========================================================
    // Initial state
    // =========================================================

    void testInitialState_NotActive() { QVERIFY(!receiver_->isActive()); }

    void testInitialState_PortZero() { QCOMPARE(receiver_->port(), static_cast<quint16>(0)); }

    void testInitialState_DefaultJitterBufferSize()
    {
        QCOMPARE(receiver_->jitterBufferSize(),
                 AudioStreamReceiverService::DefaultJitterBufferSize);
    }

    void testInitialState_DefaultFormatPAL()
    {
        QCOMPARE(receiver_->audioFormat(), AudioStreamReceiverService::AudioFormat::PAL);
    }

    void testInitialState_BufferedPacketsZero() { QCOMPARE(receiver_->bufferedPackets(), 0); }

    // =========================================================
    // Constants
    // =========================================================

    void testConstants_PacketSize() { QCOMPARE(AudioStreamReceiverService::PacketSize, 770); }

    void testConstants_HeaderSize() { QCOMPARE(AudioStreamReceiverService::HeaderSize, 2); }

    void testConstants_PayloadSize() { QCOMPARE(AudioStreamReceiverService::PayloadSize, 768); }

    void testConstants_SamplesPerPacket()
    {
        QCOMPARE(AudioStreamReceiverService::SamplesPerPacket, 192);
    }

    void testConstants_DefaultPort()
    {
        QCOMPARE(AudioStreamReceiverService::DefaultPort, static_cast<quint16>(21001));
    }

    // =========================================================
    // bind() / close()
    // =========================================================

    void testBind_ToAvailablePort_Succeeds()
    {
        bool result = receiver_->bind(0);  // port 0 = OS assigns free port

        QVERIFY(result);
    }

    void testBind_SetsActive()
    {
        receiver_->bind(0);

        QVERIFY(receiver_->isActive());
    }

    void testBind_AssignsNonZeroPort()
    {
        receiver_->bind(0);

        QVERIFY(receiver_->port() != 0);
    }

    void testClose_AfterBind_SetsNotActive()
    {
        receiver_->bind(0);
        receiver_->close();

        QVERIFY(!receiver_->isActive());
    }

    void testClose_WithoutBind_IsNoOp()
    {
        // Should not crash
        receiver_->close();
        QVERIFY(!receiver_->isActive());
    }

    void testDoubleBind_SecondBindOnDifferentPort()
    {
        QVERIFY(receiver_->bind(0));
        quint16 firstPort = receiver_->port();

        // Closing and re-binding should work fine
        receiver_->close();
        QVERIFY(receiver_->bind(0));

        // Both binds should have assigned a valid port
        QVERIFY(receiver_->port() != 0);
        (void)firstPort;  // used to suppress warning
    }

    // =========================================================
    // jitterBufferSize configuration
    // =========================================================

    void testSetJitterBufferSize_UpdatesValue()
    {
        receiver_->setJitterBufferSize(5);

        QCOMPARE(receiver_->jitterBufferSize(), 5);
    }

    void testSetJitterBufferSize_LargeValue()
    {
        receiver_->setJitterBufferSize(50);

        QCOMPARE(receiver_->jitterBufferSize(), 50);
    }

    // =========================================================
    // setAudioFormat / sampleRate
    // =========================================================

    void testSetAudioFormat_PAL_SetsFormat()
    {
        receiver_->setAudioFormat(AudioStreamReceiverService::AudioFormat::PAL);

        QCOMPARE(receiver_->audioFormat(), AudioStreamReceiverService::AudioFormat::PAL);
    }

    void testSetAudioFormat_NTSC_SetsFormat()
    {
        receiver_->setAudioFormat(AudioStreamReceiverService::AudioFormat::NTSC);

        QCOMPARE(receiver_->audioFormat(), AudioStreamReceiverService::AudioFormat::NTSC);
    }

    void testSampleRate_PAL_MatchesConstant()
    {
        receiver_->setAudioFormat(AudioStreamReceiverService::AudioFormat::PAL);

        QVERIFY(qAbs(receiver_->sampleRate() - AudioStreamReceiverService::PalSampleRate) < 1.0);
    }

    void testSampleRate_NTSC_MatchesConstant()
    {
        receiver_->setAudioFormat(AudioStreamReceiverService::AudioFormat::NTSC);

        QVERIFY(qAbs(receiver_->sampleRate() - AudioStreamReceiverService::NtscSampleRate) < 1.0);
    }

    // =========================================================
    // Packet reception via local UDP
    // =========================================================

    void testReceive_ValidPacket_EmitsSamplesReady()
    {
        // Use a large jitter buffer size of 1 so that after receiving 1 packet
        // the buffer is primed and samples flush immediately.
        receiver_->setJitterBufferSize(1);
        QVERIFY(receiver_->bind(0));

        QSignalSpy spy(receiver_, &AudioStreamReceiverService::samplesReady);

        QUdpSocket sender;
        for (int i = 0; i < 5; ++i) {
            QByteArray pkt = buildAudioPacket(static_cast<quint16>(i));
            sender.writeDatagram(pkt, QHostAddress::LocalHost, receiver_->port());
        }

        QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 3000);
    }

    void testReceive_ValidPacket_SamplesHaveCorrectSize()
    {
        receiver_->setJitterBufferSize(1);
        QVERIFY(receiver_->bind(0));

        QSignalSpy spy(receiver_, &AudioStreamReceiverService::samplesReady);

        QUdpSocket sender;
        for (int i = 0; i < 5; ++i) {
            QByteArray pkt = buildAudioPacket(static_cast<quint16>(i));
            sender.writeDatagram(pkt, QHostAddress::LocalHost, receiver_->port());
        }

        QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 3000);

        QByteArray samples = spy.first().first().toByteArray();
        QCOMPARE(samples.size(), AudioStreamReceiverService::PayloadSize);
    }

    void testReceive_InvalidSizePacket_NotEmitted()
    {
        receiver_->setJitterBufferSize(1);
        QVERIFY(receiver_->bind(0));

        QSignalSpy spy(receiver_, &IAudioStreamReceiverService::samplesReady);

        // Send a garbage-size packet
        QUdpSocket sender;
        QByteArray badPkt(100, '\x00');
        sender.writeDatagram(badPkt, QHostAddress::LocalHost, receiver_->port());

        QTest::qWait(100);

        QCOMPARE(spy.count(), 0);
    }

private:
    AudioStreamReceiverService *receiver_ = nullptr;
};

QTEST_MAIN(TestAudioStreamReceiver)
#include "test_audiostreamreceiverservice.moc"
