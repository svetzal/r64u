#include "transfertimeoutmanager.h"

TransferTimeoutManager::TransferTimeoutManager(QObject *parent)
    : QObject(parent), timer_(new QTimer(this))
{
    timer_->setSingleShot(true);
    connect(timer_, &QTimer::timeout, this, &TransferTimeoutManager::onTimerExpired);
}

void TransferTimeoutManager::start()
{
    timer_->start(OperationTimeoutMs);
}

void TransferTimeoutManager::stop()
{
    timer_->stop();
}

void TransferTimeoutManager::onTimerExpired()
{
    emit operationTimedOut();
}
