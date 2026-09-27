#include "mocks/mockftpclient.h"
#include "mocks/mockmessagepresenter.h"
#include "mocks/mockrestclient.h"
#include "services/configurationservice.h"
#include "services/deviceconnectionmanager.h"
#include "services/devicetypes.h"
#include "services/errorhandler.h"
#include "ui/configitemspanel.h"
#include "ui/configpanel.h"

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSignalSpy>
#include <QSplitter>
#include <QtTest>

/**
 * @brief REST client mock that tracks configuration operation calls.
 */
class TrackingRestClient : public MockRestClient
{
    Q_OBJECT

public:
    explicit TrackingRestClient(QObject *parent = nullptr) : MockRestClient(parent) {}

    int getConfigCategoriesCalls = 0;
    int getConfigCategoryItemsCalls = 0;
    QStringList getConfigCategoryItemsArgs;
    int saveConfigToFlashCalls = 0;
    int loadConfigFromFlashCalls = 0;
    int resetConfigToDefaultsCalls = 0;
    int setConfigItemCalls = 0;
    QString lastSetCategory;
    QString lastSetItem;
    QVariant lastSetValue;

    void getConfigCategories() override { ++getConfigCategoriesCalls; }

    void getConfigCategoryItems(const QString &category) override
    {
        ++getConfigCategoryItemsCalls;
        getConfigCategoryItemsArgs.append(category);
    }

    void saveConfigToFlash() override { ++saveConfigToFlashCalls; }
    void loadConfigFromFlash() override { ++loadConfigFromFlashCalls; }
    void resetConfigToDefaults() override { ++resetConfigToDefaultsCalls; }

    void setConfigItem(const QString &category, const QString &item, const QVariant &value) override
    {
        ++setConfigItemCalls;
        lastSetCategory = category;
        lastSetItem = item;
        lastSetValue = value;
    }
};

class TestConfigPanel : public QObject
{
    Q_OBJECT

private:
    ErrorHandler *makeErrorHandler() { return new ErrorHandler(nullptr, this); }

    /**
     * @brief Creates a TrackingRestClient-backed ConfigurationService in Connected state.
     *
     * Drives the DeviceConnectionManager through its full connect sequence so that
     * canPerformOperations() returns true.  Returns the REST and FTP client
     * pointers via out-parameters so tests can inject response signals.
     */
    ConfigurationService *makeConnectedService(TrackingRestClient **restClientOut,
                                               MockFtpClient **ftpClientOut = nullptr)
    {
        auto *restClient = new TrackingRestClient(this);
        auto *ftpClient = new MockFtpClient(this);
        auto *connection = new DeviceConnectionManager(restClient, ftpClient, this);

        // Drive the connection state machine to Connected:
        // connectToDevice() sets connectingInProgress_ = true and calls getInfo() + connectToHost()
        connection->setHost("test-device");
        connection->connectToDevice();

        // Simulate REST responding with device info (sets restConnected_ = true)
        DeviceInfo info;
        info.hostname = "test-device";
        info.firmwareVersion = "1.0.0";
        emit restClient->infoReceived(info);

        // Simulate FTP connecting (sets ftpConnected_ = true → transitions to Connected)
        ftpClient->mockSetConnected(true);

        auto *service = new ConfigurationService(connection, this);

        if (restClientOut != nullptr) {
            *restClientOut = restClient;
        }
        if (ftpClientOut != nullptr) {
            *ftpClientOut = ftpClient;
        }
        return service;
    }

    /**
     * @brief Emits a category's items through the REST client with placeholder values.
     */
    static void emitItems(TrackingRestClient *restClient, const QString &category,
                          const QStringList &names)
    {
        QHash<QString, ConfigItemMetadata> items;
        for (const QString &name : names) {
            ConfigItemMetadata meta;
            meta.current = QStringLiteral("value");
            meta.defaultValue = QStringLiteral("value");
            meta.hasRange = false;
            items[name] = meta;
        }
        emit restClient->configCategoryItemsReceived(category, items);
    }

    /**
     * @brief Emits one item whose dropdown offers the given options through the REST client.
     */
    static void emitItemWithOptions(TrackingRestClient *restClient, const QString &category,
                                    const QString &name, const QString &current,
                                    const QStringList &options)
    {
        ConfigItemMetadata meta;
        meta.current = current;
        meta.defaultValue = current;
        meta.values = options;
        meta.hasRange = false;
        emit restClient->configCategoryItemsReceived(category, {{name, meta}});
    }

    /**
     * @brief Loads Alpha..Golf, each with three items that all match "setting".
     */
    static void emitScrollingCategories(TrackingRestClient *restClient)
    {
        const QStringList categories = {"Alpha", "Bravo",   "Charlie", "Delta",
                                        "Echo",  "Foxtrot", "Golf"};
        emit restClient->configCategoriesReceived(categories);
        for (const QString &category : categories) {
            emitItems(restClient, category, {"Setting 1", "Setting 2", "Setting 3"});
        }
    }

    /**
     * @brief Clicks a category row the way a user would, scrolling the list to it first.
     */
    static void clickCategoryRow(const ConfigPanel &panel, const QString &category)
    {
        auto *list = panel.findChild<QListWidget *>();
        QVERIFY(list != nullptr);
        const auto found = list->findItems(category, Qt::MatchExactly);
        QVERIFY(found.size() == 1);
        list->scrollToItem(found.first());
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                          list->visualItemRect(found.first()).center());
    }

    /**
     * @brief Whether a category's group, header through last editor, lies in the items viewport.
     */
    static bool groupFullyVisible(const ConfigPanel &panel, const QString &category,
                                  const QString &lastItem)
    {
        auto *items = panel.findChild<ConfigItemsPanel *>();
        auto *area = items != nullptr ? items->findChild<QScrollArea *>() : nullptr;
        if (area == nullptr) {
            return false;
        }
        QLabel *header = nullptr;
        for (QLabel *label : items->findChildren<QLabel *>(QStringLiteral("categoryHeader"))) {
            if (label->text() == category) {
                header = label;
            }
        }
        auto *lastEditor =
            items->findChild<QWidget *>(category + QLatin1Char('/') + lastItem + ":editor");
        if (header == nullptr || lastEditor == nullptr) {
            return false;
        }
        const QRect viewport = area->viewport()->rect();
        const QRect headerRect(header->mapTo(area->viewport(), QPoint(0, 0)), header->size());
        const QRect editorRect(lastEditor->mapTo(area->viewport(), QPoint(0, 0)),
                               lastEditor->size());
        return viewport.contains(headerRect) && viewport.contains(editorRect);
    }

    static QScrollBar *itemsScrollBar(const ConfigPanel &panel)
    {
        auto *items = panel.findChild<ConfigItemsPanel *>();
        auto *area = items != nullptr ? items->findChild<QScrollArea *>() : nullptr;
        return area != nullptr ? area->verticalScrollBar() : nullptr;
    }

    /**
     * @brief Returns the texts of the category rows that are not hidden, in order.
     */
    static QStringList visibleCategoryRows(const ConfigPanel &panel)
    {
        auto *list = panel.findChild<QListWidget *>();
        QStringList rows;
        if (list == nullptr) {
            return rows;
        }
        for (int i = 0; i < list->count(); ++i) {
            if (!list->item(i)->isHidden()) {
                rows.append(list->item(i)->text());
            }
        }
        return rows;
    }

    static QLineEdit *filterEdit(const ConfigPanel &panel)
    {
        return panel.findChild<QLineEdit *>(QStringLiteral("configFilterEdit"));
    }

private slots:

    // ==========================================================================
    // refreshIfEmpty() — calls getConfigCategories() when connected and empty
    // ==========================================================================

    void testRefreshIfEmpty_WhenConnectedAndEmpty_CallsGetCategories()
    {
        TrackingRestClient *restClient = nullptr;
        auto *service = makeConnectedService(&restClient);
        ConfigPanel panel(service, makeErrorHandler());

        panel.refreshIfEmpty();

        QCOMPARE(restClient->getConfigCategoriesCalls, 1);
    }

    void testRefreshIfEmpty_WhenDisconnected_DoesNotCallGetCategories()
    {
        auto *restClient = new TrackingRestClient(this);
        auto *ftpClient = new MockFtpClient(this);
        auto *connection = new DeviceConnectionManager(restClient, ftpClient, this);
        // Do NOT call connectToDevice() — stays Disconnected
        auto *service = new ConfigurationService(connection, this);
        ConfigPanel panel(service, makeErrorHandler());

        panel.refreshIfEmpty();

        QCOMPARE(restClient->getConfigCategoriesCalls, 0);
    }

    void testRefreshIfEmpty_WhenCategoriesAlreadyLoaded_DoesNotRefresh()
    {
        TrackingRestClient *restClient = nullptr;
        auto *service = makeConnectedService(&restClient);
        ConfigPanel panel(service, makeErrorHandler());

        // Pre-populate with categories via signal — causes getConfigCategoryItems calls
        emit restClient->configCategoriesReceived({"Network", "Video"});

        const int callsBefore = restClient->getConfigCategoriesCalls;
        panel.refreshIfEmpty();

        // Should not add another call because categories are already loaded
        QCOMPARE(restClient->getConfigCategoriesCalls, callsBefore);
    }

    // ==========================================================================
    // onCategoriesReceived — loads items for each received category
    // ==========================================================================

    void testCategoriesReceived_LoadsItemsForEachCategory()
    {
        TrackingRestClient *restClient = nullptr;
        auto *service = makeConnectedService(&restClient);
        ConfigPanel panel(service, makeErrorHandler());

        emit restClient->configCategoriesReceived({"Network", "Video", "Audio"});

        // getConfigCategoryItems() is called once per received category (3 calls),
        // plus the panel auto-selects the first category which may trigger an
        // additional load if no items have been received for it yet. Verify that
        // all three categories were requested at least once.
        QVERIFY(restClient->getConfigCategoryItemsCalls >= 3);
        QVERIFY(restClient->getConfigCategoryItemsArgs.contains("Network"));
        QVERIFY(restClient->getConfigCategoryItemsArgs.contains("Video"));
        QVERIFY(restClient->getConfigCategoryItemsArgs.contains("Audio"));
    }

    void testCategoriesReceived_EmptyList_NoCategoryItemCalls()
    {
        TrackingRestClient *restClient = nullptr;
        auto *service = makeConnectedService(&restClient);
        ConfigPanel panel(service, makeErrorHandler());

        emit restClient->configCategoriesReceived({});

        QCOMPARE(restClient->getConfigCategoryItemsCalls, 0);
    }

    // ==========================================================================
    // configSavedToFlash signal — clears dirty state without crash
    // ==========================================================================

    void testSavedToFlash_HandledWithoutCrash()
    {
        TrackingRestClient *restClient = nullptr;
        auto *service = makeConnectedService(&restClient);
        ConfigPanel panel(service, makeErrorHandler());
        panel.show();

        // Should not crash — clears dirty indicator
        emit restClient->configSavedToFlash();

        QVERIFY(panel.isEnabled());
    }

    // ==========================================================================
    // configLoadedFromFlash signal — triggers getConfigCategories refresh
    // ==========================================================================

    void testLoadedFromFlash_TriggersGetConfigCategories()
    {
        TrackingRestClient *restClient = nullptr;
        auto *service = makeConnectedService(&restClient);
        ConfigPanel panel(service, makeErrorHandler());

        emit restClient->configLoadedFromFlash();

        // onLoadedFromFlash → onRefresh() → configService_->getConfigCategories()
        QVERIFY(restClient->getConfigCategoriesCalls >= 1);
    }

    // ==========================================================================
    // configResetToDefaults signal — triggers getConfigCategories refresh
    // ==========================================================================

    void testResetToDefaults_TriggersGetConfigCategories()
    {
        TrackingRestClient *restClient = nullptr;
        auto *service = makeConnectedService(&restClient);
        ConfigPanel panel(service, makeErrorHandler());

        emit restClient->configResetToDefaults();

        // onResetComplete → onRefresh() → configService_->getConfigCategories()
        QVERIFY(restClient->getConfigCategoriesCalls >= 1);
    }

    // ==========================================================================
    // configItemSet signal — handled without crash
    // ==========================================================================

    void testItemSet_SignalHandledWithoutCrash()
    {
        TrackingRestClient *restClient = nullptr;
        auto *service = makeConnectedService(&restClient);
        ConfigPanel panel(service, makeErrorHandler());

        // Should not crash
        emit restClient->configItemSet("Network", "ip");

        QVERIFY(panel.isEnabled());
    }

    // ==========================================================================
    // configCategoryItemsReceived signal — accepted without crash
    // ==========================================================================

    void testCategoryItemsReceived_SignalHandledWithoutCrash()
    {
        TrackingRestClient *restClient = nullptr;
        auto *service = makeConnectedService(&restClient);
        ConfigPanel panel(service, makeErrorHandler());
        emit restClient->configCategoriesReceived({"Network"});

        QHash<QString, ConfigItemMetadata> items;
        ConfigItemMetadata meta;
        meta.current = "192.168.1.1";
        meta.defaultValue = "0.0.0.0";
        meta.hasRange = false;
        items["ip"] = meta;

        // Should not crash
        emit restClient->configCategoryItemsReceived("Network", items);

        QVERIFY(panel.isEnabled());
    }

    // ==========================================================================
    // IPanel contract — statusMessage signal required for PanelCoordinator routing
    // ==========================================================================

    void testHasStatusMessageSignal()
    {
        const QMetaObject *mo = &ConfigPanel::staticMetaObject;
        bool found = false;
        for (int i = mo->methodOffset(); i < mo->methodCount(); ++i) {
            QMetaMethod m = mo->method(i);
            if (m.methodType() == QMetaMethod::Signal && m.name() == "statusMessage") {
                found = true;
                break;
            }
        }
        QVERIFY2(found, "ConfigPanel must declare statusMessage(const QString &, int) signal");
    }

    // ==========================================================================
    // onResetToDefaults — confirm dialog controls whether reset is called
    // ==========================================================================

    void testOnResetToDefaults_WhenConfirmed_CallsResetConfigToDefaults()
    {
        TrackingRestClient *restClient = nullptr;
        auto *service = makeConnectedService(&restClient);
        ConfigPanel panel(service, makeErrorHandler());

        MockMessagePresenter mock;
        mock.nextConfirmResult = 0;  // index 0 = "Reset to Defaults" button
        panel.setMessagePresenter(&mock);

        QMetaObject::invokeMethod(&panel, "onResetToDefaults", Qt::DirectConnection);

        QCOMPARE(restClient->resetConfigToDefaultsCalls, 1);
        QCOMPARE(mock.confirmCalls.size(), 1);
    }

    void testOnResetToDefaults_WhenCancelled_DoesNotCallReset()
    {
        TrackingRestClient *restClient = nullptr;
        auto *service = makeConnectedService(&restClient);
        ConfigPanel panel(service, makeErrorHandler());

        MockMessagePresenter mock;
        mock.nextConfirmResult = 1;  // index 1 = "Cancel" button
        panel.setMessagePresenter(&mock);

        QMetaObject::invokeMethod(&panel, "onResetToDefaults", Qt::DirectConnection);

        QCOMPARE(restClient->resetConfigToDefaultsCalls, 0);
        QCOMPARE(mock.confirmCalls.size(), 1);
    }

    void testOnResetToDefaults_WhenDismissed_DoesNotCallReset()
    {
        TrackingRestClient *restClient = nullptr;
        auto *service = makeConnectedService(&restClient);
        ConfigPanel panel(service, makeErrorHandler());

        MockMessagePresenter mock;
        mock.nextConfirmResult = -1;  // -1 = dialog dismissed without button click
        panel.setMessagePresenter(&mock);

        QMetaObject::invokeMethod(&panel, "onResetToDefaults", Qt::DirectConnection);

        QCOMPARE(restClient->resetConfigToDefaultsCalls, 0);
    }

    void testOnResetToDefaults_DefaultButtonIsCancel()
    {
        TrackingRestClient *restClient = nullptr;
        auto *service = makeConnectedService(&restClient);
        ConfigPanel panel(service, makeErrorHandler());

        MockMessagePresenter mock;
        mock.nextConfirmResult = -1;
        panel.setMessagePresenter(&mock);

        QMetaObject::invokeMethod(&panel, "onResetToDefaults", Qt::DirectConnection);

        QCOMPARE(mock.confirmCalls.size(), 1);
        const ConfirmCall &call = mock.confirmCalls[0];
        QVERIFY(call.defaultIndex >= 0 && call.defaultIndex < call.buttons.size());
        QCOMPARE(call.buttons[call.defaultIndex].role, IMessagePresenter::ButtonRole::Reject);
    }

    // ==========================================================================
    // Layout persistence
    // ==========================================================================

    void testLoadSettings_RestoresSplitterState()
    {
        QCoreApplication::setOrganizationName("r64utest");
        QCoreApplication::setApplicationName("test_configpanel");
        QSettings().remove("layout");

        TrackingRestClient *restClient = nullptr;
        QList<int> savedSizes;
        {
            ConfigPanel saved(makeConnectedService(&restClient), makeErrorHandler());
            saved.resize(1000, 600);
            saved.show();
            QVERIFY(QTest::qWaitForWindowExposed(&saved));
            auto *splitter = saved.findChild<QSplitter *>();
            QVERIFY(splitter != nullptr);
            splitter->setSizes({150, 850});
            QCoreApplication::processEvents();
            savedSizes = splitter->sizes();
            saved.saveSettings();
        }
        QVERIFY(!QSettings().value("layout/configSplitter").toByteArray().isEmpty());

        ConfigPanel restored(makeConnectedService(&restClient), makeErrorHandler());
        restored.resize(1000, 600);
        restored.show();
        QVERIFY(QTest::qWaitForWindowExposed(&restored));
        auto *freshSplitter = restored.findChild<QSplitter *>();
        QVERIFY(freshSplitter != nullptr);
        QVERIFY2(freshSplitter->sizes() != savedSizes,
                 "default layout must differ from the saved one");

        restored.loadSettings();
        QCoreApplication::processEvents();

        QCOMPARE(freshSplitter->sizes(), savedSizes);
        QSettings().remove("layout");
    }

    void testLoadSettings_EmptySavedState_KeepsDefaults()
    {
        QCoreApplication::setOrganizationName("r64utest");
        QCoreApplication::setApplicationName("test_configpanel");
        QSettings().remove("layout");

        TrackingRestClient *restClient = nullptr;
        ConfigPanel panel(makeConnectedService(&restClient), makeErrorHandler());
        auto *splitter = panel.findChild<QSplitter *>();
        QVERIFY(splitter != nullptr);
        const QList<int> defaults = splitter->sizes();

        panel.loadSettings();

        QCOMPARE(splitter->sizes(), defaults);
    }

    void testCategoryList_CanGrowTo320()
    {
        TrackingRestClient *restClient = nullptr;
        ConfigPanel panel(makeConnectedService(&restClient), makeErrorHandler());
        auto *list = panel.findChild<QListWidget *>();
        QVERIFY(list != nullptr);
        QCOMPARE(list->maximumWidth(), 320);
    }

    void testOnResetToDefaults_WhenDisconnected_DoesNotShowDialog()
    {
        auto *restClient = new TrackingRestClient(this);
        auto *ftpClient = new MockFtpClient(this);
        auto *connection = new DeviceConnectionManager(restClient, ftpClient, this);
        // Do NOT call connectToDevice() — stays Disconnected
        auto *service = new ConfigurationService(connection, this);
        ConfigPanel panel(service, makeErrorHandler());

        MockMessagePresenter mock;
        panel.setMessagePresenter(&mock);

        QMetaObject::invokeMethod(&panel, "onResetToDefaults", Qt::DirectConnection);

        QCOMPARE(mock.confirmCalls.size(), 0);
        QCOMPARE(restClient->resetConfigToDefaultsCalls, 0);
    }

    // ==========================================================================
    // Filter box — narrows the category list without touching the device
    // ==========================================================================

    void testFilter_MatchingText_NarrowsCategoryList()
    {
        TrackingRestClient *restClient = nullptr;
        auto *service = makeConnectedService(&restClient);
        ConfigPanel panel(service, makeErrorHandler());
        emit restClient->configCategoriesReceived({"Network", "Video", "Audio"});
        emitItems(restClient, "Network", {"Hostname", "Port"});
        emitItems(restClient, "Video", {"Mode"});
        emitItems(restClient, "Audio", {"Volume", "Mute"});

        auto *edit = filterEdit(panel);
        auto *items = panel.findChild<ConfigItemsPanel *>();
        QVERIFY(edit != nullptr);
        QVERIFY(items != nullptr);
        edit->setText("vol");

        QCOMPARE(visibleCategoryRows(panel), QStringList({"Audio"}));
        QVERIFY(items->isFilterActive());
        QCOMPARE(items->visibleItems(), QStringList({"Audio/Volume"}));
    }

    void testFilter_ClearedText_RestoresFullCategoryList()
    {
        TrackingRestClient *restClient = nullptr;
        auto *service = makeConnectedService(&restClient);
        ConfigPanel panel(service, makeErrorHandler());
        emit restClient->configCategoriesReceived({"Network", "Video", "Audio"});
        emitItems(restClient, "Network", {"Hostname", "Port"});
        emitItems(restClient, "Video", {"Mode"});
        emitItems(restClient, "Audio", {"Volume", "Mute"});
        auto *list = panel.findChild<QListWidget *>();
        QVERIFY(list != nullptr);
        const int rowBefore = list->currentRow();

        auto *edit = filterEdit(panel);
        auto *items = panel.findChild<ConfigItemsPanel *>();
        QVERIFY(edit != nullptr);
        QVERIFY(items != nullptr);
        edit->setText("vol");
        QCOMPARE(visibleCategoryRows(panel), QStringList({"Audio"}));
        QCOMPARE(items->visibleItems(), QStringList({"Audio/Volume"}));
        edit->clear();

        QCOMPARE(visibleCategoryRows(panel), QStringList({"Network", "Video", "Audio"}));
        QCOMPARE(list->currentRow(), rowBefore);
        QVERIFY(!items->isFilterActive());
        // Back to the selected category's full item list (row 0 is Network)
        QCOMPARE(items->visibleItems(), QStringList({"Network/Hostname", "Network/Port"}));
    }

    void testFilter_RebuildKeepsFocusInFilterBox()
    {
        TrackingRestClient *restClient = nullptr;
        auto *service = makeConnectedService(&restClient);
        ConfigPanel panel(service, makeErrorHandler());
        panel.resize(1000, 600);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        emit restClient->configCategoriesReceived({"Network", "Video", "Audio"});
        emitItems(restClient, "Network", {"Hostname", "Port"});
        emitItems(restClient, "Video", {"Mode"});
        emitItems(restClient, "Audio", {"Volume", "Mute"});

        auto *edit = filterEdit(panel);
        QVERIFY(edit != nullptr);
        edit->setFocus();
        QTest::keyClicks(edit, "vol");
        QCOMPARE(visibleCategoryRows(panel), QStringList({"Audio"}));

        // A refresh rebuilds the category list and every items panel
        emit restClient->configCategoriesReceived({"Network", "Video", "Audio"});
        emitItems(restClient, "Network", {"Hostname", "Port"});
        emitItems(restClient, "Video", {"Mode"});
        emitItems(restClient, "Audio", {"Volume", "Mute"});

        QCOMPARE(panel.focusWidget(), edit);
        QCOMPARE(edit->text(), QString("vol"));
        QCOMPARE(visibleCategoryRows(panel), QStringList({"Audio"}));
    }

    void testFilter_TypingIssuesNoRestRequests()
    {
        TrackingRestClient *restClient = nullptr;
        auto *service = makeConnectedService(&restClient);
        ConfigPanel panel(service, makeErrorHandler());
        emit restClient->configCategoriesReceived({"Network", "Audio"});
        emitItems(restClient, "Network", {"Hostname", "Port"});
        emitItems(restClient, "Audio", {"Volume", "Mute"});
        const int categoriesCalls = restClient->getConfigCategoriesCalls;
        const int itemsCalls = restClient->getConfigCategoryItemsCalls;
        const int setCalls = restClient->setConfigItemCalls;

        auto *edit = filterEdit(panel);
        QVERIFY(edit != nullptr);
        edit->setText("v");
        edit->setText("vo");
        edit->setText("vol");
        edit->clear();

        QCOMPARE(restClient->getConfigCategoriesCalls, categoriesCalls);
        QCOMPARE(restClient->getConfigCategoryItemsCalls, itemsCalls);
        QCOMPARE(restClient->setConfigItemCalls, setCalls);
    }

    void testFilter_LateItems_RevealCategory()
    {
        TrackingRestClient *restClient = nullptr;
        auto *service = makeConnectedService(&restClient);
        ConfigPanel panel(service, makeErrorHandler());
        emit restClient->configCategoriesReceived({"Network", "Audio"});
        emitItems(restClient, "Network", {"Hostname", "Port"});

        auto *edit = filterEdit(panel);
        QVERIFY(edit != nullptr);
        edit->setText("vol");
        QVERIFY(visibleCategoryRows(panel).isEmpty());

        emitItems(restClient, "Audio", {"Volume", "Mute"});

        QCOMPARE(visibleCategoryRows(panel), QStringList({"Audio"}));
    }

    // ==========================================================================
    // Filter box — also matches the choices a setting's dropdown offers
    // ==========================================================================

    void testFilter_TermOnlyInAComboOption_ShowsItemAndItsCategory()
    {
        TrackingRestClient *restClient = nullptr;
        auto *service = makeConnectedService(&restClient);
        ConfigPanel panel(service, makeErrorHandler());
        emit restClient->configCategoriesReceived({"Network", "Video", "Audio"});
        emitItems(restClient, "Network", {"Hostname", "Port"});
        emitItemWithOptions(restClient, "Video", "Mode", "PAL", {"PAL", "NTSC"});
        emitItems(restClient, "Audio", {"Volume", "Mute"});

        auto *edit = filterEdit(panel);
        auto *items = panel.findChild<ConfigItemsPanel *>();
        QVERIFY(edit != nullptr);
        QVERIFY(items != nullptr);
        edit->setText("ntsc");

        QCOMPARE(visibleCategoryRows(panel), QStringList({"Video"}));
        QCOMPARE(items->visibleItems(), QStringList({"Video/Mode"}));
    }

    // ==========================================================================
    // Category rows under a filter — clicking scrolls the items to that group
    // ==========================================================================

    void testFilter_ClickingCategoryRow_ScrollsItemsToItsGroup()
    {
        TrackingRestClient *restClient = nullptr;
        auto *service = makeConnectedService(&restClient);
        ConfigPanel panel(service, makeErrorHandler());
        panel.resize(800, 300);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        emitScrollingCategories(restClient);
        auto *edit = filterEdit(panel);
        QVERIFY(edit != nullptr);
        edit->setText("setting");
        auto *vbar = itemsScrollBar(panel);
        QVERIFY(vbar != nullptr);
        QTRY_VERIFY(vbar->maximum() > 0);  // the filtered items have been laid out
        QVERIFY(!groupFullyVisible(panel, "Golf", "Setting 3"));

        clickCategoryRow(panel, "Golf");

        QVERIFY(vbar->value() > 0);
        QVERIFY(groupFullyVisible(panel, "Golf", "Setting 3"));
    }

    void testFilter_ReclickingCurrentCategoryRow_ScrollsBackToItsGroup()
    {
        TrackingRestClient *restClient = nullptr;
        auto *service = makeConnectedService(&restClient);
        ConfigPanel panel(service, makeErrorHandler());
        panel.resize(800, 300);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        emitScrollingCategories(restClient);
        auto *edit = filterEdit(panel);
        QVERIFY(edit != nullptr);
        edit->setText("setting");
        clickCategoryRow(panel, "Golf");
        QVERIFY(groupFullyVisible(panel, "Golf", "Setting 3"));
        const int itemsCalls = restClient->getConfigCategoryItemsCalls;

        // The user scrolls the items back to the top, away from Golf
        auto *vbar = itemsScrollBar(panel);
        QVERIFY(vbar != nullptr);
        vbar->setValue(0);
        QVERIFY(!groupFullyVisible(panel, "Golf", "Setting 3"));

        clickCategoryRow(panel, "Golf");

        QVERIFY(vbar->value() > 0);
        QVERIFY(groupFullyVisible(panel, "Golf", "Setting 3"));
        QCOMPARE(restClient->getConfigCategoryItemsCalls, itemsCalls);
    }

    void testReclickingUnloadedCategoryRow_DoesNotRequestItemsAgain()
    {
        TrackingRestClient *restClient = nullptr;
        auto *service = makeConnectedService(&restClient);
        ConfigPanel panel(service, makeErrorHandler());
        panel.resize(800, 300);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        emit restClient->configCategoriesReceived({"Network", "Empty"});
        emitItems(restClient, "Network", {"Hostname", "Port"});
        const int itemsCalls = restClient->getConfigCategoryItemsCalls;

        clickCategoryRow(panel, "Empty");
        QCOMPARE(restClient->getConfigCategoryItemsCalls, itemsCalls + 1);

        clickCategoryRow(panel, "Empty");
        QCOMPARE(restClient->getConfigCategoryItemsCalls, itemsCalls + 1);
    }
};

QTEST_MAIN(TestConfigPanel)
#include "test_configpanel.moc"
