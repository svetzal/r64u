/**
 * @file iaudioplaybackservice.h
 * @brief Gateway interface for audio playback service implementations.
 *
 * Abstracts audio playback so that StreamingManager can be tested
 * without requiring a real audio device.
 */

#ifndef IAUDIOPLAYBACKSERVICE_H
#define IAUDIOPLAYBACKSERVICE_H

#include <QByteArray>
#include <QObject>

/**
 * @brief Abstract interface for C64 Ultimate audio playback.
 */
class IAudioPlaybackService : public QObject
{
    Q_OBJECT

public:
    explicit IAudioPlaybackService(QObject *parent = nullptr) : QObject(parent) {}
    ~IAudioPlaybackService() override = default;

    /**
     * @brief Starts audio playback.
     * @return true if playback started successfully, false otherwise.
     */
    virtual bool start() = 0;

    /**
     * @brief Stops audio playback.
     */
    virtual void stop() = 0;

    /**
     * @brief Returns whether audio is currently playing.
     * @return true if playing, false otherwise.
     */
    [[nodiscard]] virtual bool isPlaying() const = 0;

public slots:
    /**
     * @brief Writes audio samples for playback.
     * @param samples Interleaved stereo samples (16-bit signed, little-endian).
     * @param sampleCount Number of stereo sample pairs.
     */
    virtual void writeSamples(const QByteArray &samples, int sampleCount) = 0;
};

#endif  // IAUDIOPLAYBACKSERVICE_H
