/**
 * @file avicore.cpp
 * @brief Implementation of pure AVI RIFF container data structure functions.
 */

#include "avicore.h"

#include <QDataStream>
#include <QIODevice>

#include <algorithm>
#include <cmath>

namespace avi {

QByteArray writeLittleEndian32(quint32 value)
{
    char bytes[4];
    bytes[0] = static_cast<char>(value & 0xFF);
    bytes[1] = static_cast<char>((value >> 8) & 0xFF);
    bytes[2] = static_cast<char>((value >> 16) & 0xFF);
    bytes[3] = static_cast<char>((value >> 24) & 0xFF);
    return QByteArray(bytes, 4);
}

QByteArray buildChunk(const QByteArray &fourCC, const QByteArray &data)
{
    QByteArray chunk;
    chunk.append(fourCC.leftJustified(4, '\0', true));
    chunk.append(writeLittleEndian32(static_cast<quint32>(data.size())));
    chunk.append(data);
    return chunk;
}

QByteArray padToEven(const QByteArray &data)
{
    if (data.size() % 2 != 0) {
        QByteArray padded = data;
        padded.append('\0');
        return padded;
    }
    return data;
}

double calculateFps(int frameCount, qint64 durationMs)
{
    if (frameCount <= 1) {
        return 30.0;
    }
    qint64 safeDuration = (durationMs <= 0) ? 1 : durationMs;
    double fps = (frameCount - 1) * 1000.0 / safeDuration;
    return std::max(1.0, std::min(60.0, fps));
}

quint32 calculateMicroSecPerFrame(double fps)
{
    return static_cast<quint32>(1000000.0 / fps);
}

QByteArray buildIdx1(const QList<ChunkInfo> &chunks)
{
    QByteArray idx1Data;
    QDataStream idxStream(&idx1Data, QIODevice::WriteOnly);
    idxStream.setByteOrder(QDataStream::LittleEndian);

    for (const auto &chunk : chunks) {
        idxStream.writeRawData(chunk.fourCC.leftJustified(4, '\0', true).constData(), 4);
        // Video frames are keyframes (0x10), audio chunks are not (0x00)
        quint32 flags = (chunk.fourCC == "00dc") ? quint32(0x10) : quint32(0x00);
        idxStream << flags;
        idxStream << quint32(chunk.offset);
        idxStream << quint32(chunk.size);
    }

    return idx1Data;
}

// ---------------------------------------------------------------------------
// Header building helpers (shared between initial and finalised headers)
// ---------------------------------------------------------------------------

namespace {

QByteArray buildAvihChunk(quint32 microSecPerFrame, quint32 totalFrames, quint32 width,
                          quint32 height)
{
    QByteArray avih;
    QDataStream ds(&avih, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);

    ds << microSecPerFrame;  // dwMicroSecPerFrame
    ds << quint32(0);        // dwMaxBytesPerSec
    ds << quint32(0);        // dwPaddingGranularity
    ds << quint32(0x110);    // dwFlags (AVIF_HASINDEX | AVIF_ISINTERLEAVED)
    ds << totalFrames;       // dwTotalFrames
    ds << quint32(0);        // dwInitialFrames
    ds << quint32(2);        // dwStreams (video + audio)
    ds << quint32(1000000);  // dwSuggestedBufferSize
    ds << width;             // dwWidth
    ds << height;            // dwHeight
    ds << quint32(0);        // dwReserved[4]
    ds << quint32(0);
    ds << quint32(0);
    ds << quint32(0);

    return buildChunk("avih", avih);
}

QByteArray buildVideoStrlList(quint32 fpsRate, quint32 frameCount, quint32 width, quint32 height)
{
    // strh (stream header)
    QByteArray strh;
    QDataStream ds2(&strh, QIODevice::WriteOnly);
    ds2.setByteOrder(QDataStream::LittleEndian);

    ds2.writeRawData("vids", 4);          // fccType
    ds2.writeRawData("MJPG", 4);          // fccHandler
    ds2 << quint32(0);                    // dwFlags
    ds2 << quint16(0);                    // wPriority
    ds2 << quint16(0);                    // wLanguage
    ds2 << quint32(0);                    // dwInitialFrames
    ds2 << quint32(1);                    // dwScale
    ds2 << fpsRate;                       // dwRate
    ds2 << quint32(0);                    // dwStart
    ds2 << frameCount;                    // dwLength
    ds2 << quint32(1000000);              // dwSuggestedBufferSize
    ds2 << quint32(0);                    // dwQuality
    ds2 << quint32(0);                    // dwSampleSize
    ds2 << quint16(0);                    // rcFrame (left)
    ds2 << quint16(0);                    // rcFrame (top)
    ds2 << static_cast<quint16>(width);   // rcFrame (right)
    ds2 << static_cast<quint16>(height);  // rcFrame (bottom)

    // strf (BITMAPINFOHEADER)
    QByteArray strf;
    QDataStream ds3(&strf, QIODevice::WriteOnly);
    ds3.setByteOrder(QDataStream::LittleEndian);

    ds3 << quint32(40);                  // biSize
    ds3 << static_cast<qint32>(width);   // biWidth
    ds3 << static_cast<qint32>(height);  // biHeight
    ds3 << quint16(1);                   // biPlanes
    ds3 << quint16(24);                  // biBitCount
    ds3.writeRawData("MJPG", 4);         // biCompression
    ds3 << quint32(0);                   // biSizeImage
    ds3 << qint32(0);                    // biXPelsPerMeter
    ds3 << qint32(0);                    // biYPelsPerMeter
    ds3 << quint32(0);                   // biClrUsed
    ds3 << quint32(0);                   // biClrImportant

    QByteArray strlPayload;
    strlPayload.append("strl");
    strlPayload.append(buildChunk("strh", strh));
    strlPayload.append(buildChunk("strf", strf));

    QByteArray list;
    list.append("LIST");
    list.append(writeLittleEndian32(static_cast<quint32>(strlPayload.size())));
    list.append(strlPayload);
    return list;
}

QByteArray buildAudioStrlList(quint32 audioSampleCount, int sampleRate, int channels,
                              int bitsPerSample)
{
    int blockAlign = channels * (bitsPerSample / 8);
    int avgBytesPerSec = sampleRate * blockAlign;

    // strh
    QByteArray audioStrh;
    QDataStream ds4(&audioStrh, QIODevice::WriteOnly);
    ds4.setByteOrder(QDataStream::LittleEndian);

    ds4.writeRawData("auds", 4);                  // fccType
    ds4 << quint32(1);                            // fccHandler (PCM = 1)
    ds4 << quint32(0);                            // dwFlags
    ds4 << quint16(0);                            // wPriority
    ds4 << quint16(0);                            // wLanguage
    ds4 << quint32(0);                            // dwInitialFrames
    ds4 << quint32(1);                            // dwScale
    ds4 << static_cast<quint32>(sampleRate);      // dwRate
    ds4 << quint32(0);                            // dwStart
    ds4 << audioSampleCount;                      // dwLength
    ds4 << static_cast<quint32>(avgBytesPerSec);  // dwSuggestedBufferSize
    ds4 << quint32(0);                            // dwQuality
    ds4 << static_cast<quint32>(blockAlign);      // dwSampleSize
    ds4 << quint16(0);                            // rcFrame
    ds4 << quint16(0);
    ds4 << quint16(0);
    ds4 << quint16(0);

    // strf (WAVEFORMATEX)
    QByteArray audioStrf;
    QDataStream ds5(&audioStrf, QIODevice::WriteOnly);
    ds5.setByteOrder(QDataStream::LittleEndian);

    ds5 << quint16(1);                            // wFormatTag (PCM)
    ds5 << static_cast<quint16>(channels);        // nChannels
    ds5 << static_cast<quint32>(sampleRate);      // nSamplesPerSec
    ds5 << static_cast<quint32>(avgBytesPerSec);  // nAvgBytesPerSec
    ds5 << static_cast<quint16>(blockAlign);      // nBlockAlign
    ds5 << static_cast<quint16>(bitsPerSample);   // wBitsPerSample
    ds5 << quint16(0);                            // cbSize

    QByteArray strlPayload;
    strlPayload.append("strl");
    strlPayload.append(buildChunk("strh", audioStrh));
    strlPayload.append(buildChunk("strf", audioStrf));

    QByteArray list;
    list.append("LIST");
    list.append(writeLittleEndian32(static_cast<quint32>(strlPayload.size())));
    list.append(strlPayload);
    return list;
}

}  // anonymous namespace

QByteArray buildInitialHeader()
{
    // Placeholder values that will be overwritten by buildFinalizedHeader()
    constexpr quint32 PlaceholderFps = 30;
    constexpr quint32 PlaceholderMicroSec = 33333;
    constexpr quint32 PlaceholderWidth = 384;
    constexpr quint32 PlaceholderHeight = 272;

    QByteArray hdrlPayload;
    hdrlPayload.append("hdrl");
    hdrlPayload.append(buildAvihChunk(PlaceholderMicroSec, 0, PlaceholderWidth, PlaceholderHeight));
    hdrlPayload.append(buildVideoStrlList(PlaceholderFps, 0, PlaceholderWidth, PlaceholderHeight));
    hdrlPayload.append(buildAudioStrlList(0, AudioSampleRate, AudioChannels, AudioBitsPerSample));

    QByteArray hdrl;
    hdrl.append("LIST");
    hdrl.append(writeLittleEndian32(static_cast<quint32>(hdrlPayload.size())));
    hdrl.append(hdrlPayload);

    // RIFF header (file size placeholder = 0)
    QByteArray header;
    header.append("RIFF");
    header.append(writeLittleEndian32(0));  // File size placeholder
    header.append("AVI ");
    header.append(hdrl);

    // movi LIST header
    header.append("LIST");
    header.append(writeLittleEndian32(0));  // movi size placeholder
    header.append("movi");

    return header;
}

QByteArray buildFinalizedHeader(const StreamParams &params)
{
    double safeFps = (params.fps > 0.0) ? params.fps : 30.0;
    auto fpsRate = static_cast<quint32>(std::lround(safeFps));
    quint32 microSecPerFrame = calculateMicroSecPerFrame(safeFps);

    QByteArray hdrlPayload;
    hdrlPayload.append("hdrl");
    hdrlPayload.append(buildAvihChunk(microSecPerFrame, static_cast<quint32>(params.frameCount),
                                      static_cast<quint32>(params.width),
                                      static_cast<quint32>(params.height)));
    hdrlPayload.append(buildVideoStrlList(fpsRate, static_cast<quint32>(params.frameCount),
                                          static_cast<quint32>(params.width),
                                          static_cast<quint32>(params.height)));
    hdrlPayload.append(buildAudioStrlList(static_cast<quint32>(params.audioSampleCount),
                                          params.audioSampleRate, params.audioChannels,
                                          params.audioBitsPerSample));

    QByteArray hdrl;
    hdrl.append("LIST");
    hdrl.append(writeLittleEndian32(static_cast<quint32>(hdrlPayload.size())));
    hdrl.append(hdrlPayload);

    QByteArray header;
    header.append("RIFF");
    header.append(writeLittleEndian32(0));  // File size — written by VideoRecordingService
    header.append("AVI ");
    header.append(hdrl);

    // movi LIST header (size — written by VideoRecordingService)
    header.append("LIST");
    header.append(writeLittleEndian32(0));
    header.append("movi");

    return header;
}

}  // namespace avi
