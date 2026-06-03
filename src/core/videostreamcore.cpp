/**
 * @file videostreamcore.cpp
 * @brief Implementation of pure video stream packet parsing and format detection functions.
 */

#include "videostreamcore.h"

namespace videostream {

PacketHeader parseHeader(const QByteArray &packet)
{
    PacketHeader header{};
    const auto *data = reinterpret_cast<const quint8 *>(packet.constData());

    // All values are little-endian
    header.sequenceNumber = static_cast<quint16>(data[0] | (data[1] << 8));
    header.frameNumber = static_cast<quint16>(data[2] | (data[3] << 8));
    header.lineNumber = static_cast<quint16>(data[4] | (data[5] << 8));
    header.pixelsPerLine = static_cast<quint16>(data[6] | (data[7] << 8));
    header.linesPerPacket = data[8];
    header.bitsPerPixel = data[9];
    header.encodingType = static_cast<quint16>(data[10] | (data[11] << 8));

    return header;
}

SequenceAnalysis analyzeSequence(quint16 currentSeq, quint16 lastSeq)
{
    quint16 expectedSeq = static_cast<quint16>(lastSeq + 1);

    if (currentSeq == expectedSeq) {
        // Normal sequential packet
        return SequenceAnalysis{false, false, 0};
    }

    // Valid wraparound: 0xFFFF → 0
    if (lastSeq == 0xFFFF && currentSeq == 0) {
        return SequenceAnalysis{false, false, 0};
    }

    quint16 gap = static_cast<quint16>(currentSeq - expectedSeq);

    if (gap < 1000) {
        // Reasonable forward gap: packet loss
        return SequenceAnalysis{true, false, gap};
    }

    if (gap > 0xF000) {
        // Sequence went backwards: out-of-order packet
        return SequenceAnalysis{false, true, 0};
    }

    // Large gap but not backwards: likely a counter reset, not reported
    return SequenceAnalysis{false, false, 0};
}

int computeFrameLines(const PacketHeader &header)
{
    return header.actualLineNumber() + header.linesPerPacket;
}

}  // namespace videostream
