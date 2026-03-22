/**
 * @file irestclient.h
 * @brief Interface for REST client implementations.
 *
 * This interface allows dependency injection of REST clients, enabling
 * runtime swapping between production and mock implementations for testing.
 */

#ifndef IRESTCLIENT_H
#define IRESTCLIENT_H

#include "devicetypes.h"

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

/**
 * @brief Abstract interface for REST client implementations.
 *
 * This interface defines the contract for communicating with Ultimate 64/II+
 * devices via the REST API. It enables dependency injection for testing by
 * allowing mock implementations to be swapped in at runtime.
 *
 * @par Example usage:
 * @code
 * // Production code
 * IRestClient *rest = new C64URestClient(this);
 *
 * // Test code
 * IRestClient *rest = new MockRestClient(this);
 *
 * // Both can be used identically
 * rest->setHost("192.168.1.64");
 * rest->getInfo();
 * @endcode
 */
class IRestClient : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a REST client interface.
     * @param parent Optional parent QObject for memory management.
     */
    explicit IRestClient(QObject *parent = nullptr) : QObject(parent) {}

    /**
     * @brief Virtual destructor.
     */
    ~IRestClient() override = default;

    /// @name Configuration
    /// @{

    /**
     * @brief Sets the target host.
     * @param host Hostname or IP address of the Ultimate device.
     */
    virtual void setHost(const QString &host) = 0;

    /**
     * @brief Returns the currently configured host.
     * @return The hostname or IP address.
     */
    [[nodiscard]] virtual QString host() const = 0;

    /**
     * @brief Sets the authentication password.
     * @param password Password for API authentication.
     */
    virtual void setPassword(const QString &password) = 0;

    /**
     * @brief Checks if a password is configured.
     * @return True if a password has been set.
     */
    [[nodiscard]] virtual bool hasPassword() const = 0;
    /// @}

    /// @name Device Information
    /// @{

    /**
     * @brief Requests the API version from the device.
     *
     * Emits versionReceived() on success.
     */
    virtual void getVersion() = 0;

    /**
     * @brief Requests full device information.
     *
     * Emits infoReceived() on success.
     */
    virtual void getInfo() = 0;
    /// @}

    /// @name Content Playback
    /// @{

    /**
     * @brief Plays a SID music file.
     * @param filePath Path to the SID file on the device.
     * @param songNumber Optional song number for multi-song SIDs (-1 for default).
     */
    virtual void playSid(const QString &filePath, int songNumber = -1) = 0;

    /**
     * @brief Plays a MOD music file.
     * @param filePath Path to the MOD file on the device.
     */
    virtual void playMod(const QString &filePath) = 0;

    /**
     * @brief Loads a PRG file without running it.
     * @param filePath Path to the PRG file on the device.
     */
    virtual void loadPrg(const QString &filePath) = 0;

    /**
     * @brief Runs a PRG file.
     * @param filePath Path to the PRG file on the device.
     */
    virtual void runPrg(const QString &filePath) = 0;

    /**
     * @brief Runs a cartridge image.
     * @param filePath Path to the CRT file on the device.
     */
    virtual void runCrt(const QString &filePath) = 0;
    /// @}

    /// @name Drive Control
    /// @{

    /**
     * @brief Requests information about all drives.
     *
     * Emits drivesReceived() on success.
     */
    virtual void getDrives() = 0;

    /**
     * @brief Mounts a disk image on a drive.
     * @param drive Drive identifier (e.g., "A").
     * @param imagePath Path to the disk image.
     * @param mode Mount mode: "readonly" or "readwrite".
     */
    virtual void mountImage(const QString &drive, const QString &imagePath,
                            const QString &mode = "readwrite") = 0;

    /**
     * @brief Unmounts a disk image from a drive.
     * @param drive Drive identifier.
     */
    virtual void unmountImage(const QString &drive) = 0;

    /**
     * @brief Resets a drive to its initial state.
     * @param drive Drive identifier.
     */
    virtual void resetDrive(const QString &drive) = 0;
    /// @}

    /// @name Machine Control
    /// @{

    /**
     * @brief Resets the C64 machine.
     */
    virtual void resetMachine() = 0;

    /**
     * @brief Reboots the Ultimate device.
     */
    virtual void rebootMachine() = 0;

    /**
     * @brief Pauses the C64 machine execution.
     */
    virtual void pauseMachine() = 0;

    /**
     * @brief Resumes C64 machine execution.
     */
    virtual void resumeMachine() = 0;

    /**
     * @brief Powers off the Ultimate device.
     */
    virtual void powerOffMachine() = 0;

    /**
     * @brief Simulates pressing the menu button.
     */
    virtual void pressMenuButton() = 0;

    /**
     * @brief Writes data to C64 memory via DMA.
     * @param address Hexadecimal memory address (e.g., "0277").
     * @param data Data bytes to write.
     */
    virtual void writeMem(const QString &address, const QByteArray &data) = 0;

    /**
     * @brief Types text into the C64 via keyboard buffer injection.
     * @param text Text to type (ASCII, converted to PETSCII).
     */
    virtual void typeText(const QString &text) = 0;
    /// @}

    /// @name File Operations
    /// @{

    /**
     * @brief Gets information about a file.
     * @param path Path to the file on the device.
     *
     * Emits fileInfoReceived() on success.
     */
    virtual void getFileInfo(const QString &path) = 0;

    /**
     * @brief Creates a new D64 disk image.
     * @param path Path where the image will be created.
     * @param diskName Optional disk name (default: empty).
     * @param tracks Number of tracks: 35 or 40 (default: 35).
     */
    virtual void createD64(const QString &path, const QString &diskName = QString(),
                           int tracks = 35) = 0;

    /**
     * @brief Creates a new D81 disk image.
     * @param path Path where the image will be created.
     * @param diskName Optional disk name (default: empty).
     */
    virtual void createD81(const QString &path, const QString &diskName = QString()) = 0;
    /// @}

    /// @name Configuration
    /// @{

    /**
     * @brief Gets all configuration categories.
     *
     * Emits configCategoriesReceived() on success.
     */
    virtual void getConfigCategories() = 0;

    /**
     * @brief Gets all items in a configuration category.
     * @param category Category name.
     *
     * Emits configCategoryItemsReceived() on success.
     */
    virtual void getConfigCategoryItems(const QString &category) = 0;

    /**
     * @brief Gets a single configuration item value.
     * @param category Category name.
     * @param item Item name.
     *
     * Emits configItemReceived() on success.
     */
    virtual void getConfigItem(const QString &category, const QString &item) = 0;

    /**
     * @brief Sets a single configuration item value.
     * @param category Configuration category name.
     * @param item Configuration item name.
     * @param value New value for the item.
     *
     * Emits configItemSet() on success.
     */
    virtual void setConfigItem(const QString &category, const QString &item,
                               const QVariant &value) = 0;

    /**
     * @brief Updates multiple configuration values.
     * @param configs JSON object with configuration key-value pairs.
     *
     * Emits configsUpdated() on success.
     */
    virtual void updateConfigsBatch(const QJsonObject &configs) = 0;

    /**
     * @brief Saves configuration to flash.
     *
     * Emits configSavedToFlash() on success.
     */
    virtual void saveConfigToFlash() = 0;

    /**
     * @brief Loads configuration from flash.
     *
     * Emits configLoadedFromFlash() on success.
     */
    virtual void loadConfigFromFlash() = 0;

    /**
     * @brief Resets configuration to factory defaults.
     *
     * Emits configResetToDefaults() on success.
     */
    virtual void resetConfigToDefaults() = 0;
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
    void configItemReceived(const QString &category, const QString &item, const QVariant &value);

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
};

#endif  // IRESTCLIENT_H
