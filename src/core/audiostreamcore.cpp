/**
 * @file audiostreamcore.cpp
 * @brief Implementation of pure audio stream packet parsing and rate calculation functions.
 */

#include "audiostreamcore.h"

namespace audiostream {

std::optional<PacketParseResult> parsePacket(const QByteArray &packet)
{
    if (packet.size() != PacketSize) {
        return std::nullopt;
    }

    // Parse header: 2-byte sequence number, little-endian
    const auto *data = reinterpret_cast<const quint8 *>(packet.constData());
    auto sequenceNumber = static_cast<quint16>(data[0] | (data[1] << 8));

    PacketParseResult result;
    result.sequenceNumber = sequenceNumber;
    result.samples = packet.mid(HeaderSize, PayloadSize);
    return result;
}

SequenceAnalysis analyzeSequence(quint16 currentSeq, quint16 lastSeq)
{
    quint16 expectedSeq = static_cast<quint16>(lastSeq + 1);

    if (currentSeq == expectedSeq) {
        // Normal consecutive packet
        return SequenceAnalysis{false, 0};
    }

    // Check for valid wraparound (0xFFFF → 0)
    if (lastSeq == 0xFFFF && currentSeq == 0) {
        return SequenceAnalysis{false, 0};
    }

    // Compute the forward gap
    quint16 gap = static_cast<quint16>(currentSeq - expectedSeq);

    if (gap < 1000) {
        // Reasonable gap — report as discontinuity
        return SequenceAnalysis{true, gap};
    }

    // Large gap: likely a counter reset; do not report as discontinuity
    return SequenceAnalysis{false, 0};
}

int calculateFlushIntervalUs(double sampleRate, int samplesPerPacket)
{
    return static_cast<int>((samplesPerPacket / sampleRate) * 1000000.0);
}

double sampleRateForFormat(bool isNtsc)
{
    return isNtsc ? NtscSampleRate : PalSampleRate;
}

}  // namespace audiostream
