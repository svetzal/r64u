#ifndef AUDIOPLAYBACKSERVICE_H
#define AUDIOPLAYBACKSERVICE_H

#include "iaudioplaybackservice.h"

#include <QAudioFormat>
#include <QAudioSink>
#include <QByteArray>
#include <QElapsedTimer>
#include <QIODevice>
#include <QMutex>
#include <QObject>

#include <functional>
#include <memory>

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
class AudioPlaybackService : public IAudioPlaybackService
{
    Q_OBJECT

public:
    // closest standard rate to C64 audio
    static constexpr int DefaultSampleRate = 48000;

    static constexpr double PalSourceRate = 47982.8869047619;
    static constexpr double NtscSourceRate = 47940.3408482143;
    static constexpr int Channels = 2;
    static constexpr int BitsPerSample = 16;
    static constexpr int BytesPerFrame = 4;

    explicit AudioPlaybackService(QObject *parent = nullptr);
    ~AudioPlaybackService() override;

    bool start() override;
    void stop() override;
    [[nodiscard]] bool isPlaying() const override { return isPlaying_; }

    void setSampleRate(int rate);
    [[nodiscard]] int sampleRate() const { return sampleRate_; }

    void setVolume(qreal volume);
    [[nodiscard]] qreal volume() const;

    /**
     * @brief Callback interface for diagnostics timing data.
     */
    struct DiagnosticsCallback
    {
        std::function<void(qint64 writeTimeUs, qint64 bytesWritten, qint64 bytesDropped)>
            onSamplesWritten;
        std::function<void()> onPlaybackUnderrun;
    };

    /**
     * @brief Sets the diagnostics callback.
     *
     * When set, callbacks are invoked during packet processing
     * to provide high-frequency timing data for diagnostics.
     */
    void setDiagnosticsCallback(const DiagnosticsCallback &callback);

public slots:
    /**
     * @brief Writes audio samples for playback.
     * @param samples Interleaved stereo samples (16-bit signed, little-endian).
     * @param sampleCount Number of stereo sample pairs.
     */
    void writeSamples(const QByteArray &samples, int sampleCount) override;

signals:
    void errorOccurred(const QString &error);
    void playbackStateChanged(bool playing);
    void bufferUnderrun();

private slots:
    void onStateChanged(QAudio::State state);

private:
    void createAudioSink();

    std::unique_ptr<QAudioSink> audioSink_;
    QIODevice *audioDevice_ = nullptr;
    QAudioFormat audioFormat_;

    int sampleRate_ = DefaultSampleRate;
    qreal volume_ = 1.0;

    bool isPlaying_ = false;
    QMutex writeMutex_;

    DiagnosticsCallback diagnosticsCallback_;
    QElapsedTimer diagnosticsTimer_;
};

#endif  // AUDIOPLAYBACKSERVICE_H
