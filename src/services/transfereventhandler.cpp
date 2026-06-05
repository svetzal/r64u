#include "transfereventhandler.h"

#include <QTimer>

TransferEventHandler::TransferEventHandler(QObject *parent) : QObject(parent) {}

void TransferEventHandler::schedule(std::function<void()> event)
{
    queue_.enqueue(std::move(event));

    if (!scheduled_) {
        scheduled_ = true;
        QTimer::singleShot(0, this, &TransferEventHandler::processQueue);
    }
}

void TransferEventHandler::flush()
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

void TransferEventHandler::processQueue()
{
    scheduled_ = false;

    if (processing_) {
        if (!queue_.isEmpty() && !scheduled_) {
            scheduled_ = true;
            QTimer::singleShot(0, this, &TransferEventHandler::processQueue);
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
