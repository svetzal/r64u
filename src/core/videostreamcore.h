#ifndef VIDEOSTREAMCORE_H
#define VIDEOSTREAMCORE_H

#include <QByteArray>

namespace videostream {

// ---------------------------------------------------------------------------
// Protocol constants
// ---------------------------------------------------------------------------

/// Video packet size in bytes (12-byte header + 768-byte payload)
constexpr int PacketSize = 780;

/// Header size in bytes
constexpr int HeaderSize = 12;

/// Payload size in bytes (4 lines × 192 bytes per line)
constexpr int PayloadSize = 768;

/// Pixels per line in the video stream
constexpr int PixelsPerLine = 384;

/// Lines per packet
constexpr int LinesPerPacket = 4;

/// Bits per pixel (VIC-II uses 4-bit color indices)
constexpr int BitsPerPixel = 4;

/// Bytes per line (384 pixels × 4 bits / 8)
constexpr int BytesPerLine = 192;

/// Maximum frame height (PAL)
constexpr int MaxFrameHeight = 272;

/// PAL frame height in lines
constexpr int PalHeight = 272;

/// NTSC frame height in lines
constexpr int NtscHeight = 240;

/// PAL packets per frame (272 lines / 4 lines per packet)
constexpr int PalPacketsPerFrame = 68;

/// NTSC packets per frame (240 lines / 4 lines per packet)
constexpr int NtscPacketsPerFrame = 60;

// ---------------------------------------------------------------------------
// Packet header
// ---------------------------------------------------------------------------

/**
 * @brief Parsed representation of a video stream packet header.
 */
struct PacketHeader
{
    quint16 sequenceNumber;  ///< Packet sequence number (wraps at 0xFFFF)
    quint16 frameNumber;     ///< Frame number this packet belongs to
    quint16 lineNumber;      ///< Bits 0-14: line index, Bit 15: last-packet flag
    quint16 pixelsPerLine;   ///< Pixels per video line
    quint8 linesPerPacket;   ///< Number of lines in this packet
    quint8 bitsPerPixel;     ///< Colour depth (4 for VIC-II palette indices)
    quint16 encodingType;    ///< Encoding / compression type

    /// @return true when bit 15 of lineNumber is set (last packet in frame).
    [[nodiscard]] bool isLastPacket() const { return (lineNumber & 0x8000) != 0; }

    /// @return The actual line index (bits 0-14 of lineNumber).
    [[nodiscard]] quint16 actualLineNumber() const { return lineNumber & 0x7FFF; }
};

// ---------------------------------------------------------------------------
// Packet parsing
// ---------------------------------------------------------------------------

/**
 * @brief Parses the 12-byte header from a raw video UDP packet.
 *
 * @param packet Raw bytes received from the UDP socket. Behaviour is undefined
 *               if the packet is shorter than HeaderSize bytes.
 * @return Populated PacketHeader with all fields decoded from little-endian bytes.
 */
[[nodiscard]] PacketHeader parseHeader(const QByteArray &packet);

// ---------------------------------------------------------------------------
// Sequence analysis
// ---------------------------------------------------------------------------

/**
 * @brief Result of analysing the gap between two video packet sequence numbers.
 */
struct SequenceAnalysis
{
    bool isLoss = false;        ///< True when packets were dropped (gap 1–1000)
    bool isOutOfOrder = false;  ///< True when sequence went backwards (gap > 0xF000)
    quint16 gap = 0;            ///< Number of missing packets (0 for sequential or wraparound)
};

/**
 * @brief Determines whether consecutive sequence numbers represent a loss or out-of-order packet.
 *
 * Handles normal wraparound (0xFFFF → 0) as non-discontinuous.
 * A gap between 1 and 1000 is reported as packet loss.
 * A gap greater than 0xF000 is reported as out-of-order (sequence went backwards).
 * All other gaps are treated as counter resets and not reported.
 *
 * @param currentSeq  The sequence number of the packet just received.
 * @param lastSeq     The sequence number of the previously received packet.
 * @return SequenceAnalysis describing the type and magnitude of the gap.
 */
[[nodiscard]] SequenceAnalysis analyzeSequence(quint16 currentSeq, quint16 lastSeq);

// ---------------------------------------------------------------------------
// Format detection
// ---------------------------------------------------------------------------

/**
 * @brief Computes the total number of video lines from the last packet in a frame.
 *
 * Caller maps the result to a VideoFormat:
 *   - PalHeight (272)  → PAL
 *   - NtscHeight (240) → NTSC
 *   - anything else   → Unknown
 *
 * @param header The header of the last packet in the frame (isLastPacket() should be true).
 * @return Total number of lines covered by this packet.
 */
[[nodiscard]] int computeFrameLines(const PacketHeader &header);

}  // namespace videostream

#endif  // VIDEOSTREAMCORE_H
