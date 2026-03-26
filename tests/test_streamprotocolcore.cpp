/**
 * @file test_streamprotocolcore.cpp
 * @brief Unit tests for pure stream control protocol serialisation functions.
 *
 * Tests the streamprotocol:: namespace functions which are pure (no I/O, no sockets).
 * Exercises every bit of the binary wire format to prevent regressions in the
 * byte-level encoding.
 */

#include <QtTest>

#include <services/streamprotocolcore.h>

class TestStreamProtocolCore : public QObject
{
    Q_OBJECT

private slots:
    // =========================================================
    // buildStartCommand — command byte
    // =========================================================

    void buildStartCommand_video_correctCommandByte()
    {
        auto cmd = streamprotocol::buildStartCommand(streamprotocol::CommandType::StartVideo,
                                                     "192.168.1.1", 21000, 0);

        QCOMPARE(static_cast<quint8>(cmd[0]), static_cast<quint8>(0x20));
    }

    void buildStartCommand_audio_correctCommandByte()
    {
        auto cmd = streamprotocol::buildStartCommand(streamprotocol::CommandType::StartAudio,
                                                     "192.168.1.1", 21001, 0);

        QCOMPARE(static_cast<quint8>(cmd[0]), static_cast<quint8>(0x21));
    }

    // =========================================================
    // buildStartCommand — marker byte
    // =========================================================

    void buildStartCommand_hasMarkerByte()
    {
        auto cmd = streamprotocol::buildStartCommand(streamprotocol::CommandType::StartVideo,
                                                     "192.168.1.1", 21000, 0);

        QCOMPARE(static_cast<quint8>(cmd[1]), static_cast<quint8>(0xFF));
    }

    // =========================================================
    // buildStartCommand — parameter length
    // =========================================================

    void buildStartCommand_paramLengthIncludesDurationAndDest()
    {
        // dest = "10.0.0.1:21000" → 14 bytes; paramLen = 2 + 14 = 16
        auto cmd = streamprotocol::buildStartCommand(streamprotocol::CommandType::StartVideo,
                                                     "10.0.0.1", 21000, 0);

        QString dest = "10.0.0.1:21000";
        quint16 expected = static_cast<quint16>(2 + dest.size());
        quint16 actual =
            static_cast<quint16>(static_cast<quint8>(cmd[2]) | (static_cast<quint8>(cmd[3]) << 8));

        QCOMPARE(actual, expected);
    }

    // =========================================================
    // buildStartCommand — duration encoding
    // =========================================================

    void buildStartCommand_zeroDuration_bothBytesZero()
    {
        auto cmd = streamprotocol::buildStartCommand(streamprotocol::CommandType::StartVideo,
                                                     "1.2.3.4", 100, 0);

        QCOMPARE(static_cast<quint8>(cmd[4]), static_cast<quint8>(0x00));
        QCOMPARE(static_cast<quint8>(cmd[5]), static_cast<quint8>(0x00));
    }

    void buildStartCommand_durationIsLittleEndian()
    {
        // 0x0201 → lo = 0x01, hi = 0x02
        auto cmd = streamprotocol::buildStartCommand(streamprotocol::CommandType::StartAudio,
                                                     "1.2.3.4", 100, static_cast<quint16>(0x0201));

        QCOMPARE(static_cast<quint8>(cmd[4]), static_cast<quint8>(0x01));
        QCOMPARE(static_cast<quint8>(cmd[5]), static_cast<quint8>(0x02));
    }

    // =========================================================
    // buildStartCommand — destination string
    // =========================================================

    void buildStartCommand_containsDestinationString()
    {
        auto cmd = streamprotocol::buildStartCommand(streamprotocol::CommandType::StartVideo,
                                                     "192.168.1.64", 21000, 0);

        QByteArray payload = cmd.mid(6);  // after 4-byte header + 2-byte duration
        QCOMPARE(payload, QByteArray("192.168.1.64:21000"));
    }

    void buildStartCommand_totalSize()
    {
        // 4 header bytes + 2 duration bytes + strlen("192.168.1.64:21000") = 6 + 18 = 24
        auto cmd = streamprotocol::buildStartCommand(streamprotocol::CommandType::StartVideo,
                                                     "192.168.1.64", 21000, 0);

        QCOMPARE(cmd.size(), 6 + static_cast<int>(QByteArray("192.168.1.64:21000").size()));
    }

    void buildStartCommand_highPort()
    {
        auto cmd = streamprotocol::buildStartCommand(streamprotocol::CommandType::StartVideo,
                                                     "1.2.3.4", 65535, 0);

        QByteArray payload = cmd.mid(6);
        QCOMPARE(payload, QByteArray("1.2.3.4:65535"));
    }

    // =========================================================
    // buildStopCommand — command byte
    // =========================================================

    void buildStopCommand_video_correctCommandByte()
    {
        auto cmd = streamprotocol::buildStopCommand(streamprotocol::CommandType::StopVideo);

        QCOMPARE(static_cast<quint8>(cmd[0]), static_cast<quint8>(0x30));
    }

    void buildStopCommand_audio_correctCommandByte()
    {
        auto cmd = streamprotocol::buildStopCommand(streamprotocol::CommandType::StopAudio);

        QCOMPARE(static_cast<quint8>(cmd[0]), static_cast<quint8>(0x31));
    }

    // =========================================================
    // buildStopCommand — structure
    // =========================================================

    void buildStopCommand_hasMarkerByte()
    {
        auto cmd = streamprotocol::buildStopCommand(streamprotocol::CommandType::StopVideo);

        QCOMPARE(static_cast<quint8>(cmd[1]), static_cast<quint8>(0xFF));
    }

    void buildStopCommand_zeroParamLength()
    {
        auto cmd = streamprotocol::buildStopCommand(streamprotocol::CommandType::StopAudio);

        QCOMPARE(static_cast<quint8>(cmd[2]), static_cast<quint8>(0x00));
        QCOMPARE(static_cast<quint8>(cmd[3]), static_cast<quint8>(0x00));
    }

    void buildStopCommand_exactlyFourBytes()
    {
        auto cmd = streamprotocol::buildStopCommand(streamprotocol::CommandType::StopVideo);

        QCOMPARE(cmd.size(), 4);
    }

    // =========================================================
    // commandTypeToString
    // =========================================================

    void commandTypeToString_allTypes()
    {
        QCOMPARE(streamprotocol::commandTypeToString(streamprotocol::CommandType::StartVideo),
                 QString("StartVideo"));
        QCOMPARE(streamprotocol::commandTypeToString(streamprotocol::CommandType::StartAudio),
                 QString("StartAudio"));
        QCOMPARE(streamprotocol::commandTypeToString(streamprotocol::CommandType::StopVideo),
                 QString("StopVideo"));
        QCOMPARE(streamprotocol::commandTypeToString(streamprotocol::CommandType::StopAudio),
                 QString("StopAudio"));
    }
};

QTEST_MAIN(TestStreamProtocolCore)
#include "test_streamprotocolcore.moc"
