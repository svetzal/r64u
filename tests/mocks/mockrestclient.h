/**
 * @file mockrestclient.h
 * @brief Mock REST client implementing IRestClient for testing.
 */

#ifndef MOCKRESTCLIENT_H
#define MOCKRESTCLIENT_H

#include "services/irestclient.h"

#include <QList>

/**
 * @brief Mock REST client for testing services that depend on IRestClient.
 *
 * Provides controlled signal emission for testing state machine behavior.
 * All virtual methods are no-ops unless specified. Use mock control methods
 * to simulate device responses.
 */
class MockRestClient : public IRestClient
{
    Q_OBJECT

public:
    explicit MockRestClient(QObject *parent = nullptr);
    ~MockRestClient() override = default;

    // IRestClient interface - configuration
    void setHost(const QString &host) override { host_ = host; }
    [[nodiscard]] QString host() const override { return host_; }
    void setPassword(const QString &password) override { password_ = password; }
    [[nodiscard]] bool hasPassword() const override { return !password_.isEmpty(); }

    // IRestClient interface - device info
    void getVersion() override {}
    void getInfo() override;
    void getDrives() override;

    // IRestClient interface - content playback (no-op stubs)
    void playSid(const QString &, int = -1) override {}
    void playMod(const QString &) override {}
    void loadPrg(const QString &) override {}
    void runPrg(const QString &) override {}
    void runCrt(const QString &) override {}

    // IRestClient interface - drive control (no-op stubs)
    void mountImage(const QString &, const QString &, const QString & = "readwrite") override {}
    void unmountImage(const QString &) override {}
    void resetDrive(const QString &) override {}

    // IRestClient interface - machine control (no-op stubs)
    void resetMachine() override {}
    void rebootMachine() override {}
    void pauseMachine() override {}
    void resumeMachine() override {}
    void powerOffMachine() override {}
    void pressMenuButton() override {}
    void writeMem(const QString &address, const QByteArray &data) override;
    void typeText(const QString &text) override;

    // IRestClient interface - file operations (no-op stubs)
    void getFileInfo(const QString &) override {}
    void createD64(const QString &, const QString & = QString(), int = 35) override {}
    void createD81(const QString &, const QString & = QString()) override {}

    // IRestClient interface - configuration
    void getConfigCategories() override {}
    void getConfigCategoryItems(const QString &) override {}
    void getConfigItem(const QString &, const QString &) override {}
    void setConfigItem(const QString &, const QString &, const QVariant &) override {}
    void updateConfigsBatch(const QJsonObject &configs) override;
    void saveConfigToFlash() override {}
    void loadConfigFromFlash() override {}
    void resetConfigToDefaults() override {}

    /// @name Mock Control Methods
    /// @{
    void mockEmitInfoReceived(const DeviceInfo &info);
    void mockEmitDrivesReceived(const QList<DriveInfo> &drives);
    void mockEmitConnectionError(const QString &error);
    void mockEmitOperationFailed(const QString &operation, const QString &error);
    void mockEmitConfigsUpdated();
    void mockReset();

    [[nodiscard]] int mockGetInfoCallCount() const { return getInfoCalls_; }
    [[nodiscard]] int mockGetDrivesCallCount() const { return getDrivesCalls_; }
    [[nodiscard]] int mockGetWriteMemCallCount() const { return writeMemCalls_; }
    [[nodiscard]] int mockGetTypeTextCallCount() const { return typeTextCalls_; }
    [[nodiscard]] QString mockLastWriteMemAddress() const { return lastWriteMemAddress_; }
    [[nodiscard]] QByteArray mockLastWriteMemData() const { return lastWriteMemData_; }
    [[nodiscard]] QString mockLastTypeTextArg() const { return lastTypeText_; }
    [[nodiscard]] QJsonObject mockLastUpdateConfigsBatchArg() const { return lastConfigsBatch_; }
    /// @}

private:
    QString host_;
    QString password_;
    int getInfoCalls_ = 0;
    int getDrivesCalls_ = 0;
    int writeMemCalls_ = 0;
    int typeTextCalls_ = 0;
    QString lastWriteMemAddress_;
    QByteArray lastWriteMemData_;
    QString lastTypeText_;
    QJsonObject lastConfigsBatch_;
};

#endif  // MOCKRESTCLIENT_H
