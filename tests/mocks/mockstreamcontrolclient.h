/**
 * @file mockstreamcontrolclient.h
 * @brief Mock stream control client for testing StreamingManager.
 */

#ifndef MOCKSTREAMCONTROLCLIENT_H
#define MOCKSTREAMCONTROLCLIENT_H

#include "services/istreamcontrolclient.h"

#include <QString>

/**
 * @brief Mock implementation of IStreamControlClient for testing.
 *
 * Records all method calls for inspection. Signals can be emitted
 * manually via mock control methods to simulate device responses.
 */
class MockStreamControlClient : public IStreamControlClient
{
    Q_OBJECT

public:
    explicit MockStreamControlClient(QObject *parent = nullptr) : IStreamControlClient(parent) {}
    ~MockStreamControlClient() override = default;

    void setHost(const QString &host) override { host_ = host; }
    [[nodiscard]] QString host() const override { return host_; }

    void startAllStreams(const QString &targetHost, quint16 videoPort, quint16 audioPort) override
    {
        startAllStreamsCalled_++;
        lastTargetHost_ = targetHost;
        lastVideoPort_ = videoPort;
        lastAudioPort_ = audioPort;
    }

    void stopAllStreams() override { stopAllStreamsCalled_++; }

    void clearPendingCommands() override { clearPendingCommandsCalled_++; }

    /// @name Mock control methods
    /// @{

    void mockEmitCommandSucceeded(const QString &command) { emit commandSucceeded(command); }
    void mockEmitCommandFailed(const QString &command, const QString &error)
    {
        emit commandFailed(command, error);
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
};

#endif  // MOCKSTREAMCONTROLCLIENT_H
