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

IRestClient *ConfigurationService::restClient() const
{
    return deviceConnection_->restClient();
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
    performOperation(tr("Get config categories"), [](IRestClient *c) { c->getConfigCategories(); });
}

void ConfigurationService::getConfigCategoryItems(const QString &category)
{
    performOperation(tr("Get config category items"),
                     [&](IRestClient *c) { c->getConfigCategoryItems(category); });
}

void ConfigurationService::setConfigItem(const QString &category, const QString &item,
                                         const QVariant &value)
{
    performOperation(tr("Set config item"),
                     [&](IRestClient *c) { c->setConfigItem(category, item, value); });
}

void ConfigurationService::saveConfigToFlash()
{
    performOperation(tr("Save config to flash"), [](IRestClient *c) { c->saveConfigToFlash(); });
}

void ConfigurationService::loadConfigFromFlash()
{
    performOperation(tr("Load config from flash"),
                     [](IRestClient *c) { c->loadConfigFromFlash(); });
}

void ConfigurationService::resetConfigToDefaults()
{
    performOperation(tr("Reset config to defaults"),
                     [](IRestClient *c) { c->resetConfigToDefaults(); });
}
