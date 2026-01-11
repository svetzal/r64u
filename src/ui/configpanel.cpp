#include "configpanel.h"
#include "configitemspanel.h"
#include "services/deviceconnection.h"
#include "models/configurationmodel.h"

#include <QVBoxLayout>
#include <QMessageBox>

ConfigPanel::ConfigPanel(DeviceConnection *connection, QWidget *parent)
    : QWidget(parent)
    , deviceConnection_(connection)
    , configModel_(new ConfigurationModel(this))
{
    setupUi();
    setupConnections();
}

void ConfigPanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Create toolbar
    toolBar_ = new QToolBar();
    toolBar_->setMovable(false);
    toolBar_->setIconSize(QSize(16, 16));

    saveToFlashAction_ = toolBar_->addAction(tr("Save to Flash"));
    saveToFlashAction_->setToolTip(tr("Persist current settings to non-volatile storage"));
    connect(saveToFlashAction_, &QAction::triggered,
            this, &ConfigPanel::onSaveToFlash);

    loadFromFlashAction_ = toolBar_->addAction(tr("Load from Flash"));
    loadFromFlashAction_->setToolTip(tr("Revert to last saved settings"));
    connect(loadFromFlashAction_, &QAction::triggered,
            this, &ConfigPanel::onLoadFromFlash);

    resetToDefaultsAction_ = toolBar_->addAction(tr("Reset to Defaults"));
    resetToDefaultsAction_->setToolTip(tr("Reset all settings to factory defaults"));
    connect(resetToDefaultsAction_, &QAction::triggered,
            this, &ConfigPanel::onResetToDefaults);

    toolBar_->addSeparator();

    refreshAction_ = toolBar_->addAction(tr("Refresh"));
    refreshAction_->setToolTip(tr("Reload all configuration from device"));
    connect(refreshAction_, &QAction::triggered,
            this, &ConfigPanel::onRefresh);

    toolBar_->addSeparator();

    // Unsaved changes indicator
    unsavedIndicator_ = new QLabel();
    unsavedIndicator_->setStyleSheet("QLabel { color: orange; font-weight: bold; }");
    unsavedIndicator_->setVisible(false);
    toolBar_->addWidget(unsavedIndicator_);

    layout->addWidget(toolBar_);

    // Create tab widget with tabs on the left side
    categoryTabs_ = new QTabWidget();
    categoryTabs_->setTabPosition(QTabWidget::West);
    categoryTabs_->setDocumentMode(true);  // Cleaner look on macOS
    connect(categoryTabs_, &QTabWidget::currentChanged,
            this, &ConfigPanel::onCategoryTabChanged);

    layout->addWidget(categoryTabs_, 1);
}

void ConfigPanel::setupConnections()
{
    // Subscribe to device connection state changes
    connect(deviceConnection_, &DeviceConnection::stateChanged,
            this, &ConfigPanel::onConnectionStateChanged);

    // Connect model signals
    connect(configModel_, &ConfigurationModel::dirtyStateChanged,
            this, &ConfigPanel::onDirtyStateChanged);

    // Connect REST client signals
    connect(deviceConnection_->restClient(), &C64URestClient::configCategoriesReceived,
            this, &ConfigPanel::onCategoriesReceived);
    connect(deviceConnection_->restClient(), &C64URestClient::configCategoryItemsReceived,
            this, &ConfigPanel::onCategoryItemsReceived);
    connect(deviceConnection_->restClient(), &C64URestClient::configSavedToFlash,
            this, &ConfigPanel::onSavedToFlash);
    connect(deviceConnection_->restClient(), &C64URestClient::configLoadedFromFlash,
            this, &ConfigPanel::onLoadedFromFlash);
    connect(deviceConnection_->restClient(), &C64URestClient::configResetToDefaults,
            this, &ConfigPanel::onResetComplete);
    connect(deviceConnection_->restClient(), &C64URestClient::configItemSet,
            this, &ConfigPanel::onItemSetResult);
}

void ConfigPanel::updateActions()
{
    bool connected = deviceConnection_->isConnected();
    saveToFlashAction_->setEnabled(connected);
    loadFromFlashAction_->setEnabled(connected);
    resetToDefaultsAction_->setEnabled(connected);
    refreshAction_->setEnabled(connected);
}

void ConfigPanel::onConnectionStateChanged()
{
    updateActions();

    bool connected = deviceConnection_->isConnected();
    if (!connected) {
        // Could optionally clear the model here
    }
}

void ConfigPanel::refreshIfEmpty()
{
    if (deviceConnection_->isConnected() && categoryTabs_->count() == 0) {
        onRefresh();
    }
}

void ConfigPanel::onSaveToFlash()
{
    if (!deviceConnection_->isConnected()) {
        emit statusMessage(tr("Not connected"), 3000);
        return;
    }

    emit statusMessage(tr("Saving configuration to flash..."));
    deviceConnection_->restClient()->saveConfigToFlash();
}

void ConfigPanel::onLoadFromFlash()
{
    if (!deviceConnection_->isConnected()) {
        emit statusMessage(tr("Not connected"), 3000);
        return;
    }

    emit statusMessage(tr("Loading configuration from flash..."));
    deviceConnection_->restClient()->loadConfigFromFlash();
}

void ConfigPanel::onResetToDefaults()
{
    if (!deviceConnection_->isConnected()) {
        emit statusMessage(tr("Not connected"), 3000);
        return;
    }

    // Show confirmation dialog
    int reply = QMessageBox::warning(
        this,
        tr("Reset to Defaults"),
        tr("This will reset all configuration settings to factory defaults.\n\n"
           "Are you sure you want to continue?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        emit statusMessage(tr("Resetting configuration to defaults..."));
        deviceConnection_->restClient()->resetConfigToDefaults();
    }
}

void ConfigPanel::onRefresh()
{
    if (!deviceConnection_->isConnected()) {
        emit statusMessage(tr("Not connected"), 3000);
        return;
    }

    emit statusMessage(tr("Refreshing configuration..."));
    deviceConnection_->restClient()->getConfigCategories();
}

void ConfigPanel::onCategoriesReceived(const QStringList &categories)
{
    // Clear existing tabs and panels
    categoryTabs_->clear();
    itemsPanels_.clear();

    configModel_->setCategories(categories);

    // Create a tab for each category
    for (const QString &category : categories) {
        ConfigItemsPanel *panel = getOrCreateItemsPanel(category);
        categoryTabs_->addTab(panel, category);
    }

    emit statusMessage(tr("Loaded %1 configuration categories").arg(categories.size()), 3000);

    // Load items for each category
    for (const QString &category : categories) {
        deviceConnection_->restClient()->getConfigCategoryItems(category);
    }
}

ConfigItemsPanel* ConfigPanel::getOrCreateItemsPanel(const QString &category)
{
    if (!itemsPanels_.contains(category)) {
        auto *panel = new ConfigItemsPanel(configModel_);
        panel->setCategory(category);
        connect(panel, &ConfigItemsPanel::itemChanged,
                this, &ConfigPanel::onItemEdited);
        itemsPanels_[category] = panel;
    }
    return itemsPanels_[category];
}

void ConfigPanel::onCategoryItemsReceived(const QString &category,
                                           const QHash<QString, ConfigItemMetadata> &items)
{
    // Convert REST metadata to model's ConfigItemInfo format
    QHash<QString, ConfigItemInfo> infoItems;
    for (auto it = items.begin(); it != items.end(); ++it) {
        const ConfigItemMetadata &meta = it.value();
        ConfigItemInfo info;
        info.value = meta.current;
        info.defaultValue = meta.defaultValue;
        info.isDirty = false;

        // Use values array if available, otherwise use presets
        if (!meta.values.isEmpty()) {
            info.options = meta.values;
        } else if (!meta.presets.isEmpty()) {
            info.options = meta.presets;
        }

        // Store numeric range
        if (meta.hasRange) {
            info.minValue = meta.min;
            info.maxValue = meta.max;
        }

        infoItems[it.key()] = info;
    }
    configModel_->setCategoryItemsWithInfo(category, infoItems);
}

void ConfigPanel::onSavedToFlash()
{
    configModel_->clearDirtyFlags();
    emit statusMessage(tr("Configuration saved to flash"), 3000);
}

void ConfigPanel::onLoadedFromFlash()
{
    // Reload categories to get fresh data
    onRefresh();
    emit statusMessage(tr("Configuration loaded from flash"), 3000);
}

void ConfigPanel::onResetComplete()
{
    // Reload categories to get fresh data
    onRefresh();
    emit statusMessage(tr("Configuration reset to defaults"), 3000);
}

void ConfigPanel::onDirtyStateChanged(bool isDirty)
{
    if (isDirty) {
        unsavedIndicator_->setText(tr("Unsaved changes"));
        unsavedIndicator_->setVisible(true);
    } else {
        unsavedIndicator_->setVisible(false);
    }
}

void ConfigPanel::onCategoryTabChanged(int index)
{
    if (index < 0) {
        return;
    }

    QString category = categoryTabs_->tabText(index);

    // Load items for this category if not already loaded
    if (configModel_->itemCount(category) == 0 && deviceConnection_->isConnected()) {
        deviceConnection_->restClient()->getConfigCategoryItems(category);
    }
}

void ConfigPanel::onItemEdited(const QString &category, const QString &item,
                                const QVariant &value)
{
    if (!deviceConnection_->isConnected()) {
        emit statusMessage(tr("Not connected - changes are local only"), 3000);
        return;
    }

    // Send update to device immediately
    emit statusMessage(tr("Updating %1...").arg(item));
    deviceConnection_->restClient()->setConfigItem(category, item, value);
}

void ConfigPanel::onItemSetResult(const QString &category, const QString &item)
{
    Q_UNUSED(category)
    emit statusMessage(tr("%1 updated").arg(item), 2000);
}
