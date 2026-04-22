#ifndef DEVICECONNECTION_H
#define DEVICECONNECTION_H

#include "iftpclient.h"
#include "irestclient.h"

#include <QObject>
#include <QTimer>

/**
 * @brief High-level connection manager for Ultimate 64/II+ devices.
 *
 * This class provides unified connection management for C64 Ultimate devices.
 * It manages both the REST API and FTP connections, treating them as a single
 * logical connection. Features include:
 * - Combined connection state for both protocols
 * - Automatic reconnection on connection loss
 * - Cached device and drive information
 * - Direct access to underlying REST and FTP clients
 *
 * @par Example usage:
 * @code
 * DeviceConnection *conn = new DeviceConnection(this);
 * conn->setHost("192.168.1.64");
 * conn->setPassword("admin");
 * conn->setAutoReconnect(true);
 *
 * connect(conn, &DeviceConnection::connected, this, &MyClass::onConnected);
 * connect(conn, &DeviceConnection::connectionError, this, &MyClass::onError);
 *
 * conn->connectToDevice();
 * @endcode
 */
class DeviceConnection : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ConnectionState state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool connected READ isConnected NOTIFY stateChanged)

public:
    enum class ConnectionState { Disconnected, Connecting, Connected, Reconnecting };
    Q_ENUM(ConnectionState)

    explicit DeviceConnection(QObject *parent = nullptr);

    /**
     * @brief Constructs a device connection manager with injected clients (for testing).
     * @param restClient Pre-created REST client (ownership transferred).
     * @param ftpClient Pre-created FTP client (ownership transferred).
     * @param parent Optional parent QObject for memory management.
     */
    DeviceConnection(IRestClient *restClient, IFtpClient *ftpClient, QObject *parent = nullptr);

    ~DeviceConnection() override;

    void setHost(const QString &host);
    [[nodiscard]] QString host() const { return host_; }
    void setPassword(const QString &password);
    void setAutoReconnect(bool enabled);
    [[nodiscard]] bool autoReconnect() const { return autoReconnect_; }

    [[nodiscard]] ConnectionState state() const { return state_; }
    [[nodiscard]] bool isConnected() const { return state_ == ConnectionState::Connected; }

    /**
     * @brief Checks if the REST API is reachable.
     * @return True if REST connection is established (regardless of FTP state).
     *
     * Useful for enabling actions that only require the REST API,
     * such as system control commands (reset, reboot, pause, etc.).
     */
    [[nodiscard]] bool isRestConnected() const { return restConnected_; }

    /**
     * @brief Checks if file operations can be performed.
     * @return True only when state is Connected.
     *
     * Use this instead of isConnected() for guards in UI operation handlers.
     * This explicitly prevents operations during Connecting or Reconnecting
     * states, avoiding race conditions and providing clearer user feedback.
     */
    [[nodiscard]] bool canPerformOperations() const { return state_ == ConnectionState::Connected; }

    [[nodiscard]] DeviceInfo deviceInfo() const { return deviceInfo_; }
    [[nodiscard]] QList<DriveInfo> driveInfo() const { return driveInfo_; }

    [[nodiscard]] IRestClient *restClient() { return restClient_; }
    [[nodiscard]] IFtpClient *ftpClient() { return ftpClient_; }

public slots:
    void connectToDevice();
    void disconnectFromDevice();
    void refreshDeviceInfo();
    void refreshDriveInfo();

signals:
    void stateChanged(DeviceConnection::ConnectionState state);
    void connected();
    void disconnected();
    void connectionError(const QString &message);

    void deviceInfoUpdated(const DeviceInfo &info);
    void driveInfoUpdated(const QList<DriveInfo> &drives);

private slots:
    void onRestInfoReceived(const DeviceInfo &info);
    void onRestDrivesReceived(const QList<DriveInfo> &drives);
    void onRestConnectionError(const QString &error);
    void onRestOperationFailed(const QString &operation, const QString &error);

    void onFtpConnected();
    void onFtpDisconnected();
    void onFtpError(const QString &message);

    void onReconnectTimer();

private:
    /**
     * @brief Attempts to transition to a new state with guards.
     * @param newState The desired new state.
     * @return True if the transition was valid and completed, false otherwise.
     *
     * Valid state transitions:
     * - Disconnected -> Connecting (via connectToDevice)
     * - Connecting -> Connected (when both protocols succeed)
     * - Connecting -> Disconnected (on failure or cancel)
     * - Connecting -> Reconnecting (if auto-reconnect during initial connect fails)
     * - Connected -> Disconnected (via disconnectFromDevice)
     * - Connected -> Reconnecting (on connection loss with auto-reconnect)
     * - Reconnecting -> Connecting (when reconnect attempt starts)
     * - Reconnecting -> Connected (when reconnect succeeds)
     * - Reconnecting -> Disconnected (on max attempts or cancel)
     */
    bool tryTransitionTo(ConnectionState newState);

    /**
     * @brief Checks if a state transition is valid.
     * @param from Current state.
     * @param to Desired state.
     * @return True if the transition is allowed.
     */
    [[nodiscard]] static bool isValidTransition(ConnectionState from, ConnectionState to);

    void resetProtocolFlags();
    void checkBothConnected();
    void startReconnect();
    void stopReconnect();

    // Protocol clients
    IRestClient *restClient_ = nullptr;
    IFtpClient *ftpClient_ = nullptr;

    // Configuration
    QString host_;
    QString password_;

    // Connection state
    ConnectionState state_ = ConnectionState::Disconnected;
    bool restConnected_ = false;
    bool ftpConnected_ = false;
    bool connectingInProgress_ = false;

    // Reconnection
    bool autoReconnect_ = true;
    int reconnectAttempts_ = 0;
    static constexpr int MaxReconnectAttempts = 5;
    static constexpr int ReconnectIntervalMs = 3000;
    QTimer *reconnectTimer_ = nullptr;

    // Cached device info
    DeviceInfo deviceInfo_;
    QList<DriveInfo> driveInfo_;
};

#endif  // DEVICECONNECTION_H
