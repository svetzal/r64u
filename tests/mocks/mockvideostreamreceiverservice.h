/**
 * @file mockvideostreamreceiverservice.h
 * @brief Mock video stream receiver for testing StreamingService.
 */

#ifndef MOCKVIDEOSTREAMRECEIVERSERVICE_H
#define MOCKVIDEOSTREAMRECEIVERSERVICE_H

#include "services/ivideostreamreceiverservice.h"

/**
 * @brief Mock implementation of IVideoStreamReceiverService for testing.
 *
 * Records bind/close calls. bindResult_ controls whether bind() succeeds.
 */
class MockVideoStreamReceiverService : public IVideoStreamReceiverService
{
    Q_OBJECT

public:
    explicit MockVideoStreamReceiverService(QObject *parent = nullptr)
        : IVideoStreamReceiverService(parent)
    {
    }
    ~MockVideoStreamReceiverService() override = default;

    bool bind() override
    {
        bindCalled_++;
        return bindResult_;
    }

    void close() override { closeCalled_++; }

    [[nodiscard]] bool isActive() const override { return isActive_; }

    /// @name Mock control methods
    /// @{
    void mockSetBindResult(bool result) { bindResult_ = result; }
    void mockSetIsActive(bool active) { isActive_ = active; }
    void mockEmitFormatDetected(VideoFormat format) { emit formatDetected(format); }

    [[nodiscard]] int mockBindCallCount() const { return bindCalled_; }
    [[nodiscard]] int mockCloseCallCount() const { return closeCalled_; }
    /// @}

private:
    bool bindResult_ = true;
    bool isActive_ = false;
    int bindCalled_ = 0;
    int closeCalled_ = 0;
};

#endif  // MOCKVIDEOSTREAMRECEIVERSERVICE_H
