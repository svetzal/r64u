#include "transfertimeoutmanager.h"

TransferTimeoutManager::TransferTimeoutManager(int timeoutMs, QObject *parent)
    : QObject(parent), timeoutMs_(timeoutMs), timer_(new QTimer(this))
{
    timer_->setSingleShot(true);
    connect(timer_, &QTimer::timeout, this, &TransferTimeoutManager::onTimerExpired);
}

void TransferTimeoutManager::start()
{
    timer_->start(timeoutMs_);
}

void TransferTimeoutManager::stop()
{
    timer_->stop();
}

void TransferTimeoutManager::onTimerExpired()
{
    emit operationTimedOut();
}
