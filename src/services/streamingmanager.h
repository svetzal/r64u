#ifndef STREAMINGMANAGER_H
#define STREAMINGMANAGER_H

#include <QObject>
#include <QString>

class DeviceConnection;
class IStreamControlClient;
class IVideoStreamReceiver;
class IAudioStreamReceiver;
class IAudioPlaybackService;
class INetworkInterfaceProvider;
class VideoStreamReceiver;
class AudioStreamReceiver;
class KeyboardInputService;
class StreamingDiagnostics;

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
 * Dependencies are injected via the constructor for testability.
 * Use the @c createDefault() factory method for production use.
 *
 * @par Example usage (production):
 * @code
 * StreamingManager *manager = StreamingManager::createDefault(deviceConnection, this);
 *
 * connect(manager, &StreamingManager::streamingStarted,
 *         this, &MyClass::onStreamingStarted);
 *
 * manager->startStreaming();
 * @endcode
 */
class StreamingManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a streaming manager with injected dependencies.
     *
     * All pointer dependencies are non-owned (caller retains ownership unless
     * they have a Qt parent set that would cause deletion). The concrete types
     * for @p videoReceiver and @p audioReceiver are also accessible via the
     * typed accessors for signal/slot connections in UI code.
     *
     * @param connection Device connection for REST client access.
     * @param streamControl Stream control client (owned by caller or Qt parent).
     * @param videoReceiver Video stream receiver (owned by caller or Qt parent).
     * @param audioReceiver Audio stream receiver (owned by caller or Qt parent).
     * @param audioPlayback Audio playback service (owned by caller or Qt parent).
     * @param keyboardInput Keyboard input service (owned by caller or Qt parent).
     * @param networkProvider Network interface provider (owned by caller).
     * @param parent Optional parent QObject for memory management.
     */
    explicit StreamingManager(DeviceConnection *connection, IStreamControlClient *streamControl,
                              IVideoStreamReceiver *videoReceiver,
                              IAudioStreamReceiver *audioReceiver,
                              IAudioPlaybackService *audioPlayback,
                              KeyboardInputService *keyboardInput,
                              INetworkInterfaceProvider *networkProvider,
                              QObject *parent = nullptr);

    /**
     * @brief Factory method that creates a StreamingManager with production dependencies.
     *
     * Creates concrete implementations (StreamControlClient, VideoStreamReceiver, etc.)
     * and wires them together. This is the preferred way to create a StreamingManager
     * in production code.
     *
     * @param connection Device connection for network and REST client access.
     * @param parent Optional parent QObject for memory management.
     * @return A fully configured StreamingManager owning its dependencies.
     */
    static StreamingManager *createDefault(DeviceConnection *connection, QObject *parent = nullptr);

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
     * @brief Returns the video receiver for UI signal connections.
     * @return Pointer to the concrete VideoStreamReceiver (may be nullptr in tests).
     */
    [[nodiscard]] VideoStreamReceiver *videoReceiver() const { return concreteVideoReceiver_; }

    /**
     * @brief Returns the keyboard input service for UI connection.
     * @return Pointer to the keyboard input service.
     */
    [[nodiscard]] KeyboardInputService *keyboardInput() const { return keyboardInput_; }

    /**
     * @brief Returns the audio receiver for recording support.
     * @return Pointer to the concrete AudioStreamReceiver (may be nullptr in tests).
     */
    [[nodiscard]] AudioStreamReceiver *audioReceiver() const { return concreteAudioReceiver_; }

    /**
     * @brief Returns the streaming diagnostics service.
     * @return Pointer to the diagnostics service.
     */
    [[nodiscard]] StreamingDiagnostics *diagnostics() const { return diagnostics_; }

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
    [[nodiscard]] QString findLocalHostForDevice() const;

    // Non-owned dependency
    DeviceConnection *deviceConnection_ = nullptr;

    // Injected streaming service interfaces (not owned)
    IStreamControlClient *streamControl_ = nullptr;
    IVideoStreamReceiver *videoReceiver_ = nullptr;
    IAudioStreamReceiver *audioReceiver_ = nullptr;
    IAudioPlaybackService *audioPlayback_ = nullptr;
    KeyboardInputService *keyboardInput_ = nullptr;
    INetworkInterfaceProvider *networkProvider_ = nullptr;

    // Concrete typed pointers (may be nullptr when using mocks in tests)
    VideoStreamReceiver *concreteVideoReceiver_ = nullptr;
    AudioStreamReceiver *concreteAudioReceiver_ = nullptr;

    // Owned streaming services (created by createDefault)
    StreamingDiagnostics *diagnostics_ = nullptr;

    // State
    bool isStreaming_ = false;
    QString currentTargetHost_;
};

#endif  // STREAMINGMANAGER_H
