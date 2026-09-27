#include "configitemspanel.h"

#include "core/themecore.h"
#include "models/configurationmodel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
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
// Grid columns per label/editor pair: label, editor, then an empty spacer
constexpr int kColumnsPerPair = 3;
// Spacers outweigh editors so spare width lands after each pair, never in the labels
constexpr int kEditorColumnStretch = 1;
constexpr int kSpacerColumnStretch = 100;
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
    const int cap = editorMaxWidth(combo);
    combo->setMaximumWidth(std::max(cap, combo->sizeHint().width()));
    // A long option may elide in a narrow pane rather than widen the grid past it
    combo->setMinimumWidth(std::min(cap, combo->minimumSizeHint().width()));
}

// Width an editor is meant to take up: its cap, or its natural width when uncapped
int intendedEditorWidth(const QWidget *editor)
{
    return editor->maximumWidth() < QWIDGETSIZE_MAX ? editor->maximumWidth()
                                                    : editor->sizeHint().width();
}

// Width every editor column takes so the pairs stay equally wide: room for the widest
// editor when the viewport has it, else what fits, but never below the plain editor cap
int editorColumnWidth(int widestEditor, int plainCap, int roomPerEditor)
{
    return std::max(plainCap, std::min(widestEditor, roomPerEditor));
}

// Show a new child now rather than through the queued show a layout schedules for
// children of a visible widget, so the grid can place it at once (revealCategory()
// needs current geometry). A hidden parent still keeps it hidden.
void showNow(QWidget *widget)
{
    widget->setVisible(true);
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
    // The width the pairs share changes with the pane and with the vertical scroll bar
    scrollArea_->viewport()->installEventFilter(this);
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
    if (isFilterActive()) {
        revealCategory(category);
    } else {
        refresh();
    }
}

void ConfigItemsPanel::revealCategory(const QString &category)
{
    if (!isFilterActive() || sections_.size() < 2 || !scrollArea_->isVisible()) {
        return;
    }
    const auto section =
        std::find_if(sections_.cbegin(), sections_.cend(), [&category](const Section &candidate) {
            return candidate.category == category;
        });
    if (section == sections_.cend() || section->header == nullptr) {
        return;
    }

    fitContentToLayout();

    const int top = section->header->geometry().top();
    int bottom = section->header->geometry().bottom() + 1;
    for (const Entry &entry : section->entries) {
        bottom = std::max(
            {bottom, entry.label->geometry().bottom() + 1, entry.editor->geometry().bottom() + 1});
    }
    QScrollBar *vbar = scrollArea_->verticalScrollBar();
    vbar->setValue(configfiltercore::scrollValueToReveal(
        vbar->value(), scrollArea_->viewport()->height(), top, bottom));
}

void ConfigItemsPanel::fitContentToLayout()
{
    // A freshly rebuilt grid is only laid out, and the scroll area only resizes the
    // content to it, once posted events run. Do both now so geometry is current.
    grid_->activate();
    QEvent layoutRequest(QEvent::LayoutRequest);
    QCoreApplication::sendEvent(scrollArea_, &layoutRequest);
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
        configfiltercore::CategoryItems group;
        group.category = category;
        group.items = model_->itemNames(category);
        for (const QString &item : group.items) {
            const ConfigItemInfo info = model_->itemInfo(category, item);
            const QStringList choices = configfiltercore::editorChoices(info.value, info.options);
            if (!choices.isEmpty()) {
                group.choices.insert(item, choices);
            }
        }
        snapshot.append(group);
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
    showNow(entry.label);
    showNow(entry.editor);
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
    showNow(header);
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
            grid_->addWidget(section.header, row, 0, 1, pairs * kColumnsPerPair);
            ++row;
        }
        for (int i = 0; i < section.entries.size(); ++i) {
            const Entry &entry = section.entries.at(i);
            const int labelColumn = (i % pairs) * kColumnsPerPair;
            const int entryRow = row + (i / pairs);
            grid_->addWidget(entry.label, entryRow, labelColumn, Qt::AlignRight | Qt::AlignVCenter);
            grid_->addWidget(entry.editor, entryRow, labelColumn + 1);
        }
        row += (section.entries.size() + pairs - 1) / pairs;
    }

    // The spacer after each pair takes an equal share of the spare width, so spare width
    // lands after the pairs rather than in the label columns
    for (int pair = 0; pair < pairs; ++pair) {
        const int labelColumn = pair * kColumnsPerPair;
        grid_->setColumnStretch(labelColumn + 1, kEditorColumnStretch);
        grid_->setColumnStretch(labelColumn + 2, kSpacerColumnStretch);
    }
    // Keep rows packed at the top when the viewport is taller than the content
    grid_->setRowStretch(row, 1);
    updatePairWidths();
}

void ConfigItemsPanel::updatePairWidths()
{
    if (grid_ == nullptr || sections_.isEmpty()) {
        return;
    }
    int widestLabel = 0;
    int widestEditor = 0;
    int plainCap = QWIDGETSIZE_MAX;
    for (const Section &section : sections_) {
        for (const Entry &entry : section.entries) {
            widestLabel = std::max(widestLabel, entry.label->sizeHint().width());
            widestEditor = std::max(widestEditor, intendedEditorWidth(entry.editor));
            plainCap = std::min(plainCap, editorMaxWidth(entry.editor));
        }
    }

    // Give every pair the widest label and editor in the grid, so each pair starts at
    // the left of its share of the width whatever the label lengths in this category.
    // Editors give way first when that would make the grid wider than the viewport.
    // The grid puts a spacing between columns holding items. Spacer columns hold nothing,
    // except where a category header spans the row.
    const int pairs = columnCount_;
    const bool withHeaders =
        std::any_of(sections_.cbegin(), sections_.cend(),
                    [](const Section &section) { return section.header != nullptr; });
    const int gaps = (withHeaders ? kColumnsPerPair * pairs : 2 * pairs) - 1;
    const int room = scrollArea_->viewport()->width() - (2 * kGridMargin) - (gaps * kGridSpacing) -
                     (pairs * widestLabel);
    const int editorWidth = editorColumnWidth(widestEditor, plainCap, room / pairs);
    for (int pair = 0; pair < pairs; ++pair) {
        const int labelColumn = pair * kColumnsPerPair;
        if (grid_->columnMinimumWidth(labelColumn) != widestLabel) {
            grid_->setColumnMinimumWidth(labelColumn, widestLabel);
        }
        // Setting a minimum always invalidates the grid, so skip unchanged ones
        if (grid_->columnMinimumWidth(labelColumn + 1) != editorWidth) {
            grid_->setColumnMinimumWidth(labelColumn + 1, editorWidth);
        }
    }
}

bool ConfigItemsPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == scrollArea_->viewport() && event->type() == QEvent::Resize) {
        updatePairWidths();
    }
    return QWidget::eventFilter(watched, event);
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
    // A dropdown when the device offers options or the value reads as a boolean
    const QStringList choices = configfiltercore::editorChoices(value, options);
    if (!choices.isEmpty()) {
        auto *combo = new QComboBox();
        combo->addItems(choices);
        // Device options must match exactly; the built boolean pair matches in any case
        const Qt::CaseSensitivity sensitivity =
            options.isEmpty() ? Qt::CaseInsensitive : Qt::CaseSensitive;
        const auto index = choices.indexOf(value.toString(), 0, sensitivity);
        if (index >= 0) {
            combo->setCurrentIndex(static_cast<int>(index));
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
        checkBox->setMaximumWidth(editorMaxWidth(checkBox));
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
