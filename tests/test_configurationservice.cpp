#include "mocks/mockftpclient.h"
#include "mocks/mockrestclient.h"
#include "services/configurationservice.h"
#include "services/deviceconnection.h"

#include <QSignalSpy>
#include <QtTest>

class TestConfigurationService : public QObject
{
    Q_OBJECT

private:
    DeviceConnection *makeConnection()
    {
        auto *restClient = new MockRestClient(this);
        auto *ftpClient = new MockFtpClient(this);
        return new DeviceConnection(restClient, ftpClient, this);
    }

private slots:

    // ==========================================================================
    // canPerformOperations()
    // ==========================================================================

    void testCanPerformOperations_NullConnection_ReturnsFalse()
    {
        ConfigurationService service(nullptr, this);
        QVERIFY(!service.canPerformOperations());
    }

    void testCanPerformOperations_DisconnectedDevice_ReturnsFalse()
    {
        auto *connection = makeConnection();
        ConfigurationService service(connection, this);
        // DeviceConnection starts Disconnected, so canPerformOperations() returns false
        QVERIFY(!service.canPerformOperations());
    }

    // ==========================================================================
    // getConfigCategories() — emits operationNotAvailable when disconnected
    // ==========================================================================

    void testGetConfigCategories_WhenDisconnected_EmitsOperationNotAvailable()
    {
        auto *connection = makeConnection();
        ConfigurationService service(connection, this);
        QSignalSpy spy(&service, &ConfigurationService::operationNotAvailable);

        service.getConfigCategories();

        QCOMPARE(spy.count(), 1);
    }

    void testGetConfigCategories_NullConnection_EmitsOperationNotAvailable()
    {
        ConfigurationService service(nullptr, this);
        QSignalSpy spy(&service, &ConfigurationService::operationNotAvailable);

        service.getConfigCategories();

        QCOMPARE(spy.count(), 1);
    }

    // ==========================================================================
    // getConfigCategoryItems() — emits operationNotAvailable when disconnected
    // ==========================================================================

    void testGetConfigCategoryItems_WhenDisconnected_EmitsOperationNotAvailable()
    {
        auto *connection = makeConnection();
        ConfigurationService service(connection, this);
        QSignalSpy spy(&service, &ConfigurationService::operationNotAvailable);

        service.getConfigCategoryItems("Network");

        QCOMPARE(spy.count(), 1);
    }

    // ==========================================================================
    // setConfigItem() — emits operationNotAvailable when disconnected
    // ==========================================================================

    void testSetConfigItem_WhenDisconnected_EmitsOperationNotAvailable()
    {
        auto *connection = makeConnection();
        ConfigurationService service(connection, this);
        QSignalSpy spy(&service, &ConfigurationService::operationNotAvailable);

        service.setConfigItem("Network", "ip", "192.168.1.1");

        QCOMPARE(spy.count(), 1);
    }

    // ==========================================================================
    // saveConfigToFlash() — emits operationNotAvailable when disconnected
    // ==========================================================================

    void testSaveConfigToFlash_WhenDisconnected_EmitsOperationNotAvailable()
    {
        auto *connection = makeConnection();
        ConfigurationService service(connection, this);
        QSignalSpy spy(&service, &ConfigurationService::operationNotAvailable);

        service.saveConfigToFlash();

        QCOMPARE(spy.count(), 1);
    }

    // ==========================================================================
    // loadConfigFromFlash() — emits operationNotAvailable when disconnected
    // ==========================================================================

    void testLoadConfigFromFlash_WhenDisconnected_EmitsOperationNotAvailable()
    {
        auto *connection = makeConnection();
        ConfigurationService service(connection, this);
        QSignalSpy spy(&service, &ConfigurationService::operationNotAvailable);

        service.loadConfigFromFlash();

        QCOMPARE(spy.count(), 1);
    }

    // ==========================================================================
    // resetConfigToDefaults() — emits operationNotAvailable when disconnected
    // ==========================================================================

    void testResetConfigToDefaults_WhenDisconnected_EmitsOperationNotAvailable()
    {
        auto *connection = makeConnection();
        ConfigurationService service(connection, this);
        QSignalSpy spy(&service, &ConfigurationService::operationNotAvailable);

        service.resetConfigToDefaults();

        QCOMPARE(spy.count(), 1);
    }

    // ==========================================================================
    // Signal forwarding — service forwards REST client signals
    // ==========================================================================

    void testConfigCategoriesReceived_ForwardedFromRestClient()
    {
        auto *restClient = new MockRestClient(this);
        auto *ftpClient = new MockFtpClient(this);
        auto *connection = new DeviceConnection(restClient, ftpClient, this);
        ConfigurationService service(connection, this);
        QSignalSpy spy(&service, &ConfigurationService::configCategoriesReceived);

        emit restClient->configCategoriesReceived({"Network", "Video"});

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toStringList(), QStringList({"Network", "Video"}));
    }

    void testConfigSavedToFlash_ForwardedFromRestClient()
    {
        auto *restClient = new MockRestClient(this);
        auto *ftpClient = new MockFtpClient(this);
        auto *connection = new DeviceConnection(restClient, ftpClient, this);
        ConfigurationService service(connection, this);
        QSignalSpy spy(&service, &ConfigurationService::configSavedToFlash);

        emit restClient->configSavedToFlash();

        QCOMPARE(spy.count(), 1);
    }

    void testConfigLoadedFromFlash_ForwardedFromRestClient()
    {
        auto *restClient = new MockRestClient(this);
        auto *ftpClient = new MockFtpClient(this);
        auto *connection = new DeviceConnection(restClient, ftpClient, this);
        ConfigurationService service(connection, this);
        QSignalSpy spy(&service, &ConfigurationService::configLoadedFromFlash);

        emit restClient->configLoadedFromFlash();

        QCOMPARE(spy.count(), 1);
    }

    void testConfigResetToDefaults_ForwardedFromRestClient()
    {
        auto *restClient = new MockRestClient(this);
        auto *ftpClient = new MockFtpClient(this);
        auto *connection = new DeviceConnection(restClient, ftpClient, this);
        ConfigurationService service(connection, this);
        QSignalSpy spy(&service, &ConfigurationService::configResetToDefaults);

        emit restClient->configResetToDefaults();

        QCOMPARE(spy.count(), 1);
    }

    void testConfigItemSet_ForwardedFromRestClient()
    {
        auto *restClient = new MockRestClient(this);
        auto *ftpClient = new MockFtpClient(this);
        auto *connection = new DeviceConnection(restClient, ftpClient, this);
        ConfigurationService service(connection, this);
        QSignalSpy spy(&service, &ConfigurationService::configItemSet);

        emit restClient->configItemSet("Network", "ip");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), "Network");
        QCOMPARE(spy.at(0).at(1).toString(), "ip");
    }
};

QTEST_MAIN(TestConfigurationService)
#include "test_configurationservice.moc"
