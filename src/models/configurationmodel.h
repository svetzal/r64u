/**
 * @file configurationmodel.h
 * @brief Model for storing and managing device configuration data.
 *
 * Provides storage for configuration categories and items retrieved from
 * the Ultimate device REST API, with dirty state tracking for modifications.
 */

#ifndef CONFIGURATIONMODEL_H
#define CONFIGURATIONMODEL_H

#include <QObject>
#include <QHash>
#include <QStringList>
#include <QVariant>

/**
 * @brief Metadata for a configuration item.
 */
struct ConfigItemInfo {
    QVariant value;           ///< Current value
    QVariant defaultValue;    ///< Default value (if available)
    QVariant minValue;        ///< Minimum value (for numeric items)
    QVariant maxValue;        ///< Maximum value (for numeric items)
    QStringList options;      ///< Available options (for enum items)
    bool isDirty = false;     ///< True if modified since last save/load
};

/**
 * @brief Model for device configuration data.
 *
 * This class stores configuration data organized by categories and items.
 * It tracks modifications (dirty state) and emits signals when data changes.
 *
 * The model is designed to work with the Ultimate device REST API:
 * - Categories are loaded from GET /v1/configs
 * - Items are loaded from GET /v1/configs/{category}
 * - Changes are saved via PUT /v1/configs/{category}/{item}
 *
 * @par Example usage:
 * @code
 * ConfigurationModel *config = new ConfigurationModel(this);
 *
 * // Load categories
 * config->setCategories({"Audio Mixer", "Network Settings"});
 *
 * // Load items for a category
 * QHash<QString, QVariant> audioItems;
 * audioItems["Volume"] = 80;
 * audioItems["Mute"] = false;
 * config->setCategoryItems("Audio Mixer", audioItems);
 *
 * // Modify a value (marks it dirty)
 * config->setValue("Audio Mixer", "Volume", 90);
 *
 * // Check dirty state
 * if (config->isDirty()) {
 *     // Save changes...
 * }
 * @endcode
 */
class ConfigurationModel : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a configuration model.
     * @param parent Optional parent QObject.
     */
    explicit ConfigurationModel(QObject *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~ConfigurationModel() override = default;

    /// @name Category Management
    /// @{

    /**
     * @brief Sets the list of configuration categories.
     * @param categories List of category names.
     *
     * Clears any existing categories and items.
     * Emits categoriesChanged().
     */
    void setCategories(const QStringList &categories);

    /**
     * @brief Returns the list of categories.
     * @return Category names.
     */
    [[nodiscard]] QStringList categories() const { return categories_; }

    /**
     * @brief Checks if a category exists.
     * @param category Category name.
     * @return True if the category exists.
     */
    [[nodiscard]] bool hasCategory(const QString &category) const;
    /// @}

    /// @name Item Management
    /// @{

    /**
     * @brief Sets all items for a category.
     * @param category Category name.
     * @param items Map of item names to values.
     *
     * Clears any existing items for the category.
     * Emits categoryItemsChanged().
     */
    void setCategoryItems(const QString &category,
                          const QHash<QString, QVariant> &items);

    /**
     * @brief Sets detailed item info for a category.
     * @param category Category name.
     * @param items Map of item names to ConfigItemInfo.
     *
     * Use this when you have min/max/default metadata.
     * Emits categoryItemsChanged().
     */
    void setCategoryItemsWithInfo(const QString &category,
                                  const QHash<QString, ConfigItemInfo> &items);

    /**
     * @brief Returns all item names in a category.
     * @param category Category name.
     * @return List of item names, or empty if category doesn't exist.
     */
    [[nodiscard]] QStringList itemNames(const QString &category) const;

    /**
     * @brief Returns the number of items in a category.
     * @param category Category name.
     * @return Item count, or 0 if category doesn't exist.
     */
    [[nodiscard]] int itemCount(const QString &category) const;

    /**
     * @brief Checks if an item exists.
     * @param category Category name.
     * @param item Item name.
     * @return True if the item exists.
     */
    [[nodiscard]] bool hasItem(const QString &category, const QString &item) const;
    /// @}

    /// @name Value Access
    /// @{

    /**
     * @brief Gets an item's current value.
     * @param category Category name.
     * @param item Item name.
     * @return The value, or invalid QVariant if not found.
     */
    [[nodiscard]] QVariant value(const QString &category, const QString &item) const;

    /**
     * @brief Gets full item info including metadata.
     * @param category Category name.
     * @param item Item name.
     * @return Item info, or default ConfigItemInfo if not found.
     */
    [[nodiscard]] ConfigItemInfo itemInfo(const QString &category,
                                          const QString &item) const;

    /**
     * @brief Sets an item's value.
     * @param category Category name.
     * @param item Item name.
     * @param value New value.
     * @return True if the value was changed, false if unchanged or not found.
     *
     * Marks the item as dirty if the value changed.
     * Emits itemValueChanged() and dirtyStateChanged() if applicable.
     */
    bool setValue(const QString &category, const QString &item,
                  const QVariant &value);
    /// @}

    /// @name Dirty State Tracking
    /// @{

    /**
     * @brief Checks if any items have been modified.
     * @return True if any item is dirty.
     */
    [[nodiscard]] bool isDirty() const { return dirtyCount_ > 0; }

    /**
     * @brief Checks if a specific item is dirty.
     * @param category Category name.
     * @param item Item name.
     * @return True if the item is dirty.
     */
    [[nodiscard]] bool isItemDirty(const QString &category,
                                   const QString &item) const;

    /**
     * @brief Returns all dirty items.
     * @return Map of "category/item" paths to their current values.
     */
    [[nodiscard]] QHash<QString, QVariant> dirtyItems() const;

    /**
     * @brief Clears dirty flags for all items.
     *
     * Call this after successfully saving changes.
     * Emits dirtyStateChanged() if state changed.
     */
    void clearDirtyFlags();

    /**
     * @brief Clears dirty flag for a specific item.
     * @param category Category name.
     * @param item Item name.
     *
     * Emits dirtyStateChanged() if this was the last dirty item.
     */
    void clearItemDirtyFlag(const QString &category, const QString &item);
    /// @}

    /// @name Data Management
    /// @{

    /**
     * @brief Clears all data.
     *
     * Removes all categories and items.
     * Emits categoriesChanged() and dirtyStateChanged() if applicable.
     */
    void clear();
    /// @}

signals:
    /**
     * @brief Emitted when the category list changes.
     */
    void categoriesChanged();

    /**
     * @brief Emitted when items in a category change.
     * @param category The category that was modified.
     */
    void categoryItemsChanged(const QString &category);

    /**
     * @brief Emitted when a single item's value changes.
     * @param category Category name.
     * @param item Item name.
     * @param value New value.
     */
    void itemValueChanged(const QString &category, const QString &item,
                          const QVariant &value);

    /**
     * @brief Emitted when dirty state changes.
     * @param isDirty True if any items are dirty.
     */
    void dirtyStateChanged(bool isDirty);

private:
    /// List of category names (preserves order)
    QStringList categories_;

    /// Items per category: category -> (item name -> info)
    QHash<QString, QHash<QString, ConfigItemInfo>> items_;

    /// Count of dirty items (for quick isDirty() check)
    int dirtyCount_ = 0;
};

#endif // CONFIGURATIONMODEL_H
