#ifndef AUDIOSTREAMRECEIVER_H
#define AUDIOSTREAMRECEIVER_H

#include "audiostreamcore.h"
#include "iaudiostreamreceiver.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QTimer>
#include <QUdpSocket>

#include <functional>

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
class AudioStreamReceiver : public IAudioStreamReceiver
{
    Q_OBJECT

public:
    static constexpr quint16 DefaultPort = 21001;

    using AudioFormat = IAudioStreamReceiver::AudioFormat;

    static constexpr int PacketSize = audiostream::PacketSize;
    static constexpr int HeaderSize = audiostream::HeaderSize;
    static constexpr int PayloadSize = audiostream::PayloadSize;
    static constexpr int SamplesPerPacket = audiostream::SamplesPerPacket;
    static constexpr int BytesPerSample = audiostream::BytesPerSample;
    static constexpr double PalSampleRate = audiostream::PalSampleRate;
    static constexpr double NtscSampleRate = audiostream::NtscSampleRate;

    static constexpr int DefaultJitterBufferSize = 10;

    explicit AudioStreamReceiver(QObject *parent = nullptr);
    ~AudioStreamReceiver() override;

    bool bind() override;
    bool bind(quint16 port);
    void close() override;
    [[nodiscard]] bool isActive() const override;

    [[nodiscard]] quint16 port() const;

    /**
     * @brief Sets the jitter buffer size.
     * @param packets Number of packets to buffer (default: 10).
     */
    void setJitterBufferSize(int packets);

    [[nodiscard]] int jitterBufferSize() const { return jitterBufferSize_; }
    [[nodiscard]] int bufferedPackets() const;

    void setAudioFormat(AudioFormat format) override;
    [[nodiscard]] AudioFormat audioFormat() const { return audioFormat_; }
    [[nodiscard]] double sampleRate() const;

    /**
     * @brief Callback interface for diagnostics timing data.
     */
    struct DiagnosticsCallback
    {
        std::function<void(qint64 arrivalTimeUs)> onPacketReceived;
        std::function<void()> onBufferUnderrun;
        std::function<void(int gap)> onSampleDiscontinuity;
    };

    /**
     * @brief Sets the diagnostics callback.
     *
     * When set, these callbacks are invoked during packet processing
     * to provide high-frequency timing data for diagnostics.
     */
    void setDiagnosticsCallback(const DiagnosticsCallback &callback);

signals:
    void socketError(const QString &error);

    /**
     * @brief Emitted periodically with reception statistics.
     * @param packetsReceived Total packets received.
     * @param packetsLost Estimated packets lost (from sequence number gaps).
     * @param bufferLevel Current jitter buffer level.
     */
    void statsUpdated(quint64 packetsReceived, quint64 packetsLost, int bufferLevel);

    void bufferUnderrun();

private slots:
    void onReadyRead();
    void onFlushTimer();

private:
    struct AudioPacket
    {
        quint16 sequenceNumber;
        QByteArray samples;
    };

    void processPacket(const QByteArray &packet);
    void flushBuffer();

    void startFlushTimer();
    void stopFlushTimer();
    [[nodiscard]] int calculateFlushIntervalUs() const;

    // Network
    QUdpSocket *socket_ = nullptr;

    // Jitter buffer
    QQueue<AudioPacket> jitterBuffer_;
    int jitterBufferSize_ = DefaultJitterBufferSize;
    mutable QMutex bufferMutex_;
    bool bufferPrimed_ = false;

    // Flush timer for steady playback timing
    QTimer *flushTimer_ = nullptr;

    // Format
    AudioFormat audioFormat_ = AudioFormat::PAL;

    // Statistics
    quint64 totalPacketsReceived_ = 0;
    quint64 totalPacketsLost_ = 0;
    quint16 lastSequenceNumber_ = 0;
    bool firstPacket_ = true;

    // Diagnostics
    DiagnosticsCallback diagnosticsCallback_;
    QElapsedTimer diagnosticsTimer_;
};

#endif  // AUDIOSTREAMRECEIVER_H
