/**
 * @file videorecordingservice.cpp
 * @brief Implementation of video recording to AVI files.
 */

#include "videorecordingservice.h"

#include <QBuffer>
#include <QMutexLocker>

VideoRecordingService::VideoRecordingService(QObject *parent) : QObject(parent) {}

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
    chunkIndex_.clear();
    audioSampleCount_ = 0;

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
    jpegData = avi::padToEven(jpegData);

    // Record chunk info for index
    ChunkInfo info;
    info.fourCC = "00dc";
    info.offset = file_.pos() - moviListStart_;
    info.size = jpegData.size();
    chunkIndex_.append(info);

    // Write video frame chunk
    writeChunk("00dc", jpegData);
    frameCount_++;
}

void VideoRecordingService::addAudioSamples(const QByteArray &samples, int sampleCount)
{
    QMutexLocker locker(&mutex_);

    if (!recording_ || samples.isEmpty()) {
        return;
    }

    // Pad to even size (AVI requirement)
    QByteArray audioData = avi::padToEven(samples);

    // Record chunk info for index
    ChunkInfo info;
    info.fourCC = "01wb";
    info.offset = file_.pos() - moviListStart_;
    info.size = audioData.size();
    chunkIndex_.append(info);

    // Write audio chunk
    writeChunk("01wb", audioData);
    audioSampleCount_ += sampleCount;
}

void VideoRecordingService::writeChunk(const QByteArray &fourCC, const QByteArray &data)
{
    file_.write(avi::buildChunk(fourCC, data));
}

void VideoRecordingService::writeAviHeader()
{
    QByteArray initialHeader = avi::buildInitialHeader();
    file_.write(initialHeader);

    // The initial header ends with: ... LIST [size:4] movi
    // moviListSizePos_ points to the 4-byte size field before "movi"
    moviListSizePos_ = static_cast<qint64>(initialHeader.size()) - 8;
    moviListStart_ = file_.pos();  // Position right after "movi"
}

void VideoRecordingService::finalizeAvi()
{
    if (frameCount_ == 0) {
        return;
    }

    // Capture movi LIST size before writing the index
    qint64 moviEnd = file_.pos();
    qint64 moviSize = moviEnd - moviListSizePos_ - 4;

    // Write idx1 chunk using pure core function
    writeChunk("idx1", avi::buildIdx1(chunkIndex_));

    // Compute RIFF file size
    qint64 fileEnd = file_.pos();
    qint64 riffSize = fileEnd - 8;

    // Calculate actual frame rate
    qint64 durationMs = startTime_.msecsTo(lastFrameTime_);
    double fps = avi::calculateFps(frameCount_, durationMs);

    // Build and write the finalised header at offset 0
    avi::StreamParams params;
    params.width = width_;
    params.height = height_;
    params.frameCount = frameCount_;
    params.fps = fps;
    params.audioSampleCount = audioSampleCount_;

    QByteArray finalHeader = avi::buildFinalizedHeader(params);
    file_.seek(0);
    file_.write(finalHeader);

    // Patch RIFF file size at offset 4 (overrides the placeholder)
    file_.seek(4);
    file_.write(avi::writeLittleEndian32(static_cast<quint32>(riffSize)));

    // Patch movi LIST size (overrides the placeholder written into finalHeader)
    file_.seek(moviListSizePos_);
    file_.write(avi::writeLittleEndian32(static_cast<quint32>(moviSize)));
}
