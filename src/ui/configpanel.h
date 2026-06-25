#ifndef CONFIGPANEL_H
#define CONFIGPANEL_H

#include "services/devicetypes.h"
#include "ui/imessagepresenter.h"
#include "ui/ipanel.h"
#include "ui/qmessageboxpresenter.h"

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
    explicit ConfigPanel(ConfigurationService *configService, ErrorHandler *errorHandler,
                         QWidget *parent = nullptr);

    QObject *asQObject() override { return this; }

    // Public API for MainWindow coordination
    void refreshIfEmpty() override;

    /**
     * @brief Replaces the message presenter used for confirmation dialogs.
     *
     * The default presenter shows real QMessageBox dialogs. Inject a test
     * double to verify confirmation behaviour without blocking the UI.
     *
     * @param presenter Non-owning pointer; must outlive this widget. Pass nullptr to
     *                  restore the default QMessageBoxPresenter.
     */
    void setMessagePresenter(IMessagePresenter *presenter);

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
    void onItemEdited(const QString &category, const QString &item, const QVariant &value);
    void onItemSetResult(const QString &category, const QString &item);

private:
    void setupUi();
    void setupConnections();
    void updateActions();

    // Dependencies (not owned)
    ConfigurationService *configService_ = nullptr;
    ErrorHandler *errorHandler_;

    // Message presenter — owned default, swappable for tests (non-owning pointer).
    QMessageBoxPresenter defaultPresenter_;
    IMessagePresenter *presenter_ = &defaultPresenter_;

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
