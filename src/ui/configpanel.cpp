#include "configpanel.h"
#include "configitemspanel.h"
#include "services/deviceconnection.h"
#include "models/configurationmodel.h"

#include <QVBoxLayout>
#include <QMessageBox>
#include <QPushButton>

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

    // Create splitter with category list on left, items panel on right
    splitter_ = new QSplitter(Qt::Horizontal);

    // Category list
    categoryList_ = new QListWidget();
    categoryList_->setMinimumWidth(150);
    categoryList_->setMaximumWidth(250);
    categoryList_->setAlternatingRowColors(true);
    categoryList_->setSpacing(2);
    // Match the styling of tree views with slightly more padding
    categoryList_->setStyleSheet(
        "QListWidget::item { padding: 4px 8px; }"
    );
    connect(categoryList_, &QListWidget::currentItemChanged,
            this, &ConfigPanel::onCategorySelected);
    splitter_->addWidget(categoryList_);

    // Config items panel
    itemsPanel_ = new ConfigItemsPanel(configModel_);
    connect(itemsPanel_, &ConfigItemsPanel::itemChanged,
            this, &ConfigPanel::onItemEdited);
    splitter_->addWidget(itemsPanel_);

    // Set splitter sizes (category list gets ~25%, items panel gets ~75%)
    splitter_->setSizes({200, 600});

    layout->addWidget(splitter_, 1);
}

void ConfigPanel::setupConnections()
{
    // Subscribe to device connection state changes
    connect(deviceConnection_, &DeviceConnection::stateChanged,
            this, &ConfigPanel::onConnectionStateChanged);

    // Connect model signals
    connect(configModel_, &ConfigurationModel::dirtyStateChanged,
            this, &ConfigPanel::onDirtyStateChanged);
    connect(configModel_, &ConfigurationModel::categoriesChanged,
            this, [this]() {
        // Update category list when categories change
        categoryList_->clear();
        for (const QString &category : configModel_->categories()) {
            categoryList_->addItem(category);
        }
        // Select first category if available
        if (categoryList_->count() > 0) {
            categoryList_->setCurrentRow(0);
        }
    });

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
    bool canOperate = deviceConnection_->canPerformOperations();
    saveToFlashAction_->setEnabled(canOperate);
    loadFromFlashAction_->setEnabled(canOperate);
    resetToDefaultsAction_->setEnabled(canOperate);
    refreshAction_->setEnabled(canOperate);
}

void ConfigPanel::onConnectionStateChanged()
{
    updateActions();

    bool canOperate = deviceConnection_->canPerformOperations();
    if (!canOperate) {
        // Could optionally clear the model here
    }
}

void ConfigPanel::refreshIfEmpty()
{
    if (deviceConnection_->canPerformOperations() && categoryList_->count() == 0) {
        onRefresh();
    }
}

void ConfigPanel::onSaveToFlash()
{
    if (!deviceConnection_->canPerformOperations()) {
        emit statusMessage(tr("Not connected"), 3000);
        return;
    }

    emit statusMessage(tr("Saving configuration to flash..."));
    deviceConnection_->restClient()->saveConfigToFlash();
}

void ConfigPanel::onLoadFromFlash()
{
    if (!deviceConnection_->canPerformOperations()) {
        emit statusMessage(tr("Not connected"), 3000);
        return;
    }

    emit statusMessage(tr("Loading configuration from flash..."));
    deviceConnection_->restClient()->loadConfigFromFlash();
}

void ConfigPanel::onResetToDefaults()
{
    if (!deviceConnection_->canPerformOperations()) {
        emit statusMessage(tr("Not connected"), 3000);
        return;
    }

    // Show confirmation dialog
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Reset to Defaults"));
    msgBox.setText(tr("This will reset all configuration settings to factory defaults.\n\n"
                      "Are you sure you want to continue?"));
    msgBox.setIcon(QMessageBox::Warning);
    QPushButton *resetButton = msgBox.addButton(tr("Reset to Defaults"), QMessageBox::DestructiveRole);
    msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
    msgBox.exec();

    if (msgBox.clickedButton() == resetButton) {
        emit statusMessage(tr("Resetting configuration to defaults..."));
        deviceConnection_->restClient()->resetConfigToDefaults();
    }
}

void ConfigPanel::onRefresh()
{
    if (!deviceConnection_->canPerformOperations()) {
        emit statusMessage(tr("Not connected"), 3000);
        return;
    }

    emit statusMessage(tr("Refreshing configuration..."));
    deviceConnection_->restClient()->getConfigCategories();
}

void ConfigPanel::onCategoriesReceived(const QStringList &categories)
{
    configModel_->setCategories(categories);
    emit statusMessage(tr("Loaded %1 configuration categories").arg(categories.size()), 3000);

    // Load items for each category
    for (const QString &category : categories) {
        deviceConnection_->restClient()->getConfigCategoryItems(category);
    }
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

void ConfigPanel::onCategorySelected(QListWidgetItem *current, QListWidgetItem *previous)
{
    Q_UNUSED(previous)

    if (!current) {
        itemsPanel_->setCategory(QString());
        return;
    }

    QString category = current->text();
    itemsPanel_->setCategory(category);

    // Load items for this category if not already loaded
    if (configModel_->itemCount(category) == 0 && deviceConnection_->canPerformOperations()) {
        deviceConnection_->restClient()->getConfigCategoryItems(category);
    }
}

void ConfigPanel::onItemEdited(const QString &category, const QString &item,
                                const QVariant &value)
{
    if (!deviceConnection_->canPerformOperations()) {
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
