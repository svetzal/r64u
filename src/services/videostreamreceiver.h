#ifndef VIDEOSTREAMRECEIVER_H
#define VIDEOSTREAMRECEIVER_H

#include "ivideostreamreceiver.h"
#include "videostreamcore.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QSet>
#include <QUdpSocket>
#include <QVector>

#include <functional>

/**
 * @brief UDP receiver for C64 Ultimate video stream packets.
 *
 * This class manages UDP packet reception on the video port (default 21000)
 * and reassembles frames from multiple packets. Each packet contains 4 lines
 * of 4-bit VIC-II color data that must be assembled into complete frames.
 *
 * Video packet format (780 bytes total):
 * - Header (12 bytes): seq(2), frame(2), line(2), ppl(2), lpp(1), bpp(1), enc(2)
 * - Payload (768 bytes): 4 lines × 192 bytes (384 pixels × 4 bits)
 *
 * @par Example usage:
 * @code
 * VideoStreamReceiver *receiver = new VideoStreamReceiver(this);
 *
 * connect(receiver, &VideoStreamReceiver::frameReady,
 *         this, &MyClass::onFrameReceived);
 *
 * if (receiver->bind(21000)) {
 *     qDebug() << "Video receiver started on port 21000";
 * }
 * @endcode
 */
class VideoStreamReceiver : public IVideoStreamReceiver
{
    Q_OBJECT

public:
    /// Default UDP port for video stream reception
    static constexpr quint16 DefaultPort = 21000;

    /// Type alias for backward compatibility with callers using VideoStreamReceiver::VideoFormat
    using VideoFormat = IVideoStreamReceiver::VideoFormat;

    /// Video packet size in bytes (12-byte header + 768-byte payload)
    static constexpr int PacketSize = videostream::PacketSize;
    /// Header size in bytes
    static constexpr int HeaderSize = videostream::HeaderSize;
    /// Payload size in bytes (4 lines × 192 bytes per line)
    static constexpr int PayloadSize = videostream::PayloadSize;
    /// Pixels per line in the video stream
    static constexpr int PixelsPerLine = videostream::PixelsPerLine;
    /// Lines per packet
    static constexpr int LinesPerPacket = videostream::LinesPerPacket;
    /// Bits per pixel (VIC-II uses 4-bit color indices)
    static constexpr int BitsPerPixel = videostream::BitsPerPixel;
    /// Bytes per line (384 pixels × 4 bits / 8)
    static constexpr int BytesPerLine = videostream::BytesPerLine;
    /// Maximum frame height (PAL)
    static constexpr int MaxFrameHeight = videostream::MaxFrameHeight;
    /// PAL frame height
    static constexpr int PalHeight = videostream::PalHeight;
    /// NTSC frame height
    static constexpr int NtscHeight = videostream::NtscHeight;
    /// PAL packets per frame (272 lines / 4 lines per packet)
    static constexpr int PalPacketsPerFrame = videostream::PalPacketsPerFrame;
    /// NTSC packets per frame (240 lines / 4 lines per packet)
    static constexpr int NtscPacketsPerFrame = videostream::NtscPacketsPerFrame;

    /**
     * @brief Constructs a video stream receiver.
     * @param parent Optional parent QObject for memory management.
     */
    explicit VideoStreamReceiver(QObject *parent = nullptr);

    /**
     * @brief Destructor. Closes the socket.
     */
    ~VideoStreamReceiver() override;

    /**
     * @brief Binds the UDP socket to the default video port.
     * @return true if binding succeeded, false otherwise.
     */
    bool bind() override;

    /**
     * @brief Binds the UDP socket to the specified port.
     * @param port UDP port number to listen on (default: 21000).
     * @return true if binding succeeded, false otherwise.
     */
    bool bind(quint16 port);

    /**
     * @brief Closes the UDP socket and stops reception.
     */
    void close() override;

    /**
     * @brief Returns whether the socket is bound and receiving.
     * @return true if active, false otherwise.
     */
    [[nodiscard]] bool isActive() const override;

    /**
     * @brief Returns the port the socket is bound to.
     * @return The port number, or 0 if not bound.
     */
    [[nodiscard]] quint16 port() const;

    /**
     * @brief Returns the detected video format.
     * @return The video format (PAL, NTSC, or Unknown).
     */
    [[nodiscard]] VideoFormat videoFormat() const { return videoFormat_; }

    /**
     * @brief Returns the current frame number.
     * @return The frame number from the most recent packet.
     */
    [[nodiscard]] quint16 currentFrameNumber() const { return currentFrameNum_; }

    /**
     * @brief Callback interface for diagnostics timing data.
     */
    struct DiagnosticsCallback
    {
        std::function<void(qint64 arrivalTimeUs)> onPacketReceived;
        std::function<void(quint16 frameNumber, qint64 startTimeUs)> onFrameStarted;
        std::function<void(quint16 frameNumber, qint64 endTimeUs, bool complete)> onFrameCompleted;
        std::function<void()> onOutOfOrderPacket;
    };

    /**
     * @brief Sets the diagnostics callback for timing data.
     * @param callback Callback structure with timing functions.
     *
     * When set, these callbacks are invoked during packet processing
     * to provide high-frequency timing data for diagnostics.
     */
    void setDiagnosticsCallback(const DiagnosticsCallback &callback);

signals:
    /**
     * @brief Emitted when a socket error occurs.
     * @param error Error description.
     */
    void socketError(const QString &error);

    /**
     * @brief Emitted periodically with reception statistics.
     * @param packetsReceived Total packets received.
     * @param framesCompleted Total frames completed.
     * @param packetsLost Estimated packets lost (sequence gaps).
     */
    void statsUpdated(quint64 packetsReceived, quint64 framesCompleted, quint64 packetsLost);

private slots:
    void onReadyRead();

private:
    void processPacket(const QByteArray &packet);
    void startNewFrame(quint16 frameNumber);
    void completeFrame();
    [[nodiscard]] VideoFormat detectFormat(const videostream::PacketHeader &header) const;

    // Network
    QUdpSocket *socket_ = nullptr;

    // Frame assembly
    QByteArray frameBuffer_;         // Current frame being assembled
    quint16 currentFrameNum_ = 0;    // Current frame number
    QSet<quint16> receivedPackets_;  // Line numbers of received packets
    int expectedPackets_ = 0;        // Expected packet count for current frame
    bool frameInProgress_ = false;   // Whether we're assembling a frame

    // Format detection
    VideoFormat videoFormat_ = VideoFormat::Unknown;

    // Statistics
    quint64 totalPacketsReceived_ = 0;
    quint64 totalFramesCompleted_ = 0;
    quint64 totalPacketsLost_ = 0;
    quint16 lastSequenceNumber_ = 0;
    bool firstPacket_ = true;

    // Diagnostics
    DiagnosticsCallback diagnosticsCallback_;
    QElapsedTimer diagnosticsTimer_;
    qint64 frameStartTimeUs_ = 0;
};

#endif  // VIDEOSTREAMRECEIVER_H
