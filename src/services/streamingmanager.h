/**
 * @file streamingmanager.h
 * @brief High-level manager for C64 Ultimate video and audio streaming.
 *
 * Manages the complete streaming lifecycle including stream control,
 * video reception, audio reception, audio playback, and keyboard input.
 */

#ifndef STREAMINGMANAGER_H
#define STREAMINGMANAGER_H

#include <QObject>
#include <QString>

class DeviceConnection;
class StreamControlClient;
class VideoStreamReceiver;
class AudioStreamReceiver;
class AudioPlaybackService;
class KeyboardInputService;

/**
 * @brief Manager for C64 Ultimate streaming services.
 *
 * This class coordinates all streaming-related services:
 * - Stream control (start/stop commands)
 * - Video stream reception
 * - Audio stream reception
 * - Audio playback
 * - Keyboard input
 *
 * It provides a clean interface for starting and stopping streaming,
 * and exposes signals for UI updates.
 *
 * @par Example usage:
 * @code
 * StreamingManager *manager = new StreamingManager(deviceConnection, this);
 *
 * connect(manager, &StreamingManager::streamingStarted,
 *         this, &MyClass::onStreamingStarted);
 * connect(manager, &StreamingManager::videoFormatDetected,
 *         this, &MyClass::onVideoFormatChanged);
 *
 * manager->startStreaming();
 * @endcode
 */
class StreamingManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a streaming manager.
     * @param connection Device connection for network and REST client access.
     * @param parent Optional parent QObject for memory management.
     */
    explicit StreamingManager(DeviceConnection *connection, QObject *parent = nullptr);

    /**
     * @brief Destructor. Stops streaming and cleans up resources.
     */
    ~StreamingManager() override;

    /**
     * @brief Returns whether streaming is currently active.
     * @return true if streaming, false otherwise.
     */
    [[nodiscard]] bool isStreaming() const { return isStreaming_; }

    /**
     * @brief Starts video and audio streaming.
     *
     * Binds UDP receivers, starts audio playback, and sends stream
     * start commands to the device.
     *
     * @return true if streaming started successfully, false on error.
     */
    bool startStreaming();

    /**
     * @brief Stops video and audio streaming.
     *
     * Sends stop commands, closes receivers, and stops audio playback.
     */
    void stopStreaming();

    /**
     * @brief Returns the video receiver for UI connection.
     * @return Pointer to the video receiver.
     */
    [[nodiscard]] VideoStreamReceiver* videoReceiver() const { return videoReceiver_; }

    /**
     * @brief Returns the keyboard input service for UI connection.
     * @return Pointer to the keyboard input service.
     */
    [[nodiscard]] KeyboardInputService* keyboardInput() const { return keyboardInput_; }

    /**
     * @brief Returns the audio receiver for recording support.
     * @return Pointer to the audio receiver.
     */
    [[nodiscard]] AudioStreamReceiver* audioReceiver() const { return audioReceiver_; }

signals:
    /**
     * @brief Emitted when streaming has started successfully.
     * @param targetHost The local IP address receiving the stream.
     */
    void streamingStarted(const QString &targetHost);

    /**
     * @brief Emitted when streaming has stopped.
     */
    void streamingStopped();

    /**
     * @brief Emitted when video format is detected.
     * @param format Format identifier (0=unknown, 1=PAL, 2=NTSC).
     */
    void videoFormatDetected(int format);

    /**
     * @brief Emitted on streaming errors.
     * @param error Error description.
     */
    void error(const QString &error);

    /**
     * @brief Emitted for status messages.
     * @param message Status message text.
     * @param timeout Display timeout in milliseconds (0 = permanent).
     */
    void statusMessage(const QString &message, int timeout = 0);

private slots:
    void onVideoFormatDetected(int format);
    void onStreamCommandSucceeded(const QString &command);
    void onStreamCommandFailed(const QString &command, const QString &error);

private:
    QString findLocalHostForDevice() const;

    // Dependencies (not owned)
    DeviceConnection *deviceConnection_ = nullptr;

    // Owned streaming services
    StreamControlClient *streamControl_ = nullptr;
    VideoStreamReceiver *videoReceiver_ = nullptr;
    AudioStreamReceiver *audioReceiver_ = nullptr;
    AudioPlaybackService *audioPlayback_ = nullptr;
    KeyboardInputService *keyboardInput_ = nullptr;

    // State
    bool isStreaming_ = false;
    QString currentTargetHost_;
};

#endif // STREAMINGMANAGER_H
