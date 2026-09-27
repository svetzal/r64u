#include "configitemspanel.h"

#include "core/themecore.h"
#include "models/configurationmodel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

using configfiltercore::itemKey;

namespace {
constexpr int kGridMargin = 8;
constexpr int kGridSpacing = 8;
constexpr int kHeaderPaddingTop = 6;
constexpr int kEditorWidthChars = 21;
constexpr int kEditorWidthPadding = 14;
constexpr int kDefaultSpinMin = -999999;
constexpr int kDefaultSpinMax = 999999;
const char *const kBoldLabelStyle = "QLabel { font-weight: bold; }";

// Widest an editor may grow, so editors do not stretch across a wide grid column
int editorMaxWidth(const QWidget *editor)
{
    return (editor->fontMetrics().averageCharWidth() * kEditorWidthChars) + kEditorWidthPadding;
}

// Cap a combo like other editors, but never below the width its longest option needs.
// Call after the items are added.
void capComboWidth(QComboBox *combo)
{
    combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    combo->ensurePolished();  // measure with the stylesheet's padding and border applied
    combo->setMaximumWidth(std::max(editorMaxWidth(combo), combo->sizeHint().width()));
}
}  // namespace

ConfigItemsPanel::ConfigItemsPanel(ConfigurationModel *model, QWidget *parent)
    : QWidget(parent), model_(model)
{
    setupUi();

    // Connect to model signals
    connect(model_, &ConfigurationModel::categoryItemsChanged, this,
            &ConfigItemsPanel::onCategoryItemsChanged);
    connect(model_, &ConfigurationModel::itemValueChanged, this,
            &ConfigItemsPanel::onItemValueChanged);
    connect(model_, &ConfigurationModel::dirtyStateChanged, this,
            &ConfigItemsPanel::onDirtyStateChanged);
}

void ConfigItemsPanel::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    scrollArea_ = new QScrollArea();
    scrollArea_->setWidgetResizable(true);
    scrollArea_->setFrameStyle(QFrame::NoFrame);

    scrollContent_ = new QWidget();
    scrollArea_->setWidget(scrollContent_);
    mainLayout->addWidget(scrollArea_);

    // Empty state label
    emptyLabel_ = new QLabel();
    emptyLabel_->setObjectName(QStringLiteral("configEmptyLabel"));
    emptyLabel_->setTextFormat(Qt::PlainText);  // the message quotes the typed filter verbatim
    emptyLabel_->setAlignment(Qt::AlignCenter);
    emptyLabel_->setStyleSheet(
        QStringLiteral("QLabel { color: %1; }").arg(themecore::currentTokens().textMuted.name()));
    mainLayout->addWidget(emptyLabel_);

    showEmptyState(tr("Select a category to view configuration items."));
}

void ConfigItemsPanel::setCategory(const QString &category)
{
    currentCategory_ = category;
    if (!isFilterActive()) {
        refresh();
    }
}

void ConfigItemsPanel::setFilter(const QString &text)
{
    if (text == filter_) {
        return;
    }
    filter_ = text;
    refresh();
}

bool ConfigItemsPanel::isFilterActive() const
{
    return configfiltercore::isFilterActive(filter_);
}

QStringList ConfigItemsPanel::visibleItems() const
{
    QStringList keys;
    for (const Section &section : sections_) {
        for (const Entry &entry : section.entries) {
            keys.append(itemKey(entry.category, entry.item));
        }
    }
    return keys;
}

QStringList ConfigItemsPanel::headerCategories() const
{
    QStringList categories;
    for (const Section &section : sections_) {
        if (section.header != nullptr) {
            categories.append(section.category);
        }
    }
    return categories;
}

void ConfigItemsPanel::refresh()
{
    clearItems();

    if (isFilterActive()) {
        populateFiltered();
    } else {
        populateCategory();
    }

    columnCount_ = configfiltercore::columnCountForWidth(width());
    relayout();
}

void ConfigItemsPanel::clearItems()
{
    for (const Section &section : sections_) {
        delete section.header;
        for (const Entry &entry : section.entries) {
            delete entry.label;
            delete entry.editor;
        }
    }
    sections_.clear();
    itemLabels_.clear();

    delete grid_;
    grid_ = nullptr;
}

void ConfigItemsPanel::showEmptyState(const QString &message)
{
    emptyLabel_->setText(message);
    scrollArea_->setVisible(false);
    emptyLabel_->setVisible(true);
}

void ConfigItemsPanel::showItems()
{
    scrollArea_->setVisible(true);
    emptyLabel_->setVisible(false);
}

void ConfigItemsPanel::populateCategory()
{
    if (currentCategory_.isEmpty() || !model_->hasCategory(currentCategory_)) {
        showEmptyState(tr("Select a category to view configuration items."));
        return;
    }
    showItems();

    Section section;
    section.category = currentCategory_;
    QStringList items = model_->itemNames(currentCategory_);
    items.sort(Qt::CaseInsensitive);
    for (const QString &itemName : items) {
        section.entries.append(createEntry(currentCategory_, itemName));
    }
    sections_.append(section);
}

void ConfigItemsPanel::populateFiltered()
{
    const QList<configfiltercore::CategoryItems> groups =
        configfiltercore::filterSnapshot(modelSnapshot(), filter_);
    if (groups.isEmpty()) {
        showEmptyState(tr("No configuration items match \"%1\".")
                           .arg(configfiltercore::normalizeFilter(filter_)));
        return;
    }
    showItems();

    const bool withHeaders = groups.size() > 1;
    for (const configfiltercore::CategoryItems &group : groups) {
        Section section;
        section.category = group.category;
        if (withHeaders) {
            section.header = createHeader(group.category);
        }
        for (const QString &itemName : group.items) {
            section.entries.append(createEntry(group.category, itemName));
        }
        sections_.append(section);
    }
}

QList<configfiltercore::CategoryItems> ConfigItemsPanel::modelSnapshot() const
{
    QList<configfiltercore::CategoryItems> snapshot;
    for (const QString &category : model_->categories()) {
        snapshot.append({category, model_->itemNames(category)});
    }
    return snapshot;
}

ConfigItemsPanel::Entry ConfigItemsPanel::createEntry(const QString &category, const QString &item)
{
    const ConfigItemInfo info = model_->itemInfo(category, item);
    const QString key = itemKey(category, item);

    Entry entry;
    entry.category = category;
    entry.item = item;

    entry.label = new QLabel(item, scrollContent_);
    entry.label->setObjectName(key);
    if (info.isDirty) {
        entry.label->setStyleSheet(kBoldLabelStyle);
    }
    itemLabels_[key] = entry.label;

    entry.editor =
        createEditorWidget(category, item, info.value, info.options, info.minValue, info.maxValue);
    entry.editor->setObjectName(key + QStringLiteral(":editor"));
    entry.editor->setParent(scrollContent_);
    return entry;
}

QLabel *ConfigItemsPanel::createHeader(const QString &category)
{
    auto *header = new QLabel(category, scrollContent_);
    header->setObjectName(QStringLiteral("categoryHeader"));
    header->setStyleSheet(
        QStringLiteral("QLabel { font-weight: bold; color: %1; padding-top: %2px; }")
            .arg(themecore::currentTokens().textSecondary.name())
            .arg(kHeaderPaddingTop));
    return header;
}

void ConfigItemsPanel::relayout()
{
    delete grid_;
    grid_ = new QGridLayout(scrollContent_);
    grid_->setContentsMargins(kGridMargin, kGridMargin, kGridMargin, kGridMargin);
    grid_->setSpacing(kGridSpacing);

    const int pairs = columnCount_;
    int row = 0;
    for (const Section &section : sections_) {
        if (section.header != nullptr) {
            grid_->addWidget(section.header, row, 0, 1, pairs * 2);
            ++row;
        }
        for (int i = 0; i < section.entries.size(); ++i) {
            const Entry &entry = section.entries.at(i);
            const int column = i % pairs;
            const int entryRow = row + (i / pairs);
            grid_->addWidget(entry.label, entryRow, column * 2, Qt::AlignRight | Qt::AlignVCenter);
            grid_->addWidget(entry.editor, entryRow, (column * 2) + 1);
        }
        row += (section.entries.size() + pairs - 1) / pairs;
    }

    grid_->setColumnStretch(1, 1);
    if (pairs == 2) {
        grid_->setColumnStretch(3, 1);
    }
    // Keep rows packed at the top when the viewport is taller than the content
    grid_->setRowStretch(row, 1);
}

void ConfigItemsPanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    const int wanted = configfiltercore::columnCountForWidth(width());
    if (wanted == columnCount_) {
        return;
    }
    columnCount_ = wanted;
    if (!sections_.isEmpty()) {
        relayout();
    }
}

QWidget *ConfigItemsPanel::createEditorWidget(const QString &category, const QString &itemName,
                                              const QVariant &value, const QStringList &options,
                                              const QVariant &minValue, const QVariant &maxValue)
{
    // If options are provided, use combo box
    if (!options.isEmpty()) {
        auto *combo = new QComboBox();
        combo->addItems(options);
        int index = options.indexOf(value.toString());
        if (index >= 0) {
            combo->setCurrentIndex(index);
        }
        capComboWidth(combo);
        connect(combo, &QComboBox::currentTextChanged, this,
                [this, category, itemName](const QString &text) {
                    emit itemChanged(category, itemName, text);
                    model_->setValue(category, itemName, text);
                });
        return combo;
    }

    // Check value type
    auto type = static_cast<QMetaType::Type>(value.typeId());

    if (type == QMetaType::Bool) {
        auto *checkBox = new QCheckBox();
        checkBox->setChecked(value.toBool());
        connect(checkBox, &QCheckBox::toggled, this, [this, category, itemName](bool checked) {
            emit itemChanged(category, itemName, checked);
            model_->setValue(category, itemName, checked);
        });
        return checkBox;
    }

    if (type == QMetaType::Int || type == QMetaType::LongLong) {
        auto *spinBox = new QSpinBox();
        // Use min/max from metadata if available
        if (minValue.isValid() && maxValue.isValid()) {
            spinBox->setRange(minValue.toInt(), maxValue.toInt());
        } else {
            spinBox->setRange(kDefaultSpinMin, kDefaultSpinMax);
        }
        spinBox->setValue(value.toInt());
        spinBox->setMaximumWidth(editorMaxWidth(spinBox));
        connect(spinBox, &QSpinBox::valueChanged, this, [this, category, itemName](int val) {
            emit itemChanged(category, itemName, val);
            model_->setValue(category, itemName, val);
        });
        return spinBox;
    }

    if (type == QMetaType::Double) {
        auto *lineEdit = new QLineEdit();
        lineEdit->setText(value.toString());
        lineEdit->setMaximumWidth(editorMaxWidth(lineEdit));
        connect(lineEdit, &QLineEdit::editingFinished, this,
                [this, category, itemName, lineEdit]() {
                    QString text = lineEdit->text();
                    bool ok = false;
                    double d = text.toDouble(&ok);
                    if (ok) {
                        emit itemChanged(category, itemName, d);
                        model_->setValue(category, itemName, d);
                    } else {
                        emit itemChanged(category, itemName, text);
                        model_->setValue(category, itemName, text);
                    }
                });
        return lineEdit;
    }

    // Check for string that looks like boolean
    QString strVal = value.toString().toLower();
    if (strVal == "yes" || strVal == "no" || strVal == "enabled" || strVal == "disabled" ||
        strVal == "on" || strVal == "off" || strVal == "true" || strVal == "false") {
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
        capComboWidth(combo);
        connect(combo, &QComboBox::currentTextChanged, this,
                [this, category, itemName](const QString &text) {
                    emit itemChanged(category, itemName, text);
                    model_->setValue(category, itemName, text);
                });
        return combo;
    }

    // Default: string line edit
    auto *lineEdit = new QLineEdit();
    lineEdit->setText(value.toString());
    lineEdit->setMaximumWidth(editorMaxWidth(lineEdit));
    connect(lineEdit, &QLineEdit::editingFinished, this, [this, category, itemName, lineEdit]() {
        emit itemChanged(category, itemName, lineEdit->text());
        model_->setValue(category, itemName, lineEdit->text());
    });
    return lineEdit;
}

void ConfigItemsPanel::updateLabelStyle(const QString &key, bool isDirty)
{
    if (auto *label = itemLabels_.value(key)) {
        label->setStyleSheet(isDirty ? QString::fromLatin1(kBoldLabelStyle) : QString());
    }
}

void ConfigItemsPanel::onCategoryItemsChanged(const QString &category)
{
    if (isFilterActive() || category == currentCategory_) {
        refresh();
    }
}

void ConfigItemsPanel::onItemValueChanged(const QString &category, const QString &item,
                                          const QVariant &value)
{
    Q_UNUSED(value)
    updateLabelStyle(itemKey(category, item), model_->isItemDirty(category, item));
}

void ConfigItemsPanel::onDirtyStateChanged(bool isDirty)
{
    Q_UNUSED(isDirty)

    // Refresh every shown label from the model's per-item dirty flags
    for (const Section &section : sections_) {
        for (const Entry &entry : section.entries) {
            updateLabelStyle(itemKey(entry.category, entry.item),
                             model_->isItemDirty(entry.category, entry.item));
        }
    }
}
