/**
 * @file deviceconnection.h
 * @brief High-level connection manager for Ultimate 64/II+ devices.
 *
 * Provides unified connection management combining both REST and FTP
 * protocols with automatic reconnection support.
 */

#ifndef DEVICECONNECTION_H
#define DEVICECONNECTION_H

#include <QObject>
#include <QTimer>
#include "c64urestclient.h"
#include "c64uftpclient.h"

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
    /**
     * @brief Connection state of the device manager.
     */
    enum class ConnectionState {
        Disconnected,   ///< Not connected to any device
        Connecting,     ///< Connection in progress
        Connected,      ///< Both REST and FTP connections established
        Reconnecting    ///< Attempting to reconnect after connection loss
    };
    Q_ENUM(ConnectionState)

    /**
     * @brief Constructs a device connection manager.
     * @param parent Optional parent QObject for memory management.
     */
    explicit DeviceConnection(QObject *parent = nullptr);

    /**
     * @brief Destructor. Disconnects if connected.
     */
    ~DeviceConnection() override;

    /// @name Configuration
    /// @{

    /**
     * @brief Sets the target device host.
     * @param host Hostname or IP address of the Ultimate device.
     */
    void setHost(const QString &host);

    /**
     * @brief Returns the currently configured host.
     * @return The hostname or IP address.
     */
    [[nodiscard]] QString host() const { return host_; }

    /**
     * @brief Sets the authentication password.
     * @param password Password for device authentication.
     */
    void setPassword(const QString &password);

    /**
     * @brief Enables or disables automatic reconnection.
     * @param enabled True to enable auto-reconnect.
     */
    void setAutoReconnect(bool enabled);

    /**
     * @brief Returns whether auto-reconnect is enabled.
     * @return True if auto-reconnect is enabled.
     */
    [[nodiscard]] bool autoReconnect() const { return autoReconnect_; }
    /// @}

    /// @name Connection State
    /// @{

    /**
     * @brief Returns the current connection state.
     * @return The current ConnectionState enum value.
     */
    [[nodiscard]] ConnectionState state() const { return state_; }

    /**
     * @brief Checks if fully connected to the device.
     * @return True if both REST and FTP connections are established.
     */
    [[nodiscard]] bool isConnected() const { return state_ == ConnectionState::Connected; }
    /// @}

    /// @name Device Information
    /// @{

    /**
     * @brief Returns cached device information.
     * @return The DeviceInfo structure (valid when connected).
     */
    [[nodiscard]] DeviceInfo deviceInfo() const { return deviceInfo_; }

    /**
     * @brief Returns cached drive information.
     * @return List of DriveInfo structures (valid when connected).
     */
    [[nodiscard]] QList<DriveInfo> driveInfo() const { return driveInfo_; }
    /// @}

    /// @name Protocol Clients
    /// @{

    /**
     * @brief Returns the REST API client.
     * @return Pointer to the C64URestClient instance.
     */
    [[nodiscard]] C64URestClient* restClient() { return restClient_; }

    /**
     * @brief Returns the FTP client.
     * @return Pointer to the C64UFtpClient instance.
     */
    [[nodiscard]] C64UFtpClient* ftpClient() { return ftpClient_; }
    /// @}

public slots:
    /**
     * @brief Initiates connection to the configured device.
     *
     * Connects both REST and FTP protocols. Emits connected() when
     * both are established, or connectionError() on failure.
     */
    void connectToDevice();

    /**
     * @brief Disconnects from the device.
     *
     * Closes both REST and FTP connections.
     * Emits disconnected() when complete.
     */
    void disconnectFromDevice();

    /**
     * @brief Refreshes the cached device information.
     *
     * Emits deviceInfoUpdated() on success.
     */
    void refreshDeviceInfo();

    /**
     * @brief Refreshes the cached drive information.
     *
     * Emits driveInfoUpdated() on success.
     */
    void refreshDriveInfo();

signals:
    /// @name Connection Signals
    /// @{

    /**
     * @brief Emitted when the connection state changes.
     * @param state The new connection state.
     */
    void stateChanged(DeviceConnection::ConnectionState state);

    /**
     * @brief Emitted when fully connected to the device.
     */
    void connected();

    /**
     * @brief Emitted when disconnected from the device.
     */
    void disconnected();

    /**
     * @brief Emitted when a connection error occurs.
     * @param message Human-readable error description.
     */
    void connectionError(const QString &message);
    /// @}

    /// @name Information Signals
    /// @{

    /**
     * @brief Emitted when device information is updated.
     * @param info The updated device information.
     */
    void deviceInfoUpdated(const DeviceInfo &info);

    /**
     * @brief Emitted when drive information is updated.
     * @param drives The updated list of drive information.
     */
    void driveInfoUpdated(const QList<DriveInfo> &drives);
    /// @}

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
    void setState(ConnectionState state);
    void checkBothConnected();
    void startReconnect();
    void stopReconnect();

    // Protocol clients
    C64URestClient *restClient_ = nullptr;
    C64UFtpClient *ftpClient_ = nullptr;

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

#endif // DEVICECONNECTION_H
