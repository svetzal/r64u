/**
 * @file videorecordingservice.cpp
 * @brief Implementation of video recording to AVI files.
 */

#include "videorecordingservice.h"
#include <QBuffer>
#include <QDataStream>
#include <QMutexLocker>

VideoRecordingService::VideoRecordingService(QObject *parent)
    : QObject(parent)
{
}

VideoRecordingService::~VideoRecordingService()
{
    if (recording_) {
        stopRecording();
    }
}

bool VideoRecordingService::startRecording(const QString &filePath)
{
    QMutexLocker locker(&mutex_);

    if (recording_) {
        emit error(tr("Already recording"));
        return false;
    }

    file_.setFileName(filePath);
    if (!file_.open(QIODevice::WriteOnly)) {
        emit error(tr("Failed to open file for writing: %1").arg(file_.errorString()));
        return false;
    }

    recordingPath_ = filePath;
    frameCount_ = 0;
    width_ = 0;
    height_ = 0;
    startTime_ = QDateTime::currentDateTime();
    lastFrameTime_ = startTime_;
    frameOffsets_.clear();
    frameSizes_.clear();

    // Write placeholder AVI header (will be updated when finalizing)
    writeAviHeader();

    recording_ = true;
    emit recordingStarted(filePath);
    return true;
}

bool VideoRecordingService::stopRecording()
{
    QMutexLocker locker(&mutex_);

    if (!recording_) {
        return false;
    }

    recording_ = false;

    // Finalize the AVI file
    finalizeAvi();
    file_.close();

    int count = frameCount_;
    QString path = recordingPath_;
    recordingPath_.clear();

    emit recordingStopped(path, count);
    return true;
}

void VideoRecordingService::addFrame(const QImage &frame)
{
    QMutexLocker locker(&mutex_);

    if (!recording_ || frame.isNull()) {
        return;
    }

    // Store dimensions from first frame
    if (frameCount_ == 0) {
        width_ = frame.width();
        height_ = frame.height();
    }

    // Record frame timing
    lastFrameTime_ = QDateTime::currentDateTime();

    // Convert frame to JPEG
    QByteArray jpegData;
    QBuffer buffer(&jpegData);
    buffer.open(QIODevice::WriteOnly);

    // Use RGB32 format for consistent encoding
    QImage rgbFrame = frame;
    if (frame.format() != QImage::Format_RGB32) {
        rgbFrame = frame.convertToFormat(QImage::Format_RGB32);
    }

    if (!rgbFrame.save(&buffer, "JPEG", 85)) {
        // If JPEG fails, skip this frame
        return;
    }

    // Pad to even size (AVI requirement)
    if (jpegData.size() % 2 != 0) {
        jpegData.append('\0');
    }

    // Record frame offset and size
    frameOffsets_.append(file_.pos() - moviListStart_);
    frameSizes_.append(jpegData.size());

    // Write video frame chunk
    writeChunk("00dc", jpegData);
    frameCount_++;
}

void VideoRecordingService::writeChunk(const QByteArray &fourCC, const QByteArray &data)
{
    file_.write(fourCC.leftJustified(4, '\0', true));

    // Write size as little-endian 32-bit
    quint32 size = static_cast<quint32>(data.size());
    char sizeBytes[4];
    sizeBytes[0] = static_cast<char>(size & 0xFF);
    sizeBytes[1] = static_cast<char>((size >> 8) & 0xFF);
    sizeBytes[2] = static_cast<char>((size >> 16) & 0xFF);
    sizeBytes[3] = static_cast<char>((size >> 24) & 0xFF);
    file_.write(sizeBytes, 4);

    file_.write(data);
}

void VideoRecordingService::writeAviHeader()
{
    // Write RIFF header (placeholder, will be updated)
    file_.write("RIFF");
    file_.write(QByteArray(4, '\0')); // File size placeholder
    file_.write("AVI ");

    // Write hdrl LIST
    file_.write("LIST");
    file_.write(QByteArray(4, '\0')); // List size placeholder
    qint64 hdrlSizePos = file_.pos() - 4;
    file_.write("hdrl");

    // Write avih (main AVI header)
    // All values are little-endian
    QByteArray avih;
    QDataStream ds(&avih, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);

    ds << quint32(33333);   // dwMicroSecPerFrame (30 fps placeholder)
    ds << quint32(0);       // dwMaxBytesPerSec (placeholder)
    ds << quint32(0);       // dwPaddingGranularity
    ds << quint32(0x110);   // dwFlags (AVIF_HASINDEX | AVIF_ISINTERLEAVED)
    ds << quint32(0);       // dwTotalFrames (placeholder)
    ds << quint32(0);       // dwInitialFrames
    ds << quint32(1);       // dwStreams
    ds << quint32(1000000); // dwSuggestedBufferSize
    ds << quint32(384);     // dwWidth (placeholder)
    ds << quint32(272);     // dwHeight (placeholder)
    ds << quint32(0);       // dwReserved[4]
    ds << quint32(0);
    ds << quint32(0);
    ds << quint32(0);

    writeChunk("avih", avih);

    // Write strl LIST (stream list)
    file_.write("LIST");
    qint64 strlSizePos = file_.pos();
    file_.write(QByteArray(4, '\0'));
    file_.write("strl");

    // Write strh (stream header)
    QByteArray strh;
    QDataStream ds2(&strh, QIODevice::WriteOnly);
    ds2.setByteOrder(QDataStream::LittleEndian);

    ds2.writeRawData("vids", 4);  // fccType (video stream)
    ds2.writeRawData("MJPG", 4);  // fccHandler (MJPEG)
    ds2 << quint32(0);            // dwFlags
    ds2 << quint16(0);            // wPriority
    ds2 << quint16(0);            // wLanguage
    ds2 << quint32(0);            // dwInitialFrames
    ds2 << quint32(1);            // dwScale
    ds2 << quint32(30);           // dwRate (placeholder fps)
    ds2 << quint32(0);            // dwStart
    ds2 << quint32(0);            // dwLength (placeholder)
    ds2 << quint32(1000000);      // dwSuggestedBufferSize
    ds2 << quint32(0);            // dwQuality
    ds2 << quint32(0);            // dwSampleSize
    ds2 << quint16(0);            // rcFrame (left)
    ds2 << quint16(0);            // rcFrame (top)
    ds2 << quint16(384);          // rcFrame (right) placeholder
    ds2 << quint16(272);          // rcFrame (bottom) placeholder

    writeChunk("strh", strh);

    // Write strf (stream format - BITMAPINFOHEADER)
    QByteArray strf;
    QDataStream ds3(&strf, QIODevice::WriteOnly);
    ds3.setByteOrder(QDataStream::LittleEndian);

    ds3 << quint32(40);           // biSize
    ds3 << qint32(384);           // biWidth placeholder
    ds3 << qint32(272);           // biHeight placeholder
    ds3 << quint16(1);            // biPlanes
    ds3 << quint16(24);           // biBitCount
    ds3.writeRawData("MJPG", 4);  // biCompression
    ds3 << quint32(0);            // biSizeImage
    ds3 << qint32(0);             // biXPelsPerMeter
    ds3 << qint32(0);             // biYPelsPerMeter
    ds3 << quint32(0);            // biClrUsed
    ds3 << quint32(0);            // biClrImportant

    writeChunk("strf", strf);

    // Update strl LIST size
    qint64 strlEnd = file_.pos();
    qint64 strlSize = strlEnd - strlSizePos - 4;
    file_.seek(strlSizePos);
    char strlSizeBytes[4];
    strlSizeBytes[0] = static_cast<char>(strlSize & 0xFF);
    strlSizeBytes[1] = static_cast<char>((strlSize >> 8) & 0xFF);
    strlSizeBytes[2] = static_cast<char>((strlSize >> 16) & 0xFF);
    strlSizeBytes[3] = static_cast<char>((strlSize >> 24) & 0xFF);
    file_.write(strlSizeBytes, 4);
    file_.seek(strlEnd);

    // Update hdrl LIST size
    qint64 hdrlEnd = file_.pos();
    qint64 hdrlSize = hdrlEnd - hdrlSizePos;
    file_.seek(hdrlSizePos);
    char hdrlSizeBytes[4];
    hdrlSizeBytes[0] = static_cast<char>(hdrlSize & 0xFF);
    hdrlSizeBytes[1] = static_cast<char>((hdrlSize >> 8) & 0xFF);
    hdrlSizeBytes[2] = static_cast<char>((hdrlSize >> 16) & 0xFF);
    hdrlSizeBytes[3] = static_cast<char>((hdrlSize >> 24) & 0xFF);
    file_.write(hdrlSizeBytes, 4);
    file_.seek(hdrlEnd);

    // Write movi LIST header
    file_.write("LIST");
    moviListSizePos_ = file_.pos();
    file_.write(QByteArray(4, '\0')); // Size placeholder
    file_.write("movi");
    moviListStart_ = file_.pos();
}

void VideoRecordingService::finalizeAvi()
{
    if (frameCount_ == 0) {
        return;
    }

    // Update movi LIST size
    qint64 moviEnd = file_.pos();
    qint64 moviSize = moviEnd - moviListSizePos_ - 4;
    file_.seek(moviListSizePos_);
    char moviSizeBytes[4];
    moviSizeBytes[0] = static_cast<char>(moviSize & 0xFF);
    moviSizeBytes[1] = static_cast<char>((moviSize >> 8) & 0xFF);
    moviSizeBytes[2] = static_cast<char>((moviSize >> 16) & 0xFF);
    moviSizeBytes[3] = static_cast<char>((moviSize >> 24) & 0xFF);
    file_.write(moviSizeBytes, 4);
    file_.seek(moviEnd);

    // Write idx1 (index)
    QByteArray idx1Data;
    QDataStream idxStream(&idx1Data, QIODevice::WriteOnly);
    idxStream.setByteOrder(QDataStream::LittleEndian);

    for (int i = 0; i < frameCount_; ++i) {
        idxStream.writeRawData("00dc", 4);                  // ckid
        idxStream << quint32(0x10);                         // dwFlags (AVIIF_KEYFRAME)
        idxStream << quint32(frameOffsets_[i]);             // dwChunkOffset
        idxStream << quint32(frameSizes_[i]);               // dwChunkLength
    }

    writeChunk("idx1", idx1Data);

    // Calculate frame rate
    qint64 durationMs = startTime_.msecsTo(lastFrameTime_);
    if (durationMs <= 0) {
        durationMs = 1;
    }
    double fps = (frameCount_ > 1) ? (frameCount_ - 1) * 1000.0 / durationMs : 30.0;
    if (fps < 1.0) fps = 1.0;
    if (fps > 60.0) fps = 60.0;

    quint32 microSecPerFrame = static_cast<quint32>(1000000.0 / fps);
    quint32 fpsRate = static_cast<quint32>(fps + 0.5);

    // Update RIFF file size
    qint64 fileEnd = file_.pos();
    qint64 riffSize = fileEnd - 8;
    file_.seek(4);
    char riffSizeBytes[4];
    riffSizeBytes[0] = static_cast<char>(riffSize & 0xFF);
    riffSizeBytes[1] = static_cast<char>((riffSize >> 8) & 0xFF);
    riffSizeBytes[2] = static_cast<char>((riffSize >> 16) & 0xFF);
    riffSizeBytes[3] = static_cast<char>((riffSize >> 24) & 0xFF);
    file_.write(riffSizeBytes, 4);

    // Update avih header with actual values
    // avih starts at offset 32 (RIFF[8] + AVI [4] + LIST[8] + hdrl[4] + avih[8])
    file_.seek(32);

    QByteArray avih;
    QDataStream ds(&avih, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);

    ds << microSecPerFrame;         // dwMicroSecPerFrame
    ds << quint32(0);               // dwMaxBytesPerSec
    ds << quint32(0);               // dwPaddingGranularity
    ds << quint32(0x110);           // dwFlags
    ds << quint32(frameCount_);     // dwTotalFrames
    ds << quint32(0);               // dwInitialFrames
    ds << quint32(1);               // dwStreams
    ds << quint32(1000000);         // dwSuggestedBufferSize
    ds << quint32(width_);          // dwWidth
    ds << quint32(height_);         // dwHeight
    ds << quint32(0);               // dwReserved[4]
    ds << quint32(0);
    ds << quint32(0);
    ds << quint32(0);

    file_.write(avih);

    // Update strh header
    // strh starts after avih: 32 + avih_size(56) + LIST[8] + strl[4] + strh[8] = 108
    file_.seek(108);

    QByteArray strh;
    QDataStream ds2(&strh, QIODevice::WriteOnly);
    ds2.setByteOrder(QDataStream::LittleEndian);

    ds2.writeRawData("vids", 4);
    ds2.writeRawData("MJPG", 4);
    ds2 << quint32(0);              // dwFlags
    ds2 << quint16(0);              // wPriority
    ds2 << quint16(0);              // wLanguage
    ds2 << quint32(0);              // dwInitialFrames
    ds2 << quint32(1);              // dwScale
    ds2 << quint32(fpsRate);        // dwRate
    ds2 << quint32(0);              // dwStart
    ds2 << quint32(frameCount_);    // dwLength
    ds2 << quint32(1000000);        // dwSuggestedBufferSize
    ds2 << quint32(0);              // dwQuality
    ds2 << quint32(0);              // dwSampleSize
    ds2 << quint16(0);              // rcFrame (left)
    ds2 << quint16(0);              // rcFrame (top)
    ds2 << quint16(width_);         // rcFrame (right)
    ds2 << quint16(height_);        // rcFrame (bottom)

    file_.write(strh);

    // Update strf header (BITMAPINFOHEADER)
    // strf starts after strh: 108 + strh_size(56) + strf[8] = 172
    file_.seek(172);

    QByteArray strf;
    QDataStream ds3(&strf, QIODevice::WriteOnly);
    ds3.setByteOrder(QDataStream::LittleEndian);

    ds3 << quint32(40);             // biSize
    ds3 << qint32(width_);          // biWidth
    ds3 << qint32(height_);         // biHeight
    ds3 << quint16(1);              // biPlanes
    ds3 << quint16(24);             // biBitCount
    ds3.writeRawData("MJPG", 4);    // biCompression
    ds3 << quint32(0);              // biSizeImage
    ds3 << qint32(0);               // biXPelsPerMeter
    ds3 << qint32(0);               // biYPelsPerMeter
    ds3 << quint32(0);              // biClrUsed
    ds3 << quint32(0);              // biClrImportant

    file_.write(strf);
}
