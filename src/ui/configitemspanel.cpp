#include "configitemspanel.h"
#include "models/configurationmodel.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QScrollArea>
#include <QFormLayout>

ConfigItemsPanel::ConfigItemsPanel(ConfigurationModel *model, QWidget *parent)
    : QWidget(parent)
    , model_(model)
{
    setupUi();

    // Connect to model signals
    connect(model_, &ConfigurationModel::categoryItemsChanged,
            this, &ConfigItemsPanel::onCategoryItemsChanged);
    connect(model_, &ConfigurationModel::itemValueChanged,
            this, &ConfigItemsPanel::onItemValueChanged);
    connect(model_, &ConfigurationModel::dirtyStateChanged,
            this, &ConfigItemsPanel::onDirtyStateChanged);
}

void ConfigItemsPanel::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    scrollArea_ = new QScrollArea();
    scrollArea_->setWidgetResizable(true);
    scrollArea_->setFrameStyle(QFrame::NoFrame);

    scrollContent_ = new QWidget();
    formLayout_ = new QFormLayout(scrollContent_);
    formLayout_->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    formLayout_->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout_->setContentsMargins(8, 8, 8, 8);
    formLayout_->setSpacing(8);

    scrollArea_->setWidget(scrollContent_);
    mainLayout->addWidget(scrollArea_);

    // Empty state label
    emptyLabel_ = new QLabel(tr("Select a category to view configuration items."));
    emptyLabel_->setAlignment(Qt::AlignCenter);
    emptyLabel_->setStyleSheet("QLabel { color: gray; }");
    mainLayout->addWidget(emptyLabel_);

    // Initially show empty label
    scrollArea_->setVisible(false);
    emptyLabel_->setVisible(true);
}

void ConfigItemsPanel::setCategory(const QString &category)
{
    if (currentCategory_ == category) {
        return;
    }

    currentCategory_ = category;
    refresh();
}

void ConfigItemsPanel::refresh()
{
    clearItems();

    if (currentCategory_.isEmpty() || !model_->hasCategory(currentCategory_)) {
        scrollArea_->setVisible(false);
        emptyLabel_->setVisible(true);
        return;
    }

    scrollArea_->setVisible(true);
    emptyLabel_->setVisible(false);

    populateItems();
}

void ConfigItemsPanel::clearItems()
{
    // Remove all items from the form layout
    while (formLayout_->rowCount() > 0) {
        formLayout_->removeRow(0);
    }

    itemLabels_.clear();
    itemEditors_.clear();
}

void ConfigItemsPanel::populateItems()
{
    QStringList items = model_->itemNames(currentCategory_);
    items.sort(Qt::CaseInsensitive);

    for (const QString &itemName : items) {
        ConfigItemInfo info = model_->itemInfo(currentCategory_, itemName);

        // Create label
        auto *label = new QLabel(itemName);
        itemLabels_[itemName] = label;

        // Apply bold if dirty
        if (info.isDirty) {
            label->setStyleSheet("QLabel { font-weight: bold; }");
        }

        // Create appropriate editor widget
        QWidget *editor = createEditorWidget(itemName, info.value, info.options,
                                              info.minValue, info.maxValue);
        itemEditors_[itemName] = editor;

        formLayout_->addRow(label, editor);
    }
}

QWidget* ConfigItemsPanel::createEditorWidget(const QString &itemName,
                                               const QVariant &value,
                                               const QStringList &options,
                                               const QVariant &minValue,
                                               const QVariant &maxValue)
{
    // If options are provided, use combo box
    if (!options.isEmpty()) {
        auto *combo = new QComboBox();
        combo->addItems(options);
        int index = options.indexOf(value.toString());
        if (index >= 0) {
            combo->setCurrentIndex(index);
        }
        connect(combo, &QComboBox::currentTextChanged, this,
                [this, itemName](const QString &text) {
            emit itemChanged(currentCategory_, itemName, text);
            model_->setValue(currentCategory_, itemName, text);
        });
        return combo;
    }

    // Check value type
    QMetaType::Type type = static_cast<QMetaType::Type>(value.typeId());

    if (type == QMetaType::Bool) {
        auto *checkBox = new QCheckBox();
        checkBox->setChecked(value.toBool());
        connect(checkBox, &QCheckBox::toggled, this,
                [this, itemName](bool checked) {
            emit itemChanged(currentCategory_, itemName, checked);
            model_->setValue(currentCategory_, itemName, checked);
        });
        return checkBox;
    }

    if (type == QMetaType::Int || type == QMetaType::LongLong) {
        auto *spinBox = new QSpinBox();
        // Use min/max from metadata if available
        if (minValue.isValid() && maxValue.isValid()) {
            spinBox->setRange(minValue.toInt(), maxValue.toInt());
        } else {
            spinBox->setRange(-999999, 999999);
        }
        spinBox->setValue(value.toInt());
        connect(spinBox, &QSpinBox::valueChanged, this,
                [this, itemName](int val) {
            emit itemChanged(currentCategory_, itemName, val);
            model_->setValue(currentCategory_, itemName, val);
        });
        return spinBox;
    }

    if (type == QMetaType::Double) {
        // Use spinbox for doubles that are integers, line edit otherwise
        auto *lineEdit = new QLineEdit();
        lineEdit->setText(value.toString());
        connect(lineEdit, &QLineEdit::editingFinished, this,
                [this, itemName, lineEdit]() {
            QString text = lineEdit->text();
            bool ok = false;
            double d = text.toDouble(&ok);
            if (ok) {
                emit itemChanged(currentCategory_, itemName, d);
                model_->setValue(currentCategory_, itemName, d);
            } else {
                emit itemChanged(currentCategory_, itemName, text);
                model_->setValue(currentCategory_, itemName, text);
            }
        });
        return lineEdit;
    }

    // Check for string that looks like boolean
    QString strVal = value.toString().toLower();
    if (strVal == "yes" || strVal == "no" ||
        strVal == "enabled" || strVal == "disabled" ||
        strVal == "on" || strVal == "off" ||
        strVal == "true" || strVal == "false") {
        auto *combo = new QComboBox();
        if (strVal == "yes" || strVal == "no") {
            combo->addItems({"Yes", "No"});
            combo->setCurrentText(strVal == "yes" ? "Yes" : "No");
        } else if (strVal == "enabled" || strVal == "disabled") {
            combo->addItems({"Enabled", "Disabled"});
            combo->setCurrentText(strVal == "enabled" ? "Enabled" : "Disabled");
        } else if (strVal == "on" || strVal == "off") {
            combo->addItems({"On", "Off"});
            combo->setCurrentText(strVal == "on" ? "On" : "Off");
        } else {
            combo->addItems({"True", "False"});
            combo->setCurrentText(strVal == "true" ? "True" : "False");
        }
        connect(combo, &QComboBox::currentTextChanged, this,
                [this, itemName](const QString &text) {
            emit itemChanged(currentCategory_, itemName, text);
            model_->setValue(currentCategory_, itemName, text);
        });
        return combo;
    }

    // Default: string line edit
    auto *lineEdit = new QLineEdit();
    lineEdit->setText(value.toString());
    connect(lineEdit, &QLineEdit::editingFinished, this,
            [this, itemName, lineEdit]() {
        emit itemChanged(currentCategory_, itemName, lineEdit->text());
        model_->setValue(currentCategory_, itemName, lineEdit->text());
    });
    return lineEdit;
}

void ConfigItemsPanel::updateLabelStyle(const QString &itemName, bool isDirty)
{
    if (auto *label = itemLabels_.value(itemName)) {
        if (isDirty) {
            label->setStyleSheet("QLabel { font-weight: bold; }");
        } else {
            label->setStyleSheet("");
        }
    }
}

void ConfigItemsPanel::onCategoryItemsChanged(const QString &category)
{
    if (category == currentCategory_) {
        refresh();
    }
}

void ConfigItemsPanel::onItemValueChanged(const QString &category,
                                           const QString &item,
                                           const QVariant &value)
{
    Q_UNUSED(value)

    if (category == currentCategory_) {
        // Update dirty state styling
        bool isDirty = model_->isItemDirty(category, item);
        updateLabelStyle(item, isDirty);
    }
}

void ConfigItemsPanel::onDirtyStateChanged(bool isDirty)
{
    Q_UNUSED(isDirty)

    // Refresh all label styles when dirty state changes
    if (!currentCategory_.isEmpty()) {
        QStringList items = model_->itemNames(currentCategory_);
        for (const QString &itemName : items) {
            bool itemDirty = model_->isItemDirty(currentCategory_, itemName);
            updateLabelStyle(itemName, itemDirty);
        }
    }
}
