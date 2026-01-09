/**
 * @file audiostreamreceiver.h
 * @brief UDP receiver for audio stream packets from Ultimate 64/II+ devices.
 *
 * Receives and buffers audio samples from UDP packets sent by the
 * C64 Ultimate device's audio streaming feature.
 */

#ifndef AUDIOSTREAMRECEIVER_H
#define AUDIOSTREAMRECEIVER_H

#include <QObject>
#include <QUdpSocket>
#include <QByteArray>
#include <QQueue>
#include <QMutex>

/**
 * @brief UDP receiver for C64 Ultimate audio stream packets.
 *
 * This class manages UDP packet reception on the audio port (default 21001)
 * and provides audio samples through a jitter buffer to smooth network timing.
 *
 * Audio packet format (770 bytes total):
 * - Header (2 bytes): sequence number (16-bit little-endian)
 * - Payload (768 bytes): 192 stereo samples (16-bit signed, little-endian)
 *
 * Sample rates:
 * - PAL: 47,982.887 Hz
 * - NTSC: 47,940.341 Hz
 *
 * @par Example usage:
 * @code
 * AudioStreamReceiver *receiver = new AudioStreamReceiver(this);
 *
 * connect(receiver, &AudioStreamReceiver::samplesReady,
 *         this, &MyClass::onAudioSamples);
 *
 * if (receiver->bind(21001)) {
 *     qDebug() << "Audio receiver started on port 21001";
 * }
 * @endcode
 */
class AudioStreamReceiver : public QObject
{
    Q_OBJECT

public:
    /// Default UDP port for audio stream reception
    static constexpr quint16 DefaultPort = 21001;

    /// Audio packet size in bytes (2-byte header + 768-byte payload)
    static constexpr int PacketSize = 770;

    /// Header size in bytes
    static constexpr int HeaderSize = 2;

    /// Payload size in bytes (192 stereo samples × 4 bytes)
    static constexpr int PayloadSize = 768;

    /// Number of stereo samples per packet
    static constexpr int SamplesPerPacket = 192;

    /// Bytes per stereo sample (L16 + R16)
    static constexpr int BytesPerSample = 4;

    /// PAL sample rate (Hz)
    static constexpr double PalSampleRate = 47982.8869047619;

    /// NTSC sample rate (Hz)
    static constexpr double NtscSampleRate = 47940.3408482143;

    /// Default jitter buffer size (in packets)
    static constexpr int DefaultJitterBufferSize = 10;

    /**
     * @brief Audio format enumeration.
     */
    enum class AudioFormat {
        Unknown,
        PAL,
        NTSC
    };

    /**
     * @brief Constructs an audio stream receiver.
     * @param parent Optional parent QObject for memory management.
     */
    explicit AudioStreamReceiver(QObject *parent = nullptr);

    /**
     * @brief Destructor. Closes the socket.
     */
    ~AudioStreamReceiver() override;

    /**
     * @brief Binds the UDP socket to the specified port.
     * @param port UDP port number to listen on (default: 21001).
     * @return true if binding succeeded, false otherwise.
     */
    bool bind(quint16 port = DefaultPort);

    /**
     * @brief Closes the UDP socket and stops reception.
     */
    void close();

    /**
     * @brief Returns whether the socket is bound and receiving.
     * @return true if active, false otherwise.
     */
    [[nodiscard]] bool isActive() const;

    /**
     * @brief Returns the port the socket is bound to.
     * @return The port number, or 0 if not bound.
     */
    [[nodiscard]] quint16 port() const;

    /**
     * @brief Sets the jitter buffer size.
     * @param packets Number of packets to buffer (default: 10).
     */
    void setJitterBufferSize(int packets);

    /**
     * @brief Returns the jitter buffer size.
     * @return Number of packets in the jitter buffer.
     */
    [[nodiscard]] int jitterBufferSize() const { return jitterBufferSize_; }

    /**
     * @brief Returns the number of buffered packets.
     * @return Current buffer level.
     */
    [[nodiscard]] int bufferedPackets() const;

    /**
     * @brief Sets the audio format (for sample rate selection).
     * @param format The audio format (PAL or NTSC).
     */
    void setAudioFormat(AudioFormat format);

    /**
     * @brief Returns the current audio format.
     * @return The audio format.
     */
    [[nodiscard]] AudioFormat audioFormat() const { return audioFormat_; }

    /**
     * @brief Returns the sample rate for the current format.
     * @return Sample rate in Hz.
     */
    [[nodiscard]] double sampleRate() const;

signals:
    /**
     * @brief Emitted when audio samples are ready for playback.
     * @param samples Interleaved stereo samples (L, R, L, R, ...) as 16-bit signed values.
     * @param sampleCount Number of stereo sample pairs.
     */
    void samplesReady(const QByteArray &samples, int sampleCount);

    /**
     * @brief Emitted when a socket error occurs.
     * @param error Error description.
     */
    void socketError(const QString &error);

    /**
     * @brief Emitted periodically with reception statistics.
     * @param packetsReceived Total packets received.
     * @param packetsLost Estimated packets lost (sequence gaps).
     * @param bufferLevel Current jitter buffer level.
     */
    void statsUpdated(quint64 packetsReceived, quint64 packetsLost, int bufferLevel);

    /**
     * @brief Emitted when the jitter buffer underruns.
     */
    void bufferUnderrun();

private slots:
    void onReadyRead();

private:
    struct AudioPacket {
        quint16 sequenceNumber;
        QByteArray samples;
    };

    void processPacket(const QByteArray &packet);
    void flushBuffer();

    // Network
    QUdpSocket *socket_ = nullptr;

    // Jitter buffer
    QQueue<AudioPacket> jitterBuffer_;
    int jitterBufferSize_ = DefaultJitterBufferSize;
    mutable QMutex bufferMutex_;
    bool bufferPrimed_ = false;

    // Format
    AudioFormat audioFormat_ = AudioFormat::PAL;

    // Statistics
    quint64 totalPacketsReceived_ = 0;
    quint64 totalPacketsLost_ = 0;
    quint16 lastSequenceNumber_ = 0;
    bool firstPacket_ = true;
};

#endif // AUDIOSTREAMRECEIVER_H
