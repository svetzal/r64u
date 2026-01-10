/**
 * @file configitemspanel.h
 * @brief Panel displaying editable configuration items for a selected category.
 */

#ifndef CONFIGITEMSPANEL_H
#define CONFIGITEMSPANEL_H

#include <QWidget>
#include <QScrollArea>
#include <QFormLayout>
#include <QHash>
#include <QVariant>

class ConfigurationModel;
class QLabel;
class QCheckBox;
class QComboBox;
class QSpinBox;
class QLineEdit;

/**
 * @brief Widget that displays and edits configuration items for a category.
 *
 * Creates appropriate edit widgets based on item value type:
 * - Boolean -> QCheckBox
 * - Enumerated options -> QComboBox
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
     * @param category Category name, or empty to clear.
     */
    void setCategory(const QString &category);

    /**
     * @brief Returns the currently displayed category.
     * @return Category name.
     */
    [[nodiscard]] QString currentCategory() const { return currentCategory_; }

    /**
     * @brief Refreshes the display for the current category.
     */
    void refresh();

signals:
    /**
     * @brief Emitted when a config item value is changed by the user.
     * @param category Category name.
     * @param item Item name.
     * @param value New value.
     */
    void itemChanged(const QString &category, const QString &item,
                     const QVariant &value);

private slots:
    void onCategoryItemsChanged(const QString &category);
    void onItemValueChanged(const QString &category, const QString &item,
                            const QVariant &value);
    void onDirtyStateChanged(bool isDirty);

private:
    void setupUi();
    void clearItems();
    void populateItems();
    QWidget* createEditorWidget(const QString &itemName, const QVariant &value,
                                const QStringList &options);
    void updateLabelStyle(const QString &itemName, bool isDirty);

    ConfigurationModel *model_ = nullptr;
    QString currentCategory_;

    // UI elements
    QScrollArea *scrollArea_ = nullptr;
    QWidget *scrollContent_ = nullptr;
    QFormLayout *formLayout_ = nullptr;
    QLabel *emptyLabel_ = nullptr;

    // Track labels for dirty state styling
    QHash<QString, QLabel*> itemLabels_;
    // Track editor widgets for value updates
    QHash<QString, QWidget*> itemEditors_;
};

#endif // CONFIGITEMSPANEL_H
