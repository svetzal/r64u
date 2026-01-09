/**
 * @file videostreamreceiver.h
 * @brief UDP receiver for video stream packets from Ultimate 64/II+ devices.
 *
 * Receives and reassembles video frames from UDP packets sent by the
 * C64 Ultimate device's video streaming feature.
 */

#ifndef VIDEOSTREAMRECEIVER_H
#define VIDEOSTREAMRECEIVER_H

#include <QObject>
#include <QUdpSocket>
#include <QByteArray>
#include <QVector>
#include <QSet>

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
class VideoStreamReceiver : public QObject
{
    Q_OBJECT

public:
    /// Default UDP port for video stream reception
    static constexpr quint16 DefaultPort = 21000;

    /// Video packet size in bytes (12-byte header + 768-byte payload)
    static constexpr int PacketSize = 780;

    /// Header size in bytes
    static constexpr int HeaderSize = 12;

    /// Payload size in bytes (4 lines × 192 bytes per line)
    static constexpr int PayloadSize = 768;

    /// Pixels per line in the video stream
    static constexpr int PixelsPerLine = 384;

    /// Lines per packet
    static constexpr int LinesPerPacket = 4;

    /// Bits per pixel (VIC-II uses 4-bit color indices)
    static constexpr int BitsPerPixel = 4;

    /// Bytes per line (384 pixels × 4 bits / 8)
    static constexpr int BytesPerLine = 192;

    /// Maximum frame height (PAL)
    static constexpr int MaxFrameHeight = 272;

    /// PAL frame height
    static constexpr int PalHeight = 272;

    /// NTSC frame height
    static constexpr int NtscHeight = 240;

    /// PAL packets per frame (272 lines / 4 lines per packet)
    static constexpr int PalPacketsPerFrame = 68;

    /// NTSC packets per frame (240 lines / 4 lines per packet)
    static constexpr int NtscPacketsPerFrame = 60;

    /**
     * @brief Video format enumeration.
     */
    enum class VideoFormat {
        Unknown,
        PAL,
        NTSC
    };

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
     * @brief Binds the UDP socket to the specified port.
     * @param port UDP port number to listen on (default: 21000).
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
     * @brief Returns the detected video format.
     * @return The video format (PAL, NTSC, or Unknown).
     */
    [[nodiscard]] VideoFormat videoFormat() const { return videoFormat_; }

    /**
     * @brief Returns the current frame number.
     * @return The frame number from the most recent packet.
     */
    [[nodiscard]] quint16 currentFrameNumber() const { return currentFrameNum_; }

signals:
    /**
     * @brief Emitted when a complete frame has been assembled.
     * @param frameData Raw 4-bit indexed color data (384 × height bytes packed).
     * @param frameNumber The frame sequence number.
     * @param format The detected video format (PAL or NTSC).
     */
    void frameReady(const QByteArray &frameData, quint16 frameNumber, VideoFormat format);

    /**
     * @brief Emitted when the video format is detected or changes.
     * @param format The detected video format.
     */
    void formatDetected(VideoFormat format);

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
    /**
     * @brief Video packet header structure.
     */
    struct PacketHeader {
        quint16 sequenceNumber;
        quint16 frameNumber;
        quint16 lineNumber;     // Bits 0-14: line num, Bit 15: last packet flag
        quint16 pixelsPerLine;
        quint8 linesPerPacket;
        quint8 bitsPerPixel;
        quint16 encodingType;

        [[nodiscard]] bool isLastPacket() const { return (lineNumber & 0x8000) != 0; }
        [[nodiscard]] quint16 actualLineNumber() const { return lineNumber & 0x7FFF; }
    };

    void processPacket(const QByteArray &packet);
    [[nodiscard]] PacketHeader parseHeader(const QByteArray &packet) const;
    void startNewFrame(quint16 frameNumber);
    void completeFrame();
    [[nodiscard]] VideoFormat detectFormat(const PacketHeader &header) const;

    // Network
    QUdpSocket *socket_ = nullptr;

    // Frame assembly
    QByteArray frameBuffer_;           // Current frame being assembled
    quint16 currentFrameNum_ = 0;      // Current frame number
    QSet<quint16> receivedPackets_;    // Line numbers of received packets
    int expectedPackets_ = 0;          // Expected packet count for current frame
    bool frameInProgress_ = false;     // Whether we're assembling a frame

    // Format detection
    VideoFormat videoFormat_ = VideoFormat::Unknown;

    // Statistics
    quint64 totalPacketsReceived_ = 0;
    quint64 totalFramesCompleted_ = 0;
    quint64 totalPacketsLost_ = 0;
    quint16 lastSequenceNumber_ = 0;
    bool firstPacket_ = true;
};

#endif // VIDEOSTREAMRECEIVER_H
