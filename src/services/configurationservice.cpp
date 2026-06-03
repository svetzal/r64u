#include "configurationservice.h"

#include "services/deviceconnectionmanager.h"
#include "services/irestclient.h"

ConfigurationService::ConfigurationService(DeviceConnectionManager *connection, QObject *parent)
    : QObject(parent), deviceConnection_(connection)
{
    if (deviceConnection_ && deviceConnection_->restClient()) {
        IRestClient *restClient = deviceConnection_->restClient();
        connect(restClient, &IRestClient::configCategoriesReceived, this,
                &ConfigurationService::configCategoriesReceived);
        connect(restClient, &IRestClient::configCategoryItemsReceived, this,
                &ConfigurationService::configCategoryItemsReceived);
        connect(restClient, &IRestClient::configItemSet, this,
                &ConfigurationService::configItemSet);
        connect(restClient, &IRestClient::configSavedToFlash, this,
                &ConfigurationService::configSavedToFlash);
        connect(restClient, &IRestClient::configLoadedFromFlash, this,
                &ConfigurationService::configLoadedFromFlash);
        connect(restClient, &IRestClient::configResetToDefaults, this,
                &ConfigurationService::configResetToDefaults);
    }
}

bool ConfigurationService::canPerformOperations() const
{
    return deviceConnection_ && deviceConnection_->canPerformOperations();
}

bool ConfigurationService::ensureCanPerformOperations(const QString &operation)
{
    if (!canPerformOperations()) {
        emit operationNotAvailable(operation);
        return false;
    }
    return true;
}

void ConfigurationService::getConfigCategories()
{
    if (!ensureCanPerformOperations(tr("Get config categories")))
        return;
    deviceConnection_->restClient()->getConfigCategories();
}

void ConfigurationService::getConfigCategoryItems(const QString &category)
{
    if (!ensureCanPerformOperations(tr("Get config category items")))
        return;
    deviceConnection_->restClient()->getConfigCategoryItems(category);
}

void ConfigurationService::setConfigItem(const QString &category, const QString &item,
                                         const QVariant &value)
{
    if (!ensureCanPerformOperations(tr("Set config item")))
        return;
    deviceConnection_->restClient()->setConfigItem(category, item, value);
}

void ConfigurationService::saveConfigToFlash()
{
    if (!ensureCanPerformOperations(tr("Save config to flash")))
        return;
    deviceConnection_->restClient()->saveConfigToFlash();
}

void ConfigurationService::loadConfigFromFlash()
{
    if (!ensureCanPerformOperations(tr("Load config from flash")))
        return;
    deviceConnection_->restClient()->loadConfigFromFlash();
}

void ConfigurationService::resetConfigToDefaults()
{
    if (!ensureCanPerformOperations(tr("Reset config to defaults")))
        return;
    deviceConnection_->restClient()->resetConfigToDefaults();
}
