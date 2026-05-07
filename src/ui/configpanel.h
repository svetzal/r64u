#ifndef CONFIGPANEL_H
#define CONFIGPANEL_H

#include "services/devicetypes.h"
#include "ui/ipanel.h"

#include <QLabel>
#include <QListWidget>
#include <QSplitter>
#include <QToolBar>
#include <QWidget>

class ConfigurationService;
class ConfigurationModel;
class ConfigItemsPanel;
class ErrorHandler;

class ConfigPanel : public QWidget, public IPanel
{
    Q_OBJECT

public:
    explicit ConfigPanel(ConfigurationService *configService, QWidget *parent = nullptr);

    QObject *asQObject() override { return this; }

    // Public API for MainWindow coordination
    void refreshIfEmpty() override;

    void setErrorHandler(ErrorHandler *handler);

private slots:
    void onSaveToFlash();
    void onLoadFromFlash();
    void onResetToDefaults();
    void onRefresh();
    void onCategoriesReceived(const QStringList &categories);
    void onCategoryItemsReceived(const QString &category,
                                 const QHash<QString, ConfigItemMetadata> &items);
    void onSavedToFlash();
    void onLoadedFromFlash();
    void onResetComplete();
    void onDirtyStateChanged(bool isDirty);
    void onCategorySelected(QListWidgetItem *current, QListWidgetItem *previous);
    void onItemEdited(const QString &category, const QString &item, const QVariant &value);
    void onItemSetResult(const QString &category, const QString &item);

private:
    void setupUi();
    void setupConnections();
    void updateActions();

    // Dependencies (not owned)
    ConfigurationService *configService_ = nullptr;
    ErrorHandler *errorHandler_ = nullptr;

    // Owned model
    ConfigurationModel *configModel_ = nullptr;

    // UI widgets
    QToolBar *toolBar_ = nullptr;
    QAction *saveToFlashAction_ = nullptr;
    QAction *loadFromFlashAction_ = nullptr;
    QAction *resetToDefaultsAction_ = nullptr;
    QAction *refreshAction_ = nullptr;
    QLabel *unsavedIndicator_ = nullptr;
    QSplitter *splitter_ = nullptr;
    QListWidget *categoryList_ = nullptr;
    ConfigItemsPanel *itemsPanel_ = nullptr;
};

#endif  // CONFIGPANEL_H
