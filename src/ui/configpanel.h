#ifndef CONFIGPANEL_H
#define CONFIGPANEL_H

#include <QWidget>
#include <QToolBar>
#include <QSplitter>
#include <QListWidget>
#include <QLabel>

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
    void onConnectionStateChanged(bool connected);
    void refreshIfEmpty();

signals:
    void statusMessage(const QString &message, int timeout = 0);

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
    void onItemEdited(const QString &category, const QString &item,
                      const QVariant &value);
    void onItemSetResult(const QString &category, const QString &item);

private:
    void setupUi();
    void setupConnections();
    void updateActions();

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
    QSplitter *splitter_ = nullptr;
    QListWidget *categoryList_ = nullptr;
    ConfigItemsPanel *itemsPanel_ = nullptr;
};

#endif // CONFIGPANEL_H
