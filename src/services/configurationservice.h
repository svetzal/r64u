#ifndef CONFIGURATIONSERVICE_H
#define CONFIGURATIONSERVICE_H

#include "services/devicetypes.h"
#include "services/irestclient.h"

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVariant>

class DeviceConnection;

/**
 * @brief Service wrapping configuration REST operations.
 *
 * Mediates between ConfigPanel and IRestClient, encapsulating the
 * canPerformOperations() guard and forwarding config signals so UI
 * components have no direct dependency on IRestClient.
 *
 * All methods are no-ops and emit operationNotAvailable() when the
 * device connection cannot perform operations.
 */
class ConfigurationService : public QObject
{
    Q_OBJECT

public:
    explicit ConfigurationService(DeviceConnection *connection, QObject *parent = nullptr);

    /**
     * @brief Requests all configuration categories from the device.
     *
     * Emits operationNotAvailable() if the device is not connected.
     */
    void getConfigCategories();

    /**
     * @brief Requests all items in the given configuration category.
     * @param category Category name.
     *
     * Emits operationNotAvailable() if the device is not connected.
     */
    void getConfigCategoryItems(const QString &category);

    /**
     * @brief Sets a configuration item value on the device.
     * @param category Category name.
     * @param item Item name.
     * @param value New value.
     *
     * Emits operationNotAvailable() if the device is not connected.
     */
    void setConfigItem(const QString &category, const QString &item, const QVariant &value);

    /**
     * @brief Saves the current configuration to non-volatile flash storage.
     *
     * Emits operationNotAvailable() if the device is not connected.
     */
    void saveConfigToFlash();

    /**
     * @brief Loads the configuration from flash storage.
     *
     * Emits operationNotAvailable() if the device is not connected.
     */
    void loadConfigFromFlash();

    /**
     * @brief Resets all configuration values to factory defaults.
     *
     * Emits operationNotAvailable() if the device is not connected.
     */
    void resetConfigToDefaults();

    /**
     * @brief Returns true when the underlying device connection can perform operations.
     */
    [[nodiscard]] bool canPerformOperations() const;

signals:
    /// Forwarded from IRestClient::configCategoriesReceived.
    void configCategoriesReceived(const QStringList &categories);

    /// Forwarded from IRestClient::configCategoryItemsReceived.
    void configCategoryItemsReceived(const QString &category,
                                     const QHash<QString, ConfigItemMetadata> &items);

    /// Forwarded from IRestClient::configItemSet.
    void configItemSet(const QString &category, const QString &item);

    /// Forwarded from IRestClient::configSavedToFlash.
    void configSavedToFlash();

    /// Forwarded from IRestClient::configLoadedFromFlash.
    void configLoadedFromFlash();

    /// Forwarded from IRestClient::configResetToDefaults.
    void configResetToDefaults();

    /**
     * @brief Emitted when an operation is attempted but the device is not connected.
     * @param operation Human-readable name of the attempted operation.
     */
    void operationNotAvailable(const QString &operation);

private:
    DeviceConnection *deviceConnection_ = nullptr;
};

#endif  // CONFIGURATIONSERVICE_H
