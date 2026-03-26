/**
 * @file audiostreamcore.h
 * @brief Pure core functions for audio stream packet parsing and rate calculation.
 *
 * All functions in this namespace are pure: they take immutable input and return
 * new output with no side effects. This enables comprehensive unit testing and
 * clean separation from I/O concerns (QUdpSocket, jitter buffer, timers).
 *
 * This follows the same pattern as playlistcore.h / favoritescore.h.
 */

#ifndef AUDIOSTREAMCORE_H
#define AUDIOSTREAMCORE_H

#include <QByteArray>

#include <optional>

namespace audiostream {

/// Audio packet size in bytes (2-byte header + 768-byte payload)
constexpr int PacketSize = 770;

/// Header size in bytes (16-bit little-endian sequence number)
constexpr int HeaderSize = 2;

/// Payload size in bytes (192 stereo samples × 4 bytes)
constexpr int PayloadSize = 768;

/// Number of stereo samples per packet
constexpr int SamplesPerPacket = 192;

/// Bytes per stereo sample (L16 + R16)
constexpr int BytesPerSample = 4;

/// PAL C64 sample rate (Hz)
constexpr double PalSampleRate = 47982.8869047619;

/// NTSC C64 sample rate (Hz)
constexpr double NtscSampleRate = 47940.3408482143;

// ---------------------------------------------------------------------------
// Packet parsing
// ---------------------------------------------------------------------------

/**
 * @brief Result of parsing a valid audio packet.
 */
struct PacketParseResult
{
    quint16 sequenceNumber;  ///< 16-bit little-endian sequence number from header
    QByteArray samples;      ///< Raw audio payload (PayloadSize bytes)
};

/**
 * @brief Parses a raw UDP audio packet into its header and payload.
 *
 * @param packet Raw bytes received from the UDP socket.
 * @return PacketParseResult on success, or std::nullopt if the packet size
 *         is not exactly PacketSize (770) bytes.
 */
[[nodiscard]] std::optional<PacketParseResult> parsePacket(const QByteArray &packet);

// ---------------------------------------------------------------------------
// Sequence analysis
// ---------------------------------------------------------------------------

/**
 * @brief Result of analysing the gap between two sequence numbers.
 */
struct SequenceAnalysis
{
    bool isDiscontinuity = false;  ///< True when a meaningful gap was detected
    quint16 gap = 0;               ///< Number of missing packets (0 for sequential / wraparound)
};

/**
 * @brief Determines whether consecutive sequence numbers represent a discontinuity.
 *
 * The function handles normal wraparound (0xFFFF → 0) as non-discontinuous.
 * A gap > 1000 is treated as a counter reset and is NOT reported as a discontinuity.
 *
 * @param currentSeq  The sequence number of the packet just received.
 * @param lastSeq     The sequence number of the previously received packet.
 * @return SequenceAnalysis with isDiscontinuity=true when gap is 1–1000.
 */
[[nodiscard]] SequenceAnalysis analyzeSequence(quint16 currentSeq, quint16 lastSeq);

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------

/**
 * @brief Calculates the flush interval in microseconds for the jitter buffer timer.
 *
 * Formula: (samplesPerPacket / sampleRate) × 1,000,000
 *
 * @param sampleRate      Sample rate in Hz (use PalSampleRate or NtscSampleRate).
 * @param samplesPerPacket Number of samples per packet (default: SamplesPerPacket).
 * @return Interval in microseconds (always ≥ 1).
 */
[[nodiscard]] int calculateFlushIntervalUs(double sampleRate,
                                           int samplesPerPacket = SamplesPerPacket);

/**
 * @brief Returns the sample rate for a given video standard.
 * @param isNtsc True for NTSC, false for PAL.
 * @return NtscSampleRate if @p isNtsc is true, otherwise PalSampleRate.
 */
[[nodiscard]] double sampleRateForFormat(bool isNtsc);

}  // namespace audiostream

#endif  // AUDIOSTREAMCORE_H
