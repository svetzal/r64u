#ifndef TRANSFEREVENTPROCESSOR_H
#define TRANSFEREVENTPROCESSOR_H

#include <QObject>
#include <QQueue>

#include <functional>

/// Manages the deferred event queue used by TransferQueue to serialise
/// processNext() calls onto the Qt event loop.
class TransferEventProcessor : public QObject
{
    Q_OBJECT

public:
    explicit TransferEventProcessor(QObject *parent = nullptr);

    /// Enqueue an event and schedule processing on the next event-loop tick.
    void schedule(std::function<void()> event);

    /// Synchronously drain the queue (for tests).
    void flush();

private slots:
    void processQueue();

private:
    QQueue<std::function<void()>> queue_;
    bool processing_ = false;
    bool scheduled_ = false;
};

#endif  // TRANSFEREVENTPROCESSOR_H
