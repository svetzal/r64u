/**
 * @file c64urestclient.h
 * @brief REST API client for communicating with Ultimate 64/II+ devices.
 *
 * Provides HTTP-based control of C64 Ultimate devices including machine
 * control, drive operations, and content playback.
 */

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

    /**
     * @brief Constructs a REST client.
     * @param parent Optional parent QObject for memory management.
     */
    explicit C64URestClient(QObject *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~C64URestClient() override;

    /**
     * @brief Sets the target host.
     * @param host Hostname or IP address of the Ultimate device.
     */
    void setHost(const QString &host) override;

    /**
     * @brief Returns the currently configured host.
     * @return The hostname or IP address.
     */
    [[nodiscard]] QString host() const override { return host_; }

    /**
     * @brief Sets the authentication password.
     * @param password Password for API authentication.
     */
    void setPassword(const QString &password) override;

    /**
     * @brief Checks if a password is configured.
     * @return True if a password has been set.
     */
    [[nodiscard]] bool hasPassword() const override { return !password_.isEmpty(); }

    /// @name Device Information
    /// @{

    /**
     * @brief Requests the API version from the device.
     *
     * Emits versionReceived() on success.
     */
    void getVersion() override;

    /**
     * @brief Requests full device information.
     *
     * Emits infoReceived() on success.
     */
    void getInfo() override;
    /// @}

    /// @name Content Playback
    /// @{

    /**
     * @brief Plays a SID music file.
     * @param filePath Path to the SID file on the device.
     * @param songNumber Optional song number for multi-song SIDs (-1 for default).
     */
    void playSid(const QString &filePath, int songNumber = -1) override;

    /**
     * @brief Plays a MOD music file.
     * @param filePath Path to the MOD file on the device.
     */
    void playMod(const QString &filePath) override;

    /**
     * @brief Loads a PRG file without running it.
     * @param filePath Path to the PRG file on the device.
     */
    void loadPrg(const QString &filePath) override;

    /**
     * @brief Runs a PRG file.
     * @param filePath Path to the PRG file on the device.
     */
    void runPrg(const QString &filePath) override;

    /**
     * @brief Runs a cartridge image.
     * @param filePath Path to the CRT file on the device.
     */
    void runCrt(const QString &filePath) override;
    /// @}

    /// @name Drive Control
    /// @{

    /**
     * @brief Requests information about all drives.
     *
     * Emits drivesReceived() on success.
     */
    void getDrives() override;

    /**
     * @brief Mounts a disk image on a drive.
     * @param drive Drive identifier (e.g., "A").
     * @param imagePath Path to the disk image.
     * @param mode Mount mode: "readonly" or "readwrite".
     */
    void mountImage(const QString &drive, const QString &imagePath,
                    const QString &mode = "readwrite") override;

    /**
     * @brief Unmounts a disk image from a drive.
     * @param drive Drive identifier.
     */
    void unmountImage(const QString &drive) override;

    /**
     * @brief Resets a drive to its initial state.
     * @param drive Drive identifier.
     */
    void resetDrive(const QString &drive) override;
    /// @}

    /// @name Machine Control
    /// @{

    /**
     * @brief Resets the C64 machine.
     */
    void resetMachine() override;

    /**
     * @brief Reboots the Ultimate device.
     */
    void rebootMachine() override;

    /**
     * @brief Pauses the C64 machine execution.
     */
    void pauseMachine() override;

    /**
     * @brief Resumes C64 machine execution.
     */
    void resumeMachine() override;

    /**
     * @brief Powers off the Ultimate device.
     */
    void powerOffMachine() override;

    /**
     * @brief Simulates pressing the menu button.
     */
    void pressMenuButton() override;

    /**
     * @brief Writes data to C64 memory via DMA.
     * @param address Hexadecimal memory address (e.g., "0277").
     * @param data Data bytes to write.
     */
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
    /// @}

    /// @name File Operations
    /// @{

    /**
     * @brief Gets information about a file.
     * @param path Path to the file on the device.
     *
     * Emits fileInfoReceived() on success.
     */
    void getFileInfo(const QString &path) override;

    /**
     * @brief Creates a new D64 disk image.
     * @param path Path where the image will be created.
     * @param diskName Optional disk name (default: empty).
     * @param tracks Number of tracks: 35 or 40 (default: 35).
     */
    void createD64(const QString &path, const QString &diskName = QString(),
                   int tracks = 35) override;

    /**
     * @brief Creates a new D81 disk image.
     * @param path Path where the image will be created.
     * @param diskName Optional disk name (default: empty).
     */
    void createD81(const QString &path, const QString &diskName = QString()) override;
    /// @}

    /// @name Configuration
    /// @{

    /**
     * @brief Gets all configuration categories.
     *
     * Emits configCategoriesReceived() on success.
     */
    void getConfigCategories() override;

    /**
     * @brief Gets all items in a configuration category.
     * @param category Category name.
     *
     * Emits configCategoryItemsReceived() on success.
     */
    void getConfigCategoryItems(const QString &category) override;

    /**
     * @brief Gets a single configuration item value.
     * @param category Category name.
     * @param item Item name.
     *
     * Emits configItemReceived() on success.
     */
    void getConfigItem(const QString &category, const QString &item) override;

    /**
     * @brief Sets a single configuration item value.
     * @param category Configuration category name.
     * @param item Configuration item name.
     * @param value New value for the item.
     *
     * Emits configItemSet() on success.
     */
    void setConfigItem(const QString &category, const QString &item,
                       const QVariant &value) override;

    /**
     * @brief Updates multiple configuration values.
     * @param configs JSON object with configuration key-value pairs.
     *
     * Emits configsUpdated() on success.
     */
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

    /**
     * @brief Resets configuration to factory defaults.
     *
     * Emits configResetToDefaults() on success.
     */
    void resetConfigToDefaults() override;
    /// @}

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
