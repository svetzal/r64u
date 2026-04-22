#ifndef AVICORE_H
#define AVICORE_H

#include <QByteArray>
#include <QList>

namespace avi {

/// Audio stream constants matching the device's PCM audio output.
constexpr int AudioSampleRate = 48000;
constexpr int AudioChannels = 2;
constexpr int AudioBitsPerSample = 16;

// ---------------------------------------------------------------------------
// Low-level helpers
// ---------------------------------------------------------------------------

/**
 * @brief Encodes a 32-bit unsigned value as 4 little-endian bytes.
 * @param value The value to encode.
 * @return 4-byte QByteArray in LE byte order.
 */
[[nodiscard]] QByteArray writeLittleEndian32(quint32 value);

/**
 * @brief Builds a RIFF chunk from a fourCC tag and data payload.
 *
 * Wire format: [fourCC:4] [size_le:4] [data:size]
 *
 * @param fourCC 4-character chunk identifier (padded/truncated to 4 bytes).
 * @param data   Chunk payload.
 * @return Complete chunk bytes.
 */
[[nodiscard]] QByteArray buildChunk(const QByteArray &fourCC, const QByteArray &data);

/**
 * @brief Pads data to an even number of bytes (AVI alignment requirement).
 * @param data Input data.
 * @return Data unchanged if already even; data + '\0' if odd.
 */
[[nodiscard]] QByteArray padToEven(const QByteArray &data);

// ---------------------------------------------------------------------------
// Frame rate
// ---------------------------------------------------------------------------

/**
 * @brief Calculates the frame rate from the first/last frame timestamps.
 *
 * Returns 30.0 when frameCount ≤ 1.  Clamps to [1.0, 60.0].
 *
 * @param frameCount  Total number of recorded frames.
 * @param durationMs  Time between first and last frame in milliseconds.
 * @return Frames per second, clamped to [1.0, 60.0].
 */
[[nodiscard]] double calculateFps(int frameCount, qint64 durationMs);

/**
 * @brief Converts a frame rate to the AVI dwMicroSecPerFrame field.
 * @param fps Frames per second.
 * @return Microseconds per frame (≥ 1).
 */
[[nodiscard]] quint32 calculateMicroSecPerFrame(double fps);

// ---------------------------------------------------------------------------
// Index
// ---------------------------------------------------------------------------

/**
 * @brief Describes one data chunk written into the movi LIST.
 */
struct ChunkInfo
{
    QByteArray fourCC;  ///< "00dc" for video keyframe, "01wb" for audio
    qint64 offset;      ///< Byte offset relative to movi LIST start
    int size;           ///< Chunk data size (before padding)
};

/**
 * @brief Builds the idx1 index payload from a list of chunk descriptors.
 *
 * Video chunks ("00dc") are flagged as keyframes (0x10).
 * Audio chunks ("01wb") use flag 0x00.
 *
 * @param chunks All video and audio chunks in recording order.
 * @return idx1 payload bytes (to be wrapped with buildChunk("idx1", ...)).
 */
[[nodiscard]] QByteArray buildIdx1(const QList<ChunkInfo> &chunks);

// ---------------------------------------------------------------------------
// Headers
// ---------------------------------------------------------------------------

/**
 * @brief Parameters needed to finalise an AVI file header.
 */
struct StreamParams
{
    int width = 384;                              ///< Video frame width in pixels
    int height = 272;                             ///< Video frame height in pixels
    int frameCount = 0;                           ///< Total video frames recorded
    double fps = 30.0;                            ///< Frames per second
    int audioSampleCount = 0;                     ///< Total audio samples recorded
    int audioSampleRate = AudioSampleRate;        ///< Audio sample rate
    int audioChannels = AudioChannels;            ///< Number of audio channels
    int audioBitsPerSample = AudioBitsPerSample;  ///< Bits per audio sample
};

/**
 * @brief Builds the complete placeholder AVI header written at recording start.
 *
 * All variable fields (frame count, fps, dimensions) are written with
 * placeholder values that are later overwritten by buildFinalizedHeader().
 *
 * @return Full header bytes (RIFF + AVI + hdrl + video strl + audio strl +
 *         movi LIST header).  The caller should record @c moviListStart as
 *         the file position immediately after the returned bytes.
 */
[[nodiscard]] QByteArray buildInitialHeader();

/**
 * @brief Builds the finalised AVI header with actual recording values.
 *
 * This replaces the placeholder values written by buildInitialHeader().
 * The returned bytes must be written at file offset 0.
 *
 * @param params Actual recording statistics.
 * @return Bytes identical in length to buildInitialHeader() but with real values.
 */
[[nodiscard]] QByteArray buildFinalizedHeader(const StreamParams &params);

}  // namespace avi

#endif  // AVICORE_H
