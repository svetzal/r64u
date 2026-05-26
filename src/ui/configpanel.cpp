#include "configpanel.h"

#include "configitemspanel.h"

#include "models/configurationmodel.h"
#include "services/configurationservice.h"
#include "services/errorhandler.h"
#include "utils/logging.h"

#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

ConfigPanel::ConfigPanel(ConfigurationService *configService, ErrorHandler *errorHandler,
                         QWidget *parent)
    : QWidget(parent), configService_(configService), errorHandler_(errorHandler),
      configModel_(new ConfigurationModel(this))
{
    Q_ASSERT(configService_ && "ConfigurationService is required");
    Q_ASSERT(errorHandler_ && "ErrorHandler is required");

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
    connect(saveToFlashAction_, &QAction::triggered, this, &ConfigPanel::onSaveToFlash);

    loadFromFlashAction_ = toolBar_->addAction(tr("Load from Flash"));
    loadFromFlashAction_->setToolTip(tr("Revert to last saved settings"));
    connect(loadFromFlashAction_, &QAction::triggered, this, &ConfigPanel::onLoadFromFlash);

    resetToDefaultsAction_ = toolBar_->addAction(tr("Reset to Defaults"));
    resetToDefaultsAction_->setToolTip(tr("Reset all settings to factory defaults"));
    connect(resetToDefaultsAction_, &QAction::triggered, this, &ConfigPanel::onResetToDefaults);

    toolBar_->addSeparator();

    refreshAction_ = toolBar_->addAction(tr("Refresh"));
    refreshAction_->setToolTip(tr("Reload all configuration from device"));
    connect(refreshAction_, &QAction::triggered, this, &ConfigPanel::onRefresh);

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
    categoryList_->setStyleSheet("QListWidget::item { padding: 4px 8px; }");
    connect(categoryList_, &QListWidget::currentItemChanged, this,
            &ConfigPanel::onCategorySelected);
    splitter_->addWidget(categoryList_);

    // Config items panel
    itemsPanel_ = new ConfigItemsPanel(configModel_);
    connect(itemsPanel_, &ConfigItemsPanel::itemChanged, this, &ConfigPanel::onItemEdited);
    splitter_->addWidget(itemsPanel_);

    // Set splitter sizes (category list gets ~25%, items panel gets ~75%)
    splitter_->setSizes({200, 600});

    layout->addWidget(splitter_, 1);
}

void ConfigPanel::setupConnections()
{
    // Connect ConfigurationService signals
    connect(configService_, &ConfigurationService::configCategoriesReceived, this,
            &ConfigPanel::onCategoriesReceived);
    connect(configService_, &ConfigurationService::configCategoryItemsReceived, this,
            &ConfigPanel::onCategoryItemsReceived);
    connect(configService_, &ConfigurationService::configSavedToFlash, this,
            &ConfigPanel::onSavedToFlash);
    connect(configService_, &ConfigurationService::configLoadedFromFlash, this,
            &ConfigPanel::onLoadedFromFlash);
    connect(configService_, &ConfigurationService::configResetToDefaults, this,
            &ConfigPanel::onResetComplete);
    connect(configService_, &ConfigurationService::configItemSet, this,
            &ConfigPanel::onItemSetResult);

    // Connect model signals
    if (configModel_) {
        connect(configModel_, &ConfigurationModel::dirtyStateChanged, this,
                &ConfigPanel::onDirtyStateChanged);
        connect(configModel_, &ConfigurationModel::categoriesChanged, this, [this]() {
            // Update category list when categories change
            if (!categoryList_ || !configModel_) {
                qCDebug(LogUi)
                    << "categoriesChanged: categoryList_ or configModel_ is null, skipping update";
                return;
            }
            categoryList_->clear();
            for (const QString &category : configModel_->categories()) {
                categoryList_->addItem(category);
            }
            // Select first category if available
            if (categoryList_->count() > 0) {
                categoryList_->setCurrentRow(0);
            }
        });
    }
}

void ConfigPanel::updateActions()
{
    bool canOperate = configService_ && configService_->canPerformOperations();
    if (saveToFlashAction_) {
        saveToFlashAction_->setEnabled(canOperate);
    }
    if (loadFromFlashAction_) {
        loadFromFlashAction_->setEnabled(canOperate);
    }
    if (resetToDefaultsAction_) {
        resetToDefaultsAction_->setEnabled(canOperate);
    }
    if (refreshAction_) {
        refreshAction_->setEnabled(canOperate);
    }
}

void ConfigPanel::refreshIfEmpty()
{
    if (configService_ && configService_->canPerformOperations() && categoryList_ &&
        categoryList_->count() == 0) {
        onRefresh();
    }
}

void ConfigPanel::onSaveToFlash()
{
    if (!configService_ || !configService_->canPerformOperations()) {
        errorHandler_->handleError(ErrorCategory::Connection, ErrorSeverity::Warning,
                                   tr("Not connected to device"));
        return;
    }

    errorHandler_->info(ErrorCategory::FileOperation, tr("Saving configuration to flash..."));
    configService_->saveConfigToFlash();
}

void ConfigPanel::onLoadFromFlash()
{
    if (!configService_ || !configService_->canPerformOperations()) {
        errorHandler_->handleError(ErrorCategory::Connection, ErrorSeverity::Warning,
                                   tr("Not connected to device"));
        return;
    }

    errorHandler_->info(ErrorCategory::FileOperation, tr("Loading configuration from flash..."));
    configService_->loadConfigFromFlash();
}

void ConfigPanel::onResetToDefaults()
{
    if (!configService_ || !configService_->canPerformOperations()) {
        errorHandler_->handleError(ErrorCategory::Connection, ErrorSeverity::Warning,
                                   tr("Not connected to device"));
        return;
    }

    // Show confirmation dialog
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Reset to Defaults"));
    msgBox.setText(tr("This will reset all configuration settings to factory defaults.\n\n"
                      "Are you sure you want to continue?"));
    msgBox.setIcon(QMessageBox::Warning);
    QPushButton *resetButton =
        msgBox.addButton(tr("Reset to Defaults"), QMessageBox::DestructiveRole);
    msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
    msgBox.exec();

    if (msgBox.clickedButton() == resetButton) {
        errorHandler_->info(ErrorCategory::FileOperation,
                            tr("Resetting configuration to defaults..."));
        configService_->resetConfigToDefaults();
    }
}

void ConfigPanel::onRefresh()
{
    if (!configService_ || !configService_->canPerformOperations()) {
        errorHandler_->handleError(ErrorCategory::Connection, ErrorSeverity::Warning,
                                   tr("Not connected to device"));
        return;
    }

    errorHandler_->info(ErrorCategory::FileOperation, tr("Refreshing configuration..."));
    configService_->getConfigCategories();
}

void ConfigPanel::onCategoriesReceived(const QStringList &categories)
{
    if (configModel_) {
        configModel_->setCategories(categories);
    }
    errorHandler_->info(ErrorCategory::FileOperation,
                        tr("Loaded %1 configuration categories").arg(categories.size()));

    // Load items for each category
    for (const QString &category : categories) {
        configService_->getConfigCategoryItems(category);
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
    if (configModel_) {
        configModel_->clearDirtyFlags();
    }
    errorHandler_->info(ErrorCategory::FileOperation, tr("Configuration saved to flash"));
}

void ConfigPanel::onLoadedFromFlash()
{
    // Reload categories to get fresh data
    onRefresh();
    errorHandler_->info(ErrorCategory::FileOperation, tr("Configuration loaded from flash"));
}

void ConfigPanel::onResetComplete()
{
    // Reload categories to get fresh data
    onRefresh();
    errorHandler_->info(ErrorCategory::FileOperation, tr("Configuration reset to defaults"));
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
        if (itemsPanel_) {
            itemsPanel_->setCategory(QString());
        }
        return;
    }

    QString category = current->text();
    if (itemsPanel_) {
        itemsPanel_->setCategory(category);
    }

    // Load items for this category if not already loaded
    if (configModel_ && configModel_->itemCount(category) == 0 && configService_ &&
        configService_->canPerformOperations()) {
        configService_->getConfigCategoryItems(category);
    }
}

void ConfigPanel::onItemEdited(const QString &category, const QString &item, const QVariant &value)
{
    if (!configService_ || !configService_->canPerformOperations()) {
        errorHandler_->handleError(ErrorCategory::Connection, ErrorSeverity::Warning,
                                   tr("Not connected - changes are local only"));
        return;
    }

    // Send update to device immediately
    errorHandler_->info(ErrorCategory::FileOperation, tr("Updating %1...").arg(item));
    configService_->setConfigItem(category, item, value);
}

void ConfigPanel::onItemSetResult(const QString &category, const QString &item)
{
    Q_UNUSED(category)
    errorHandler_->info(ErrorCategory::FileOperation, tr("%1 updated").arg(item));
}
