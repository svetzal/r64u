#ifndef C64URESTCLIENT_H
#define C64URESTCLIENT_H

#include "irestclient.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

/**
 * @brief REST API client for Ultimate 64/II+ devices.
 *
 * This class provides a Qt-based HTTP client for the Ultimate device
 * REST API. It supports:
 * - Machine control (reset, reboot, pause, power off)
 * - Drive management (mount, unmount, create disk images)
 * - Content playback (SID, MOD, PRG, CRT files)
 * - Configuration management
 * - Device information queries
 *
 * All operations are asynchronous with results delivered via Qt signals
 * inherited from IRestClient.
 *
 * @par Example usage:
 * @code
 * IRestClient *rest = new C64URestClient(this);
 * rest->setHost("192.168.1.64");
 *
 * connect(rest, &IRestClient::infoReceived, this, &MyClass::onInfo);
 * connect(rest, &IRestClient::operationFailed, this, &MyClass::onError);
 *
 * rest->getInfo();
 * @endcode
 */
class C64URestClient : public IRestClient
{
    Q_OBJECT

public:
    /// Maximum characters to include from error response body
    static constexpr int ErrorResponsePreviewLength = 200;
    /// Request timeout in milliseconds
    static constexpr int RequestTimeoutMs = 15000;

    explicit C64URestClient(QObject *parent = nullptr);
    ~C64URestClient() override;

    void setHost(const QString &host) override;
    [[nodiscard]] QString host() const override { return host_; }
    void setPassword(const QString &password) override;
    [[nodiscard]] bool hasPassword() const override { return !password_.isEmpty(); }

    void getVersion() override;
    void getInfo() override;

    void playSid(const QString &filePath, int songNumber = -1) override;
    void playMod(const QString &filePath) override;
    void loadPrg(const QString &filePath) override;
    void runPrg(const QString &filePath) override;
    void runCrt(const QString &filePath) override;

    void getDrives() override;
    void mountImage(const QString &drive, const QString &imagePath,
                    const QString &mode = "readwrite") override;
    void unmountImage(const QString &drive) override;
    void resetDrive(const QString &drive) override;

    void resetMachine() override;
    void rebootMachine() override;
    void pauseMachine() override;
    void resumeMachine() override;
    void powerOffMachine() override;
    void pressMenuButton() override;
    void writeMem(const QString &address, const QByteArray &data) override;

    /**
     * @brief Types text into the C64 via keyboard buffer injection.
     * @param text Text to type (ASCII, converted to PETSCII).
     *
     * Writes to the C64 keyboard buffer at $0277 and sets the
     * character count at $C6. Limited to 10 characters at a time.
     * For longer text, call multiple times with delays.
     */
    void typeText(const QString &text) override;

    void getFileInfo(const QString &path) override;
    void createD64(const QString &path, const QString &diskName = QString(),
                   int tracks = 35) override;
    void createD81(const QString &path, const QString &diskName = QString()) override;

    void getConfigCategories() override;
    void getConfigCategoryItems(const QString &category) override;
    void getConfigItem(const QString &category, const QString &item) override;
    void setConfigItem(const QString &category, const QString &item,
                       const QVariant &value) override;
    void updateConfigsBatch(const QJsonObject &configs) override;

    /**
     * @brief Saves configuration to flash.
     *
     * Persists current settings to non-volatile storage.
     * Emits configSavedToFlash() on success.
     */
    void saveConfigToFlash() override;

    /**
     * @brief Loads configuration from flash.
     *
     * Reverts to last saved settings from non-volatile storage.
     * Emits configLoadedFromFlash() on success.
     */
    void loadConfigFromFlash() override;

    void resetConfigToDefaults() override;

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    [[nodiscard]] QNetworkRequest createRequest(const QString &endpoint) const;
    void sendGetRequest(const QString &endpoint, const QString &operation);
    void sendPutRequest(const QString &endpoint, const QString &operation,
                        const QByteArray &data = QByteArray());
    void sendPostRequest(const QString &endpoint, const QString &operation, const QByteArray &data,
                         const QString &contentType = "application/octet-stream");

    void handleGenericResponse(const QString &operation, const QJsonObject &json);

    // Network
    QNetworkAccessManager *networkManager_ = nullptr;

    // Configuration
    QString host_;
    QString password_;

    // Operation tracking
    QHash<QNetworkReply *, QString> pendingOperations_;
};

#endif  // C64URESTCLIENT_H
