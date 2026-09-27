#ifndef CONFIGITEMSPANEL_H
#define CONFIGITEMSPANEL_H

#include "core/configfiltercore.h"

#include <QHash>
#include <QList>
#include <QScrollArea>
#include <QVariant>
#include <QWidget>

class ConfigurationModel;
class QGridLayout;
class QLabel;
class QCheckBox;
class QComboBox;
class QSpinBox;
class QLineEdit;
class QResizeEvent;

/**
 * @brief Widget that displays and edits configuration items.
 *
 * Shows the items of the selected category, or, while a filter is active,
 * every matching item across all loaded categories grouped under category
 * headers. Items are laid out in one or two label/editor columns depending
 * on the panel width.
 *
 * Creates appropriate edit widgets based on item value type:
 * - Boolean -> QCheckBox
 * - Enumerated options, or a string that reads as a boolean -> QComboBox
 *   offering configfiltercore::editorChoices()
 * - Integer -> QSpinBox
 * - String -> QLineEdit
 *
 * Modified items are displayed with bold labels.
 */
class ConfigItemsPanel : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a config items panel.
     * @param model Configuration model to edit.
     * @param parent Parent widget.
     */
    explicit ConfigItemsPanel(ConfigurationModel *model, QWidget *parent = nullptr);

    /**
     * @brief Sets the category to display.
     *
     * The category is remembered while a filter is active, but the filtered
     * view is kept until the filter is cleared; instead the category's group
     * is scrolled into view (see revealCategory()).
     *
     * @param category Category name, or empty to clear.
     */
    void setCategory(const QString &category);

    /**
     * @brief Scrolls a category's group of filter results fully into view.
     *
     * Applies only while a filter's matches span more than one category and
     * the category has a group among them; otherwise the scroll position is
     * left alone. A group taller than the viewport is scrolled so its header
     * sits at the top. Changes nothing but the scroll position.
     *
     * @param category Category name.
     */
    void revealCategory(const QString &category);

    /**
     * @brief Returns the currently selected category.
     * @return Category name.
     */
    [[nodiscard]] QString currentCategory() const { return currentCategory_; }

    /**
     * @brief Narrows the panel to items matching the filter text.
     *
     * Matching follows configfiltercore::matches(). Whitespace-only text
     * clears the filter and restores the selected category's items.
     *
     * @param text Raw filter text.
     */
    void setFilter(const QString &text);

    /**
     * @brief Returns the raw filter text.
     */
    [[nodiscard]] QString filter() const { return filter_; }

    /**
     * @brief Whether a filter is currently narrowing the panel.
     */
    [[nodiscard]] bool isFilterActive() const;

    /**
     * @brief Number of label/editor column pairs in the current layout.
     */
    [[nodiscard]] int columnCount() const { return columnCount_; }

    /**
     * @brief Keys ("Category/Item") of the items currently shown, in display order.
     */
    [[nodiscard]] QStringList visibleItems() const;

    /**
     * @brief Categories shown with a header, in display order.
     *
     * Headers appear only when a filter's matches span more than one category.
     */
    [[nodiscard]] QStringList headerCategories() const;

    /**
     * @brief Panel width at which items switch to two columns.
     */
    [[nodiscard]] static constexpr int twoColumnMinWidth()
    {
        return configfiltercore::kTwoColumnMinWidth;
    }

    /**
     * @brief Rebuilds the display from the model.
     */
    void refresh();

    /**
     * @brief Every category in the model with its loaded item names and the
     *        choices each item's editor offers, as the filter rules expect.
     */
    [[nodiscard]] QList<configfiltercore::CategoryItems> modelSnapshot() const;

signals:
    /**
     * @brief Emitted when a config item value is changed by the user.
     * @param category Category name.
     * @param item Item name.
     * @param value New value.
     */
    void itemChanged(const QString &category, const QString &item, const QVariant &value);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onCategoryItemsChanged(const QString &category);
    void onItemValueChanged(const QString &category, const QString &item, const QVariant &value);
    void onDirtyStateChanged(bool isDirty);

private:
    /// One label/editor pair for a config item.
    struct Entry
    {
        QString category;
        QString item;
        QLabel *label = nullptr;
        QWidget *editor = nullptr;
    };

    /// The entries of one category, with an optional header above them.
    struct Section
    {
        QString category;
        QLabel *header = nullptr;
        QList<Entry> entries;
    };

    void setupUi();
    void clearItems();
    void populateCategory();
    void populateFiltered();
    void showEmptyState(const QString &message);
    void showItems();
    [[nodiscard]] Entry createEntry(const QString &category, const QString &item);
    [[nodiscard]] QLabel *createHeader(const QString &category);
    QWidget *createEditorWidget(const QString &category, const QString &itemName,
                                const QVariant &value, const QStringList &options,
                                const QVariant &minValue = QVariant(),
                                const QVariant &maxValue = QVariant());
    void relayout();
    void fitContentToLayout();
    void updateLabelStyle(const QString &key, bool isDirty);

    ConfigurationModel *model_ = nullptr;
    QString currentCategory_;
    QString filter_;
    int columnCount_ = 1;

    // UI elements
    QScrollArea *scrollArea_ = nullptr;
    QWidget *scrollContent_ = nullptr;
    QGridLayout *grid_ = nullptr;
    QLabel *emptyLabel_ = nullptr;
    QList<Section> sections_;

    // Labels keyed by configfiltercore::itemKey, for dirty state styling
    QHash<QString, QLabel *> itemLabels_;
};

#endif  // CONFIGITEMSPANEL_H
