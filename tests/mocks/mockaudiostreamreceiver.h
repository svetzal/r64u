/**
 * @file mockaudiostreamreceiver.h
 * @brief Mock audio stream receiver for testing StreamingManager.
 */

#ifndef MOCKAUDIOSTREAMRECEIVER_H
#define MOCKAUDIOSTREAMRECEIVER_H

#include "services/iaudiostreamreceiver.h"

/**
 * @brief Mock implementation of IAudioStreamReceiver for testing.
 *
 * Records bind/close/setAudioFormat calls. bindResult_ controls bind() result.
 */
class MockAudioStreamReceiver : public IAudioStreamReceiver
{
    Q_OBJECT

public:
    explicit MockAudioStreamReceiver(QObject *parent = nullptr) : IAudioStreamReceiver(parent) {}
    ~MockAudioStreamReceiver() override = default;

    bool bind() override
    {
        bindCalled_++;
        return bindResult_;
    }

    void close() override { closeCalled_++; }

    [[nodiscard]] bool isActive() const override { return isActive_; }

    void setAudioFormat(AudioFormat format) override
    {
        setAudioFormatCalled_++;
        lastAudioFormat_ = format;
    }

    /// @name Mock control methods
    /// @{
    void mockSetBindResult(bool result) { bindResult_ = result; }
    void mockSetIsActive(bool active) { isActive_ = active; }

    [[nodiscard]] int mockBindCallCount() const { return bindCalled_; }
    [[nodiscard]] int mockCloseCallCount() const { return closeCalled_; }
    [[nodiscard]] int mockSetAudioFormatCallCount() const { return setAudioFormatCalled_; }
    [[nodiscard]] AudioFormat mockLastAudioFormat() const { return lastAudioFormat_; }
    /// @}

private:
    bool bindResult_ = true;
    bool isActive_ = false;
    int bindCalled_ = 0;
    int closeCalled_ = 0;
    int setAudioFormatCalled_ = 0;
    AudioFormat lastAudioFormat_ = AudioFormat::Unknown;
};

#endif  // MOCKAUDIOSTREAMRECEIVER_H
