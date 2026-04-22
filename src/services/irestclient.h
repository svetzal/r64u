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
    explicit IRestClient(QObject *parent = nullptr) : QObject(parent) {}
    ~IRestClient() override = default;

    virtual void setHost(const QString &host) = 0;
    [[nodiscard]] virtual QString host() const = 0;
    virtual void setPassword(const QString &password) = 0;
    [[nodiscard]] virtual bool hasPassword() const = 0;

    virtual void getVersion() = 0;
    virtual void getInfo() = 0;

    /**
     * @brief Plays a SID music file.
     * @param filePath Path to the SID file on the device.
     * @param songNumber Song index for multi-song SIDs; -1 selects the default song.
     */
    virtual void playSid(const QString &filePath, int songNumber = -1) = 0;

    virtual void playMod(const QString &filePath) = 0;
    virtual void loadPrg(const QString &filePath) = 0;
    virtual void runPrg(const QString &filePath) = 0;
    virtual void runCrt(const QString &filePath) = 0;

    virtual void getDrives() = 0;

    /**
     * @brief Mounts a disk image on a drive.
     * @param drive Drive identifier (e.g., "A").
     * @param imagePath Path to the disk image.
     * @param mode Mount mode: "readonly" or "readwrite".
     */
    virtual void mountImage(const QString &drive, const QString &imagePath,
                            const QString &mode = "readwrite") = 0;

    virtual void unmountImage(const QString &drive) = 0;
    virtual void resetDrive(const QString &drive) = 0;

    virtual void resetMachine() = 0;
    virtual void rebootMachine() = 0;
    virtual void pauseMachine() = 0;
    virtual void resumeMachine() = 0;
    virtual void powerOffMachine() = 0;
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

    virtual void getFileInfo(const QString &path) = 0;

    /**
     * @brief Creates a new D64 disk image.
     * @param path Path where the image will be created.
     * @param diskName Optional disk name.
     * @param tracks Number of tracks: 35 or 40.
     */
    virtual void createD64(const QString &path, const QString &diskName = QString(),
                           int tracks = 35) = 0;

    virtual void createD81(const QString &path, const QString &diskName = QString()) = 0;

    virtual void getConfigCategories() = 0;

    /**
     * @brief Gets all items in a configuration category.
     * @param category Category name.
     */
    virtual void getConfigCategoryItems(const QString &category) = 0;

    /**
     * @brief Gets a single configuration item value.
     * @param category Category name.
     * @param item Item name within the category.
     */
    virtual void getConfigItem(const QString &category, const QString &item) = 0;

    /**
     * @brief Sets a single configuration item value.
     * @param category Configuration category name.
     * @param item Configuration item name.
     * @param value New value for the item.
     */
    virtual void setConfigItem(const QString &category, const QString &item,
                               const QVariant &value) = 0;

    /**
     * @brief Updates multiple configuration values in one call.
     * @param configs JSON object mapping configuration keys to values.
     */
    virtual void updateConfigsBatch(const QJsonObject &configs) = 0;

    virtual void saveConfigToFlash() = 0;
    virtual void loadConfigFromFlash() = 0;
    virtual void resetConfigToDefaults() = 0;

signals:
    void versionReceived(const QString &version);
    void infoReceived(const DeviceInfo &info);
    void drivesReceived(const QList<DriveInfo> &drives);
    void fileInfoReceived(const QString &path, qint64 size, const QString &extension);
    void configCategoriesReceived(const QStringList &categories);

    /**
     * @brief Emitted when configuration category items are received.
     * @param category Category name.
     * @param items Map of item names to metadata including current values and options.
     */
    void configCategoryItemsReceived(const QString &category,
                                     const QHash<QString, ConfigItemMetadata> &items);

    void configItemReceived(const QString &category, const QString &item, const QVariant &value);
    void configItemSet(const QString &category, const QString &item);
    void configsUpdated();
    void configSavedToFlash();
    void configLoadedFromFlash();
    void configResetToDefaults();

    void operationSucceeded(const QString &operation);
    void operationFailed(const QString &operation, const QString &error);
    void connectionError(const QString &error);
};

#endif  // IRESTCLIENT_H
