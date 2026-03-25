/**
 * @file mockaudioplaybackservice.h
 * @brief Mock audio playback service for testing StreamingManager.
 */

#ifndef MOCKAUDIOPLAYBACKSERVICE_H
#define MOCKAUDIOPLAYBACKSERVICE_H

#include "services/iaudioplaybackservice.h"

/**
 * @brief Mock implementation of IAudioPlaybackService for testing.
 *
 * Records start/stop calls. startResult_ controls whether start() succeeds.
 */
class MockAudioPlaybackService : public IAudioPlaybackService
{
    Q_OBJECT

public:
    explicit MockAudioPlaybackService(QObject *parent = nullptr) : IAudioPlaybackService(parent) {}
    ~MockAudioPlaybackService() override = default;

    bool start() override
    {
        startCalled_++;
        isPlaying_ = startResult_;
        return startResult_;
    }

    void stop() override
    {
        stopCalled_++;
        isPlaying_ = false;
    }

    [[nodiscard]] bool isPlaying() const override { return isPlaying_; }

    void writeSamples(const QByteArray & /*samples*/, int /*sampleCount*/) override {}

    /// @name Mock control methods
    /// @{
    void mockSetStartResult(bool result) { startResult_ = result; }

    [[nodiscard]] int mockStartCallCount() const { return startCalled_; }
    [[nodiscard]] int mockStopCallCount() const { return stopCalled_; }
    /// @}

private:
    bool startResult_ = true;
    bool isPlaying_ = false;
    int startCalled_ = 0;
    int stopCalled_ = 0;
};

#endif  // MOCKAUDIOPLAYBACKSERVICE_H
