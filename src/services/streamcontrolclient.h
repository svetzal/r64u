/**
 * @file streamcontrolclient.h
 * @brief TCP client for controlling video/audio streaming on Ultimate 64/II+ devices.
 *
 * Manages the TCP connection to port 64 for starting and stopping
 * video and audio streams from the C64 Ultimate device.
 */

#ifndef STREAMCONTROLCLIENT_H
#define STREAMCONTROLCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QHostAddress>

/**
 * @brief TCP client for controlling C64 Ultimate video/audio streams.
 *
 * This class manages the TCP connection to port 64 on the C64 Ultimate device
 * and sends commands to start/stop video and audio streaming. The protocol uses
 * binary commands with the following format:
 *
 * Start stream: [cmd] 0xFF [len_lo] [len_hi] [duration_lo] [duration_hi] [IP:PORT]
 * Stop stream:  [cmd] 0xFF 0x00 0x00
 *
 * Where cmd is:
 * - 0x20: Start video stream
 * - 0x21: Start audio stream
 * - 0x30: Stop video stream
 * - 0x31: Stop audio stream
 *
 * @par Example usage:
 * @code
 * StreamControlClient *client = new StreamControlClient(this);
 * client->setHost("192.168.1.64");
 *
 * connect(client, &StreamControlClient::commandSucceeded, this, &MyClass::onSuccess);
 * connect(client, &StreamControlClient::commandFailed, this, &MyClass::onError);
 *
 * client->startVideoStream("192.168.1.100", 21000);
 * @endcode
 */
class StreamControlClient : public QObject
{
    Q_OBJECT

public:
    /// TCP port used for stream control on C64 Ultimate devices
    static constexpr quint16 ControlPort = 64;

    /// Default video stream port
    static constexpr quint16 DefaultVideoPort = 21000;

    /// Default audio stream port
    static constexpr quint16 DefaultAudioPort = 21001;

    /**
     * @brief Constructs a stream control client.
     * @param parent Optional parent QObject for memory management.
     */
    explicit StreamControlClient(QObject *parent = nullptr);

    /**
     * @brief Destructor. Closes any open connection.
     */
    ~StreamControlClient() override;

    /**
     * @brief Sets the target C64 Ultimate host.
     * @param host Hostname or IP address of the device.
     */
    void setHost(const QString &host);

    /**
     * @brief Returns the currently configured host.
     * @return The hostname or IP address.
     */
    [[nodiscard]] QString host() const { return host_; }

    /// @name Stream Control
    /// @{

    /**
     * @brief Starts the video stream to the specified target.
     * @param targetHost IP address to receive the video stream.
     * @param targetPort UDP port to receive the video stream (default: 21000).
     * @param durationTicks Stream duration in 5ms ticks (0 = infinite).
     */
    void startVideoStream(const QString &targetHost,
                          quint16 targetPort = DefaultVideoPort,
                          quint16 durationTicks = 0);

    /**
     * @brief Starts the audio stream to the specified target.
     * @param targetHost IP address to receive the audio stream.
     * @param targetPort UDP port to receive the audio stream (default: 21001).
     * @param durationTicks Stream duration in 5ms ticks (0 = infinite).
     */
    void startAudioStream(const QString &targetHost,
                          quint16 targetPort = DefaultAudioPort,
                          quint16 durationTicks = 0);

    /**
     * @brief Stops the video stream.
     */
    void stopVideoStream();

    /**
     * @brief Stops the audio stream.
     */
    void stopAudioStream();

    /**
     * @brief Starts both video and audio streams to the specified target.
     * @param targetHost IP address to receive the streams.
     * @param videoPort UDP port for video (default: 21000).
     * @param audioPort UDP port for audio (default: 21001).
     */
    void startAllStreams(const QString &targetHost,
                         quint16 videoPort = DefaultVideoPort,
                         quint16 audioPort = DefaultAudioPort);

    /**
     * @brief Stops both video and audio streams.
     */
    void stopAllStreams();

    /**
     * @brief Clears any pending commands without sending them.
     */
    void clearPendingCommands();
    /// @}

signals:
    /**
     * @brief Emitted when a command succeeds.
     * @param command Description of the command that succeeded.
     */
    void commandSucceeded(const QString &command);

    /**
     * @brief Emitted when a command fails.
     * @param command Description of the command that failed.
     * @param error Error description.
     */
    void commandFailed(const QString &command, const QString &error);

    /**
     * @brief Emitted when a connection error occurs.
     * @param error Error description.
     */
    void connectionError(const QString &error);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    /// Command types for the control protocol
    enum class CommandType : quint8 {
        StartVideo = 0x20,
        StartAudio = 0x21,
        StopVideo = 0x30,
        StopAudio = 0x31
    };

    /// Pending command structure
    struct PendingCommand {
        CommandType type;
        QString description;
        QByteArray data;
    };

    void sendCommand(const PendingCommand &command);
    void connectAndSend();
    [[nodiscard]] QByteArray buildStartCommand(CommandType type,
                                                const QString &targetHost,
                                                quint16 targetPort,
                                                quint16 durationTicks) const;
    [[nodiscard]] QByteArray buildStopCommand(CommandType type) const;
    [[nodiscard]] QString commandTypeToString(CommandType type) const;

    // Network
    QTcpSocket *socket_ = nullptr;

    // Configuration
    QString host_;

    // Command queue
    QList<PendingCommand> pendingCommands_;
    bool connecting_ = false;
};

#endif // STREAMCONTROLCLIENT_H
