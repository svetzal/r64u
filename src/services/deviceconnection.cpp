#include "deviceconnection.h"

DeviceConnection::DeviceConnection(QObject *parent)
    : QObject(parent)
    , restClient_(new C64URestClient(this))
    , ftpClient_(new C64UFtpClient(this))
    , reconnectTimer_(new QTimer(this))
{
    // REST client signals
    connect(restClient_, &C64URestClient::infoReceived,
            this, &DeviceConnection::onRestInfoReceived);
    connect(restClient_, &C64URestClient::drivesReceived,
            this, &DeviceConnection::onRestDrivesReceived);
    connect(restClient_, &C64URestClient::connectionError,
            this, &DeviceConnection::onRestConnectionError);
    connect(restClient_, &C64URestClient::operationFailed,
            this, &DeviceConnection::onRestOperationFailed);

    // FTP client signals
    connect(ftpClient_, &C64UFtpClient::connected,
            this, &DeviceConnection::onFtpConnected);
    connect(ftpClient_, &C64UFtpClient::disconnected,
            this, &DeviceConnection::onFtpDisconnected);
    connect(ftpClient_, &C64UFtpClient::error,
            this, &DeviceConnection::onFtpError);

    // Reconnect timer
    reconnectTimer_->setSingleShot(true);
    connect(reconnectTimer_, &QTimer::timeout,
            this, &DeviceConnection::onReconnectTimer);
}

DeviceConnection::~DeviceConnection() = default;

void DeviceConnection::setHost(const QString &host)
{
    host_ = host;
    restClient_->setHost(host);
    ftpClient_->setHost(host);
}

void DeviceConnection::setPassword(const QString &password)
{
    password_ = password;
    restClient_->setPassword(password);
    // FTP typically uses anonymous access on Ultimate devices
}

void DeviceConnection::setAutoReconnect(bool enabled)
{
    autoReconnect_ = enabled;
    if (!enabled) {
        stopReconnect();
    }
}

void DeviceConnection::setState(ConnectionState state)
{
    if (state_ != state) {
        state_ = state;
        emit stateChanged(state);
    }
}

void DeviceConnection::connectToDevice()
{
    if (state_ == ConnectionState::Connecting ||
        state_ == ConnectionState::Connected) {
        return;
    }

    if (host_.isEmpty()) {
        emit connectionError(tr("No host configured"));
        return;
    }

    stopReconnect();
    reconnectAttempts_ = 0;

    setState(ConnectionState::Connecting);
    connectingInProgress_ = true;
    restConnected_ = false;
    ftpConnected_ = false;

    // Start REST connection by fetching device info
    restClient_->getInfo();

    // Start FTP connection
    ftpClient_->connectToHost();
}

void DeviceConnection::disconnectFromDevice()
{
    stopReconnect();
    connectingInProgress_ = false;

    ftpClient_->disconnect();

    restConnected_ = false;
    ftpConnected_ = false;
    deviceInfo_ = DeviceInfo();
    driveInfo_.clear();

    setState(ConnectionState::Disconnected);
    emit disconnected();
}

void DeviceConnection::refreshDeviceInfo()
{
    if (state_ == ConnectionState::Connected) {
        restClient_->getInfo();
    }
}

void DeviceConnection::refreshDriveInfo()
{
    if (state_ == ConnectionState::Connected) {
        restClient_->getDrives();
    }
}

void DeviceConnection::onRestInfoReceived(const DeviceInfo &info)
{
    deviceInfo_ = info;
    emit deviceInfoUpdated(info);

    if (connectingInProgress_) {
        restConnected_ = true;
        checkBothConnected();
    }

    // Also fetch drive info
    restClient_->getDrives();
}

void DeviceConnection::onRestDrivesReceived(const QList<DriveInfo> &drives)
{
    driveInfo_ = drives;
    emit driveInfoUpdated(drives);
}

void DeviceConnection::onRestConnectionError(const QString &error)
{
    if (connectingInProgress_) {
        connectingInProgress_ = false;
        ftpClient_->disconnect();

        if (state_ == ConnectionState::Reconnecting) {
            startReconnect();
        } else {
            setState(ConnectionState::Disconnected);
            emit connectionError(tr("REST connection failed: %1").arg(error));
        }
    } else if (state_ == ConnectionState::Connected && autoReconnect_) {
        // Connection lost, try to reconnect
        setState(ConnectionState::Reconnecting);
        startReconnect();
    }
}

void DeviceConnection::onRestOperationFailed(const QString &operation, const QString &error)
{
    // If this was the initial info request during connection
    if (connectingInProgress_ && operation == "info") {
        onRestConnectionError(error);
    }
}

void DeviceConnection::onFtpConnected()
{
    if (connectingInProgress_) {
        ftpConnected_ = true;
        checkBothConnected();
    }
}

void DeviceConnection::onFtpDisconnected()
{
    if (state_ == ConnectionState::Connected && autoReconnect_) {
        setState(ConnectionState::Reconnecting);
        startReconnect();
    }
}

void DeviceConnection::onFtpError(const QString &message)
{
    if (connectingInProgress_) {
        connectingInProgress_ = false;

        if (state_ == ConnectionState::Reconnecting) {
            startReconnect();
        } else {
            setState(ConnectionState::Disconnected);
            emit connectionError(tr("FTP connection failed: %1").arg(message));
        }
    }
}

void DeviceConnection::checkBothConnected()
{
    if (restConnected_ && ftpConnected_) {
        connectingInProgress_ = false;
        reconnectAttempts_ = 0;
        setState(ConnectionState::Connected);
        emit connected();
    }
}

void DeviceConnection::startReconnect()
{
    if (!autoReconnect_) {
        setState(ConnectionState::Disconnected);
        return;
    }

    reconnectAttempts_++;

    if (reconnectAttempts_ > MaxReconnectAttempts) {
        setState(ConnectionState::Disconnected);
        emit connectionError(tr("Failed to reconnect after %1 attempts")
                             .arg(MaxReconnectAttempts));
        return;
    }

    setState(ConnectionState::Reconnecting);
    reconnectTimer_->start(ReconnectIntervalMs);
}

void DeviceConnection::stopReconnect()
{
    reconnectTimer_->stop();
}

void DeviceConnection::onReconnectTimer()
{
    if (state_ != ConnectionState::Reconnecting) {
        return;
    }

    connectingInProgress_ = true;
    restConnected_ = false;
    ftpConnected_ = false;

    restClient_->getInfo();
    ftpClient_->connectToHost();
}
