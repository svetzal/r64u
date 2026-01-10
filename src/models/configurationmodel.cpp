#include "configurationmodel.h"

ConfigurationModel::ConfigurationModel(QObject *parent)
    : QObject(parent)
{
}

void ConfigurationModel::setCategories(const QStringList &categories)
{
    // Clear existing data
    bool wasDirty = isDirty();
    categories_.clear();
    items_.clear();
    dirtyCount_ = 0;

    categories_ = categories;

    emit categoriesChanged();

    if (wasDirty) {
        emit dirtyStateChanged(false);
    }
}

bool ConfigurationModel::hasCategory(const QString &category) const
{
    return categories_.contains(category);
}

void ConfigurationModel::setCategoryItems(const QString &category,
                                          const QHash<QString, QVariant> &items)
{
    QHash<QString, ConfigItemInfo> itemsWithInfo;
    for (auto it = items.constBegin(); it != items.constEnd(); ++it) {
        ConfigItemInfo info;
        info.value = it.value();
        info.isDirty = false;
        itemsWithInfo[it.key()] = info;
    }
    setCategoryItemsWithInfo(category, itemsWithInfo);
}

void ConfigurationModel::setCategoryItemsWithInfo(
    const QString &category,
    const QHash<QString, ConfigItemInfo> &items)
{
    // Count dirty items being removed
    if (items_.contains(category)) {
        for (const ConfigItemInfo &info : items_[category]) {
            if (info.isDirty) {
                dirtyCount_--;
            }
        }
    }

    // Store the new items (ensure dirty flags are cleared on load)
    QHash<QString, ConfigItemInfo> cleanItems;
    for (auto it = items.constBegin(); it != items.constEnd(); ++it) {
        ConfigItemInfo info = it.value();
        info.isDirty = false;
        cleanItems[it.key()] = info;
    }
    items_[category] = cleanItems;

    emit categoryItemsChanged(category);

    if (dirtyCount_ == 0) {
        emit dirtyStateChanged(false);
    }
}

QStringList ConfigurationModel::itemNames(const QString &category) const
{
    if (!items_.contains(category)) {
        return {};
    }
    return items_[category].keys();
}

int ConfigurationModel::itemCount(const QString &category) const
{
    if (!items_.contains(category)) {
        return 0;
    }
    return items_[category].count();
}

bool ConfigurationModel::hasItem(const QString &category, const QString &item) const
{
    if (!items_.contains(category)) {
        return false;
    }
    return items_[category].contains(item);
}

QVariant ConfigurationModel::value(const QString &category, const QString &item) const
{
    if (!items_.contains(category)) {
        return {};
    }
    if (!items_[category].contains(item)) {
        return {};
    }
    return items_[category][item].value;
}

ConfigItemInfo ConfigurationModel::itemInfo(const QString &category,
                                            const QString &item) const
{
    if (!items_.contains(category)) {
        return {};
    }
    if (!items_[category].contains(item)) {
        return {};
    }
    return items_[category][item];
}

bool ConfigurationModel::setValue(const QString &category, const QString &item,
                                  const QVariant &value)
{
    if (!items_.contains(category)) {
        return false;
    }
    if (!items_[category].contains(item)) {
        return false;
    }

    ConfigItemInfo &info = items_[category][item];

    // Check if value actually changed
    if (info.value == value) {
        return false;
    }

    bool wasDirty = isDirty();

    // Update the value
    info.value = value;

    // Mark as dirty if not already
    if (!info.isDirty) {
        info.isDirty = true;
        dirtyCount_++;
    }

    emit itemValueChanged(category, item, value);

    if (!wasDirty && isDirty()) {
        emit dirtyStateChanged(true);
    }

    return true;
}

bool ConfigurationModel::isItemDirty(const QString &category,
                                     const QString &item) const
{
    if (!items_.contains(category)) {
        return false;
    }
    if (!items_[category].contains(item)) {
        return false;
    }
    return items_[category][item].isDirty;
}

QHash<QString, QVariant> ConfigurationModel::dirtyItems() const
{
    QHash<QString, QVariant> result;

    for (auto catIt = items_.constBegin(); catIt != items_.constEnd(); ++catIt) {
        const QString &category = catIt.key();
        const QHash<QString, ConfigItemInfo> &categoryItems = catIt.value();

        for (auto itemIt = categoryItems.constBegin();
             itemIt != categoryItems.constEnd(); ++itemIt) {
            if (itemIt.value().isDirty) {
                QString path = category + "/" + itemIt.key();
                result[path] = itemIt.value().value;
            }
        }
    }

    return result;
}

void ConfigurationModel::clearDirtyFlags()
{
    if (dirtyCount_ == 0) {
        return;
    }

    for (auto catIt = items_.begin(); catIt != items_.end(); ++catIt) {
        for (auto itemIt = catIt.value().begin();
             itemIt != catIt.value().end(); ++itemIt) {
            itemIt.value().isDirty = false;
        }
    }

    dirtyCount_ = 0;
    emit dirtyStateChanged(false);
}

void ConfigurationModel::clearItemDirtyFlag(const QString &category,
                                            const QString &item)
{
    if (!items_.contains(category)) {
        return;
    }
    if (!items_[category].contains(item)) {
        return;
    }

    ConfigItemInfo &info = items_[category][item];
    if (!info.isDirty) {
        return;
    }

    info.isDirty = false;
    dirtyCount_--;

    if (dirtyCount_ == 0) {
        emit dirtyStateChanged(false);
    }
}

void ConfigurationModel::clear()
{
    bool wasDirty = isDirty();

    categories_.clear();
    items_.clear();
    dirtyCount_ = 0;

    emit categoriesChanged();

    if (wasDirty) {
        emit dirtyStateChanged(false);
    }
}
