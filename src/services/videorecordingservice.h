/**
 * @file videorecordingservice.h
 * @brief Service for recording video streams to AVI files.
 */

#ifndef VIDEORECORDINGSERVICE_H
#define VIDEORECORDINGSERVICE_H

#include <QObject>
#include <QFile>
#include <QMutex>
#include <QImage>
#include <QString>
#include <QDateTime>

/**
 * @brief Records video and audio to an AVI file.
 *
 * This service receives QImage frames and audio samples and writes them
 * to an AVI container using MJPEG video encoding and PCM audio for
 * reasonable file sizes and wide compatibility.
 *
 * Usage:
 * 1. Call startRecording() with the output file path
 * 2. Call addFrame() for each video frame
 * 3. Call addAudioSamples() for audio data
 * 4. Call stopRecording() to finalize the file
 *
 * The frame rate is calculated based on the time between the first
 * and last frames.
 */
class VideoRecordingService : public QObject
{
    Q_OBJECT

public:
    explicit VideoRecordingService(QObject *parent = nullptr);
    ~VideoRecordingService() override;

    /**
     * @brief Returns whether recording is currently active.
     */
    [[nodiscard]] bool isRecording() const { return recording_; }

    /**
     * @brief Returns the current recording file path.
     */
    [[nodiscard]] QString recordingPath() const { return recordingPath_; }

    /**
     * @brief Returns the number of frames recorded so far.
     */
    [[nodiscard]] int frameCount() const { return frameCount_; }

public slots:
    /**
     * @brief Starts recording to the specified file.
     * @param filePath Path to the output AVI file.
     * @return True if recording started successfully.
     */
    bool startRecording(const QString &filePath);

    /**
     * @brief Stops recording and finalizes the AVI file.
     * @return True if the file was finalized successfully.
     */
    bool stopRecording();

    /**
     * @brief Adds a frame to the recording.
     * @param frame The frame to add.
     *
     * Does nothing if not currently recording.
     */
    void addFrame(const QImage &frame);

    /**
     * @brief Adds audio samples to the recording.
     * @param samples Interleaved stereo samples (16-bit signed, little-endian).
     * @param sampleCount Number of stereo sample pairs.
     *
     * Does nothing if not currently recording.
     */
    void addAudioSamples(const QByteArray &samples, int sampleCount);

signals:
    /**
     * @brief Emitted when recording starts.
     * @param filePath The output file path.
     */
    void recordingStarted(const QString &filePath);

    /**
     * @brief Emitted when recording stops.
     * @param filePath The output file path.
     * @param frameCount The number of frames recorded.
     */
    void recordingStopped(const QString &filePath, int frameCount);

    /**
     * @brief Emitted when an error occurs.
     * @param error The error message.
     */
    void error(const QString &error);

private:
    void writeAviHeader();
    void finalizeAvi();
    void writeChunk(const QByteArray &fourCC, const QByteArray &data);

    QMutex mutex_;
    QFile file_;
    QString recordingPath_;
    bool recording_ = false;
    int frameCount_ = 0;
    int width_ = 0;
    int height_ = 0;
    QDateTime startTime_;
    QDateTime lastFrameTime_;

    // AVI file structure tracking
    qint64 moviListStart_ = 0;
    qint64 moviListSizePos_ = 0;

    // Index entries for all chunks (video and audio)
    struct ChunkInfo {
        QByteArray fourCC;  // "00dc" for video, "01wb" for audio
        qint64 offset;
        int size;
    };
    QList<ChunkInfo> chunkIndex_;

    // Audio tracking
    int audioSampleCount_ = 0;
    static constexpr int AudioSampleRate = 48000;  // Close to actual ~47983 Hz
    static constexpr int AudioChannels = 2;
    static constexpr int AudioBitsPerSample = 16;
};

#endif // VIDEORECORDINGSERVICE_H
