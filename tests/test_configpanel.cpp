#include "mocks/mockftpclient.h"
#include "mocks/mockrestclient.h"
#include "services/configurationservice.h"
#include "services/deviceconnectionmanager.h"
#include "services/devicetypes.h"
#include "ui/configpanel.h"

#include <QSignalSpy>
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

private slots:

    // ==========================================================================
    // refreshIfEmpty() — calls getConfigCategories() when connected and empty
    // ==========================================================================

    void testRefreshIfEmpty_WhenConnectedAndEmpty_CallsGetCategories()
    {
        TrackingRestClient *restClient = nullptr;
        auto *service = makeConnectedService(&restClient);
        ConfigPanel panel(service);

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
        ConfigPanel panel(service);

        panel.refreshIfEmpty();

        QCOMPARE(restClient->getConfigCategoriesCalls, 0);
    }

    void testRefreshIfEmpty_WhenCategoriesAlreadyLoaded_DoesNotRefresh()
    {
        TrackingRestClient *restClient = nullptr;
        auto *service = makeConnectedService(&restClient);
        ConfigPanel panel(service);

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
        ConfigPanel panel(service);

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
        ConfigPanel panel(service);

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
        ConfigPanel panel(service);
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
        ConfigPanel panel(service);

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
        ConfigPanel panel(service);

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
        ConfigPanel panel(service);

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
        ConfigPanel panel(service);
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
};

QTEST_MAIN(TestConfigPanel)
#include "test_configpanel.moc"
