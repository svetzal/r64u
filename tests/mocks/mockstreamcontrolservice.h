/**
 * @file mockstreamcontrolservice.h
 * @brief Mock stream control client for testing StreamingService.
 */

#ifndef MOCKSTREAMCONTROLSERVICE_H
#define MOCKSTREAMCONTROLSERVICE_H

#include "services/istreamcontrolservice.h"

#include <QString>

/**
 * @brief Mock implementation of IStreamControlService for testing.
 *
 * Records all method calls for inspection. Signals can be emitted
 * manually via mock control methods to simulate device responses.
 */
class MockStreamControlService : public IStreamControlService
{
    Q_OBJECT

public:
    explicit MockStreamControlService(QObject *parent = nullptr) : IStreamControlService(parent) {}
    ~MockStreamControlService() override = default;

    void setHost(const QString &host) override { host_ = host; }
    [[nodiscard]] QString host() const override { return host_; }

    void startAllStreams(const QString &targetHost, quint16 videoPort, quint16 audioPort) override
    {
        startAllStreamsCalled_++;
        lastTargetHost_ = targetHost;
        lastVideoPort_ = videoPort;
        lastAudioPort_ = audioPort;
    }

    void stopAllStreams() override
    {
        stopAllStreamsCalled_++;
        if (holdCommandsOnStop_) {
            idle_ = false;
        }
    }

    void clearPendingCommands() override { clearPendingCommandsCalled_++; }

    [[nodiscard]] bool isIdle() const override { return idle_; }

    /// @name Mock control methods
    /// @{

    void mockEmitCommandSucceeded(const QString &command) { emit commandSucceeded(command); }
    void mockEmitCommandFailed(const QString &command, const QString &error)
    {
        emit commandFailed(command, error);
    }

    /// Makes stop commands stay undelivered (not idle) until mockSettleCommands().
    void mockHoldCommandsOnStop() { holdCommandsOnStop_ = true; }

    /// Settles every held command and reports idle.
    void mockSettleCommands()
    {
        idle_ = true;
        emit idle();
    }

    [[nodiscard]] int mockStartAllStreamsCallCount() const { return startAllStreamsCalled_; }
    [[nodiscard]] int mockStopAllStreamsCallCount() const { return stopAllStreamsCalled_; }
    [[nodiscard]] int mockClearPendingCommandsCallCount() const
    {
        return clearPendingCommandsCalled_;
    }
    [[nodiscard]] QString mockLastTargetHost() const { return lastTargetHost_; }
    [[nodiscard]] quint16 mockLastVideoPort() const { return lastVideoPort_; }
    [[nodiscard]] quint16 mockLastAudioPort() const { return lastAudioPort_; }
    /// @}

private:
    QString host_;
    QString lastTargetHost_;
    quint16 lastVideoPort_ = 0;
    quint16 lastAudioPort_ = 0;
    int startAllStreamsCalled_ = 0;
    int stopAllStreamsCalled_ = 0;
    int clearPendingCommandsCalled_ = 0;
    bool holdCommandsOnStop_ = false;
    bool idle_ = true;
};

#endif  // MOCKSTREAMCONTROLSERVICE_H
