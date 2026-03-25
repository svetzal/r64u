/**
 * @file ivideostreamreceiver.h
 * @brief Gateway interface for video stream receiver implementations.
 *
 * Abstracts UDP video reception so that StreamingManager can be tested
 * with a mock instead of requiring real network sockets.
 */

#ifndef IVIDEOSTREAMRECEIVER_H
#define IVIDEOSTREAMRECEIVER_H

#include <QByteArray>
#include <QObject>

/**
 * @brief Abstract interface for receiving C64 Ultimate video stream packets.
 *
 * The VideoFormat enum is defined here so that both the interface and
 * callers (viewpanel.cpp signal connections) can use a shared type.
 */
class IVideoStreamReceiver : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Video format enumeration.
     */
    enum class VideoFormat { Unknown, PAL, NTSC };
    Q_ENUM(VideoFormat)

    explicit IVideoStreamReceiver(QObject *parent = nullptr) : QObject(parent) {}
    ~IVideoStreamReceiver() override = default;

    /**
     * @brief Binds the UDP socket to the default video port.
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

signals:
    /**
     * @brief Emitted when a complete video frame is ready.
     * @param frameData Raw frame bytes.
     * @param frameNumber Frame sequence number.
     * @param format Detected video format.
     */
    void frameReady(const QByteArray &frameData, quint16 frameNumber, VideoFormat format);

    /**
     * @brief Emitted when the video format is first detected.
     * @param format Detected video format.
     */
    void formatDetected(VideoFormat format);
};

#endif  // IVIDEOSTREAMRECEIVER_H
