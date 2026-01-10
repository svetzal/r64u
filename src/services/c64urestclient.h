/**
 * @file c64urestclient.h
 * @brief REST API client for communicating with Ultimate 64/II+ devices.
 *
 * Provides HTTP-based control of C64 Ultimate devices including machine
 * control, drive operations, and content playback.
 */

#ifndef C64URESTCLIENT_H
#define C64URESTCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>

/**
 * @brief Device information returned by the Ultimate API.
 */
struct DeviceInfo {
    QString product;         ///< Product name (e.g., "Ultimate 64")
    QString firmwareVersion; ///< Firmware version string
    QString fpgaVersion;     ///< FPGA core version
    QString coreVersion;     ///< Core version string
    QString hostname;        ///< Network hostname
    QString uniqueId;        ///< Unique device identifier
    QString apiVersion;      ///< REST API version
};

/**
 * @brief Metadata for a configuration item including available options.
 */
struct ConfigItemMetadata {
    QVariant current;           ///< Current value
    QVariant defaultValue;      ///< Default value
    QStringList values;         ///< Available options for enum types
    QStringList presets;        ///< Preset options for file-type items
    int min = 0;                ///< Minimum value for numeric types
    int max = 0;                ///< Maximum value for numeric types
    QString format;             ///< Format string (e.g., "%d" or "%d00 ms")
    bool hasRange = false;      ///< True if min/max are valid
};

/**
 * @brief Information about a single drive on the device.
 */
struct DriveInfo {
    QString name;            ///< Drive identifier (e.g., "A", "B")
    bool enabled = false;    ///< Whether the drive is enabled
    int busId = 0;           ///< IEC bus device number
    QString type;            ///< Drive type (1541, 1571, 1581, etc.)
    QString rom;             ///< Current ROM image
    QString imageFile;       ///< Filename of mounted disk image
    QString imagePath;       ///< Full path to mounted image
    QString lastError;       ///< Last error message, if any
};

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
 * All operations are asynchronous with results delivered via Qt signals.
 *
 * @par Example usage:
 * @code
 * C64URestClient *rest = new C64URestClient(this);
 * rest->setHost("192.168.1.64");
 *
 * connect(rest, &C64URestClient::infoReceived, this, &MyClass::onInfo);
 * connect(rest, &C64URestClient::operationFailed, this, &MyClass::onError);
 *
 * rest->getInfo();
 * @endcode
 */
class C64URestClient : public QObject
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
    void setHost(const QString &host);

    /**
     * @brief Returns the currently configured host.
     * @return The hostname or IP address.
     */
    [[nodiscard]] QString host() const { return host_; }

    /**
     * @brief Sets the authentication password.
     * @param password Password for API authentication.
     */
    void setPassword(const QString &password);

    /**
     * @brief Checks if a password is configured.
     * @return True if a password has been set.
     */
    [[nodiscard]] bool hasPassword() const { return !password_.isEmpty(); }

    /// @name Device Information
    /// @{

    /**
     * @brief Requests the API version from the device.
     *
     * Emits versionReceived() on success.
     */
    void getVersion();

    /**
     * @brief Requests full device information.
     *
     * Emits infoReceived() on success.
     */
    void getInfo();
    /// @}

    /// @name Content Playback
    /// @{

    /**
     * @brief Plays a SID music file.
     * @param filePath Path to the SID file on the device.
     * @param songNumber Optional song number for multi-song SIDs (-1 for default).
     */
    void playSid(const QString &filePath, int songNumber = -1);

    /**
     * @brief Plays a MOD music file.
     * @param filePath Path to the MOD file on the device.
     */
    void playMod(const QString &filePath);

    /**
     * @brief Loads a PRG file without running it.
     * @param filePath Path to the PRG file on the device.
     */
    void loadPrg(const QString &filePath);

    /**
     * @brief Runs a PRG file.
     * @param filePath Path to the PRG file on the device.
     */
    void runPrg(const QString &filePath);

    /**
     * @brief Runs a cartridge image.
     * @param filePath Path to the CRT file on the device.
     */
    void runCrt(const QString &filePath);
    /// @}

    /// @name Drive Control
    /// @{

    /**
     * @brief Requests information about all drives.
     *
     * Emits drivesReceived() on success.
     */
    void getDrives();

    /**
     * @brief Mounts a disk image on a drive.
     * @param drive Drive identifier (e.g., "A").
     * @param imagePath Path to the disk image.
     * @param mode Mount mode: "readonly" or "readwrite".
     */
    void mountImage(const QString &drive, const QString &imagePath,
                    const QString &mode = "readwrite");

    /**
     * @brief Unmounts a disk image from a drive.
     * @param drive Drive identifier.
     */
    void unmountImage(const QString &drive);

    /**
     * @brief Resets a drive to its initial state.
     * @param drive Drive identifier.
     */
    void resetDrive(const QString &drive);
    /// @}

    /// @name Machine Control
    /// @{

    /**
     * @brief Resets the C64 machine.
     */
    void resetMachine();

    /**
     * @brief Reboots the Ultimate device.
     */
    void rebootMachine();

    /**
     * @brief Pauses the C64 machine execution.
     */
    void pauseMachine();

    /**
     * @brief Resumes C64 machine execution.
     */
    void resumeMachine();

    /**
     * @brief Powers off the Ultimate device.
     */
    void powerOffMachine();

    /**
     * @brief Simulates pressing the menu button.
     */
    void pressMenuButton();

    /**
     * @brief Writes data to C64 memory via DMA.
     * @param address Hexadecimal memory address (e.g., "0277").
     * @param data Data bytes to write.
     */
    void writeMem(const QString &address, const QByteArray &data);
    /// @}

    /// @name File Operations
    /// @{

    /**
     * @brief Gets information about a file.
     * @param path Path to the file on the device.
     *
     * Emits fileInfoReceived() on success.
     */
    void getFileInfo(const QString &path);

    /**
     * @brief Creates a new D64 disk image.
     * @param path Path where the image will be created.
     * @param diskName Optional disk name (default: empty).
     * @param tracks Number of tracks: 35 or 40 (default: 35).
     */
    void createD64(const QString &path, const QString &diskName = QString(),
                   int tracks = 35);

    /**
     * @brief Creates a new D81 disk image.
     * @param path Path where the image will be created.
     * @param diskName Optional disk name (default: empty).
     */
    void createD81(const QString &path, const QString &diskName = QString());
    /// @}

    /// @name Configuration
    /// @{

    /**
     * @brief Gets all configuration categories.
     *
     * Emits configCategoriesReceived() on success.
     */
    void getConfigCategories();

    /**
     * @brief Gets all items in a configuration category.
     * @param category Category name.
     *
     * Emits configCategoryItemsReceived() on success.
     */
    void getConfigCategoryItems(const QString &category);

    /**
     * @brief Gets a single configuration item value.
     * @param category Category name.
     * @param item Item name.
     *
     * Emits configItemReceived() on success.
     */
    void getConfigItem(const QString &category, const QString &item);

    /**
     * @brief Sets a single configuration item value.
     * @param category Configuration category name.
     * @param item Configuration item name.
     * @param value New value for the item.
     *
     * Emits configItemSet() on success.
     */
    void setConfigItem(const QString &category, const QString &item,
                       const QVariant &value);

    /**
     * @brief Updates multiple configuration values.
     * @param configs JSON object with configuration key-value pairs.
     *
     * Emits configsUpdated() on success.
     */
    void updateConfigsBatch(const QJsonObject &configs);

    /**
     * @brief Saves configuration to flash.
     *
     * Persists current settings to non-volatile storage.
     * Emits configSavedToFlash() on success.
     */
    void saveConfigToFlash();

    /**
     * @brief Loads configuration from flash.
     *
     * Reverts to last saved settings from non-volatile storage.
     * Emits configLoadedFromFlash() on success.
     */
    void loadConfigFromFlash();

    /**
     * @brief Resets configuration to factory defaults.
     *
     * Emits configResetToDefaults() on success.
     */
    void resetConfigToDefaults();
    /// @}

signals:
    /// @name Information Signals
    /// @{

    /**
     * @brief Emitted when the API version is received.
     * @param version The API version string.
     */
    void versionReceived(const QString &version);

    /**
     * @brief Emitted when device information is received.
     * @param info The device information structure.
     */
    void infoReceived(const DeviceInfo &info);

    /**
     * @brief Emitted when drive information is received.
     * @param drives List of drive information structures.
     */
    void drivesReceived(const QList<DriveInfo> &drives);

    /**
     * @brief Emitted when file information is received.
     * @param path The file path.
     * @param size File size in bytes.
     * @param extension File extension.
     */
    void fileInfoReceived(const QString &path, qint64 size, const QString &extension);

    /**
     * @brief Emitted when configuration categories are received.
     * @param categories List of category names.
     */
    void configCategoriesReceived(const QStringList &categories);

    /**
     * @brief Emitted when configuration category items are received.
     * @param category Category name.
     * @param items Map of item names to metadata including current values and options.
     */
    void configCategoryItemsReceived(const QString &category,
                                     const QHash<QString, ConfigItemMetadata> &items);

    /**
     * @brief Emitted when a single configuration item value is received.
     * @param category Category name.
     * @param item Item name.
     * @param value The item's current value.
     */
    void configItemReceived(const QString &category, const QString &item,
                            const QVariant &value);

    /**
     * @brief Emitted when a single configuration item is set.
     * @param category Category name.
     * @param item Item name.
     */
    void configItemSet(const QString &category, const QString &item);

    /**
     * @brief Emitted when configuration update completes.
     */
    void configsUpdated();

    /**
     * @brief Emitted when configuration is saved to flash.
     */
    void configSavedToFlash();

    /**
     * @brief Emitted when configuration is loaded from flash.
     */
    void configLoadedFromFlash();

    /**
     * @brief Emitted when configuration is reset to defaults.
     */
    void configResetToDefaults();
    /// @}

    /// @name Operation Signals
    /// @{

    /**
     * @brief Emitted when an operation succeeds.
     * @param operation Name of the operation that succeeded.
     */
    void operationSucceeded(const QString &operation);

    /**
     * @brief Emitted when an operation fails.
     * @param operation Name of the operation that failed.
     * @param error Error description.
     */
    void operationFailed(const QString &operation, const QString &error);

    /**
     * @brief Emitted when a network connection error occurs.
     * @param error Error description.
     */
    void connectionError(const QString &error);
    /// @}

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    [[nodiscard]] QNetworkRequest createRequest(const QString &endpoint) const;
    void sendGetRequest(const QString &endpoint, const QString &operation);
    void sendPutRequest(const QString &endpoint, const QString &operation,
                        const QByteArray &data = QByteArray());
    void sendPostRequest(const QString &endpoint, const QString &operation,
                         const QByteArray &data,
                         const QString &contentType = "application/octet-stream");

    void handleVersionResponse(const QJsonObject &json);
    void handleInfoResponse(const QJsonObject &json);
    void handleDrivesResponse(const QJsonObject &json);
    void handleFileInfoResponse(const QJsonObject &json);
    void handleConfigCategoriesResponse(const QJsonObject &json);
    void handleConfigCategoryItemsResponse(const QString &category,
                                           const QJsonObject &json);
    void handleGenericResponse(const QString &operation, const QJsonObject &json);

    [[nodiscard]] QStringList extractErrors(const QJsonObject &json) const;

    // Network
    QNetworkAccessManager *networkManager_ = nullptr;

    // Configuration
    QString host_;
    QString password_;

    // Operation tracking
    QHash<QNetworkReply*, QString> pendingOperations_;
};

#endif // C64URESTCLIENT_H
