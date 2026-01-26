/**
 * @file audioplaybackservice.h
 * @brief Audio playback service using Qt Multimedia.
 *
 * Plays audio samples received from the C64 Ultimate device through
 * the system's audio output device using QAudioSink.
 */

#ifndef AUDIOPLAYBACKSERVICE_H
#define AUDIOPLAYBACKSERVICE_H

#include <QObject>
#include <QAudioFormat>
#include <QAudioSink>
#include <QIODevice>
#include <QByteArray>
#include <QMutex>
#include <QElapsedTimer>
#include <memory>
#include <functional>

/**
 * @brief Service for audio playback using Qt Multimedia.
 *
 * This class handles audio playback of 16-bit stereo samples from the
 * C64 Ultimate device. It uses QAudioSink for low-latency audio output
 * and handles sample rate conversion if needed.
 *
 * @par Example usage:
 * @code
 * AudioPlaybackService *playback = new AudioPlaybackService(this);
 *
 * if (playback->start()) {
 *     // Connect to audio receiver
 *     connect(receiver, &AudioStreamReceiver::samplesReady,
 *             playback, &AudioPlaybackService::writeSamples);
 * }
 * @endcode
 */
class AudioPlaybackService : public QObject
{
    Q_OBJECT

public:
    /// Default sample rate (closest standard rate to C64 audio)
    static constexpr int DefaultSampleRate = 48000;

    /// PAL source sample rate
    static constexpr double PalSourceRate = 47982.8869047619;

    /// NTSC source sample rate
    static constexpr double NtscSourceRate = 47940.3408482143;

    /// Number of audio channels (stereo)
    static constexpr int Channels = 2;

    /// Bits per sample
    static constexpr int BitsPerSample = 16;

    /// Bytes per sample frame (stereo = 4 bytes)
    static constexpr int BytesPerFrame = 4;

    /**
     * @brief Constructs an audio playback service.
     * @param parent Optional parent QObject for memory management.
     */
    explicit AudioPlaybackService(QObject *parent = nullptr);

    /**
     * @brief Destructor. Stops playback and releases resources.
     */
    ~AudioPlaybackService() override;

    /**
     * @brief Starts audio playback.
     * @return true if playback started successfully, false otherwise.
     */
    bool start();

    /**
     * @brief Stops audio playback.
     */
    void stop();

    /**
     * @brief Returns whether audio is currently playing.
     * @return true if playing, false otherwise.
     */
    [[nodiscard]] bool isPlaying() const { return isPlaying_; }

    /**
     * @brief Sets the output sample rate.
     * @param rate Sample rate in Hz (default: 48000).
     */
    void setSampleRate(int rate);

    /**
     * @brief Returns the current sample rate.
     * @return Sample rate in Hz.
     */
    [[nodiscard]] int sampleRate() const { return sampleRate_; }

    /**
     * @brief Sets the volume level.
     * @param volume Volume from 0.0 (muted) to 1.0 (full volume).
     */
    void setVolume(qreal volume);

    /**
     * @brief Returns the current volume level.
     * @return Volume from 0.0 to 1.0.
     */
    [[nodiscard]] qreal volume() const;

    /**
     * @brief Callback interface for diagnostics timing data.
     */
    struct DiagnosticsCallback {
        std::function<void(qint64 writeTimeUs, qint64 bytesWritten, qint64 bytesDropped)> onSamplesWritten;
        std::function<void()> onPlaybackUnderrun;
    };

    /**
     * @brief Sets the diagnostics callback for timing data.
     * @param callback Callback structure with timing functions.
     */
    void setDiagnosticsCallback(const DiagnosticsCallback &callback);

public slots:
    /**
     * @brief Writes audio samples for playback.
     * @param samples Interleaved stereo samples (16-bit signed, little-endian).
     * @param sampleCount Number of stereo sample pairs.
     */
    void writeSamples(const QByteArray &samples, int sampleCount);

signals:
    /**
     * @brief Emitted when an error occurs.
     * @param error Error description.
     */
    void errorOccurred(const QString &error);

    /**
     * @brief Emitted when playback state changes.
     * @param playing true if now playing, false if stopped.
     */
    void playbackStateChanged(bool playing);

    /**
     * @brief Emitted when audio buffer underruns (not enough data).
     */
    void bufferUnderrun();

private slots:
    void onStateChanged(QAudio::State state);

private:
    void createAudioSink();

    // Audio output
    std::unique_ptr<QAudioSink> audioSink_;
    QIODevice *audioDevice_ = nullptr;
    QAudioFormat audioFormat_;

    // Configuration
    int sampleRate_ = DefaultSampleRate;
    qreal volume_ = 1.0;

    // State
    bool isPlaying_ = false;
    QMutex writeMutex_;

    // Diagnostics
    DiagnosticsCallback diagnosticsCallback_;
    QElapsedTimer diagnosticsTimer_;
};

#endif // AUDIOPLAYBACKSERVICE_H
