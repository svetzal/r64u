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

    // IRestClient interface - drive control
    void mountImage(const QString &, const QString &, const QString & = "readwrite") override {}
    void unmountImage(const QString &drive) override
    {
        ++unmountImageCalls_;
        lastUnmountDrive_ = drive;
    }
    void resetDrive(const QString &) override {}

    // IRestClient interface - machine control
    void resetMachine() override { ++resetMachineCalls_; }
    void rebootMachine() override { ++rebootMachineCalls_; }
    void pauseMachine() override { ++pauseMachineCalls_; }
    void resumeMachine() override { ++resumeMachineCalls_; }
    void powerOffMachine() override { ++powerOffMachineCalls_; }
    void pressMenuButton() override { ++pressMenuButtonCalls_; }
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
    [[nodiscard]] int mockResetMachineCallCount() const { return resetMachineCalls_; }
    [[nodiscard]] int mockRebootMachineCallCount() const { return rebootMachineCalls_; }
    [[nodiscard]] int mockPauseMachineCallCount() const { return pauseMachineCalls_; }
    [[nodiscard]] int mockResumeMachineCallCount() const { return resumeMachineCalls_; }
    [[nodiscard]] int mockPressMenuButtonCallCount() const { return pressMenuButtonCalls_; }
    [[nodiscard]] int mockPowerOffMachineCallCount() const { return powerOffMachineCalls_; }
    [[nodiscard]] int mockUnmountImageCallCount() const { return unmountImageCalls_; }
    [[nodiscard]] QString mockLastUnmountDrive() const { return lastUnmountDrive_; }
    /// @}

private:
    QString host_;
    QString password_;
    int getInfoCalls_ = 0;
    int getDrivesCalls_ = 0;
    int writeMemCalls_ = 0;
    int typeTextCalls_ = 0;
    int resetMachineCalls_ = 0;
    int rebootMachineCalls_ = 0;
    int pauseMachineCalls_ = 0;
    int resumeMachineCalls_ = 0;
    int powerOffMachineCalls_ = 0;
    int pressMenuButtonCalls_ = 0;
    int unmountImageCalls_ = 0;
    QString lastWriteMemAddress_;
    QByteArray lastWriteMemData_;
    QString lastTypeText_;
    QJsonObject lastConfigsBatch_;
    QString lastUnmountDrive_;
};

#endif  // MOCKRESTCLIENT_H
