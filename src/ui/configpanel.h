#ifndef CONFIGPANEL_H
#define CONFIGPANEL_H

#include <QWidget>
#include <QToolBar>
#include <QTabWidget>
#include <QLabel>
#include <QHash>

#include "services/c64urestclient.h"

class DeviceConnection;
class ConfigurationModel;
class ConfigItemsPanel;

class ConfigPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ConfigPanel(DeviceConnection *connection, QWidget *parent = nullptr);

    // Public API for MainWindow coordination
    void refreshIfEmpty();

signals:
    void statusMessage(const QString &message, int timeout = 0);

private slots:
    void onConnectionStateChanged();
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
    void onCategoryTabChanged(int index);
    void onItemEdited(const QString &category, const QString &item,
                      const QVariant &value);
    void onItemSetResult(const QString &category, const QString &item);

private:
    void setupUi();
    void setupConnections();
    void updateActions();
    ConfigItemsPanel* getOrCreateItemsPanel(const QString &category);

    // Dependencies (not owned)
    DeviceConnection *deviceConnection_ = nullptr;

    // Owned model
    ConfigurationModel *configModel_ = nullptr;

    // UI widgets
    QToolBar *toolBar_ = nullptr;
    QAction *saveToFlashAction_ = nullptr;
    QAction *loadFromFlashAction_ = nullptr;
    QAction *resetToDefaultsAction_ = nullptr;
    QAction *refreshAction_ = nullptr;
    QLabel *unsavedIndicator_ = nullptr;
    QTabWidget *categoryTabs_ = nullptr;
    QHash<QString, ConfigItemsPanel*> itemsPanels_;
};

#endif // CONFIGPANEL_H
