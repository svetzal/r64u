#ifndef DEVICECONNECTION_H
#define DEVICECONNECTION_H

#include <QObject>
#include <QTimer>
#include "c64urestclient.h"
#include "c64uftpclient.h"

class DeviceConnection : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ConnectionState state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool connected READ isConnected NOTIFY stateChanged)

public:
    enum class ConnectionState {
        Disconnected,
        Connecting,
        Connected,
        Reconnecting
    };
    Q_ENUM(ConnectionState)

    explicit DeviceConnection(QObject *parent = nullptr);
    ~DeviceConnection() override;

    // Configuration
    void setHost(const QString &host);
    [[nodiscard]] QString host() const { return host_; }

    void setPassword(const QString &password);

    void setAutoReconnect(bool enabled);
    [[nodiscard]] bool autoReconnect() const { return autoReconnect_; }

    // State
    [[nodiscard]] ConnectionState state() const { return state_; }
    [[nodiscard]] bool isConnected() const { return state_ == ConnectionState::Connected; }

    // Device info (valid when connected)
    [[nodiscard]] DeviceInfo deviceInfo() const { return deviceInfo_; }
    [[nodiscard]] QList<DriveInfo> driveInfo() const { return driveInfo_; }

    // Access to underlying clients
    [[nodiscard]] C64URestClient* restClient() { return restClient_; }
    [[nodiscard]] C64UFtpClient* ftpClient() { return ftpClient_; }

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
    void setState(ConnectionState state);
    void checkBothConnected();
    void startReconnect();
    void stopReconnect();

    C64URestClient *restClient_ = nullptr;
    C64UFtpClient *ftpClient_ = nullptr;

    QString host_;
    QString password_;

    ConnectionState state_ = ConnectionState::Disconnected;
    bool autoReconnect_ = true;
    int reconnectAttempts_ = 0;
    static constexpr int MaxReconnectAttempts = 5;
    static constexpr int ReconnectIntervalMs = 3000;

    QTimer *reconnectTimer_ = nullptr;

    // Cached device info
    DeviceInfo deviceInfo_;
    QList<DriveInfo> driveInfo_;

    // Track connection state of both clients
    bool restConnected_ = false;
    bool ftpConnected_ = false;
    bool connectingInProgress_ = false;
};

#endif // DEVICECONNECTION_H
