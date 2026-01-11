/**
 * @file mockrestclient.h
 * @brief Mock REST client for testing DeviceConnection.
 *
 * This mock provides the same signal interface as C64URestClient
 * without any network activity.
 */

#ifndef MOCKRESTCLIENT_H
#define MOCKRESTCLIENT_H

#include <QObject>
#include <QString>
#include <QList>

// Forward declare the structs from c64urestclient.h
struct DeviceInfo {
    QString product;
    QString firmwareVersion;
    QString fpgaVersion;
    QString coreVersion;
    QString hostname;
    QString uniqueId;
    QString apiVersion;
};

struct DriveInfo {
    QString name;
    bool enabled = false;
    int busId = 0;
    QString type;
    QString rom;
    QString imageFile;
    QString imagePath;
    QString lastError;
};

/**
 * @brief Mock REST client for testing DeviceConnection.
 *
 * Provides the same signals as C64URestClient and allows
 * controlled emission of those signals for testing state machine behavior.
 */
class MockRestClient : public QObject
{
    Q_OBJECT

public:
    explicit MockRestClient(QObject *parent = nullptr);
    ~MockRestClient() override = default;

    // Methods that match C64URestClient interface
    void setHost(const QString &host) { host_ = host; }
    QString host() const { return host_; }
    void setPassword(const QString &password) { password_ = password; }

    void getInfo();
    void getDrives();

    /// @name Mock Control Methods
    /// @{

    /**
     * @brief Simulates successful device info response.
     */
    void mockEmitInfoReceived(const DeviceInfo &info);

    /**
     * @brief Simulates successful drives response.
     */
    void mockEmitDrivesReceived(const QList<DriveInfo> &drives);

    /**
     * @brief Simulates connection error.
     */
    void mockEmitConnectionError(const QString &error);

    /**
     * @brief Simulates operation failure.
     */
    void mockEmitOperationFailed(const QString &operation, const QString &error);

    /**
     * @brief Resets mock state.
     */
    void mockReset();

    int mockGetInfoCallCount() const { return getInfoCalls_; }
    int mockGetDrivesCallCount() const { return getDrivesCalls_; }
    /// @}

signals:
    // Signals matching C64URestClient
    void infoReceived(const DeviceInfo &info);
    void drivesReceived(const QList<DriveInfo> &drives);
    void connectionError(const QString &error);
    void operationFailed(const QString &operation, const QString &error);
    void operationSucceeded(const QString &operation);

private:
    QString host_;
    QString password_;
    int getInfoCalls_ = 0;
    int getDrivesCalls_ = 0;
};

#endif // MOCKRESTCLIENT_H
