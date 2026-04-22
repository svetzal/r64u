#ifndef IAUDIOSTREAMRECEIVER_H
#define IAUDIOSTREAMRECEIVER_H

#include <QByteArray>
#include <QObject>

/**
 * @brief Abstract interface for receiving C64 Ultimate audio stream packets.
 *
 * The AudioFormat enum is defined here so that both the interface and its
 * implementations use a consistent type.
 */
class IAudioStreamReceiver : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Audio format enumeration.
     */
    enum class AudioFormat { Unknown, PAL, NTSC };
    Q_ENUM(AudioFormat)

    explicit IAudioStreamReceiver(QObject *parent = nullptr) : QObject(parent) {}
    ~IAudioStreamReceiver() override = default;

    /**
     * @brief Binds the UDP socket to the default audio port.
     * @return true if binding succeeded, false otherwise.
     */
    virtual bool bind() = 0;

    /**
     * @brief Closes the UDP socket and stops reception.
     */
    virtual void close() = 0;

    /**
     * @brief Returns whether the socket is bound and receiving.
     * @return true if active, false otherwise.
     */
    [[nodiscard]] virtual bool isActive() const = 0;

    /**
     * @brief Sets the audio format (for sample rate selection).
     * @param format The audio format (PAL or NTSC).
     */
    virtual void setAudioFormat(AudioFormat format) = 0;

signals:
    /**
     * @brief Emitted when audio samples are ready for playback.
     * @param samples Interleaved stereo samples.
     * @param sampleCount Number of stereo sample pairs.
     */
    void samplesReady(const QByteArray &samples, int sampleCount);
};

#endif  // IAUDIOSTREAMRECEIVER_H
