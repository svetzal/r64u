#include "transfereventprocessor.h"

#include <QTimer>

TransferEventProcessor::TransferEventProcessor(QObject *parent) : QObject(parent) {}

void TransferEventProcessor::schedule(std::function<void()> event)
{
    queue_.enqueue(std::move(event));

    if (!scheduled_) {
        scheduled_ = true;
        QTimer::singleShot(0, this, &TransferEventProcessor::processQueue);
    }
}

void TransferEventProcessor::flush()
{
    if (processing_) {
        return;
    }

    scheduled_ = false;
    processing_ = true;

    while (!queue_.isEmpty()) {
        auto event = queue_.dequeue();
        event();
    }

    processing_ = false;
}

void TransferEventProcessor::processQueue()
{
    scheduled_ = false;

    if (processing_) {
        if (!queue_.isEmpty() && !scheduled_) {
            scheduled_ = true;
            QTimer::singleShot(0, this, &TransferEventProcessor::processQueue);
        }
        return;
    }

    processing_ = true;
    while (!queue_.isEmpty()) {
        auto event = queue_.dequeue();
        event();
    }
    processing_ = false;
}
