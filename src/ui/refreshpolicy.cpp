#include "refreshpolicy.h"

#include "models/remotefilemodel.h"
#include "utils/logging.h"

RefreshPolicy::RefreshPolicy(QObject *parent) : QObject(parent) {}

void RefreshPolicy::setModel(RemoteFileModel *model)
{
    model_ = model;
}

void RefreshPolicy::setConnected(bool connected)
{
    connected_ = connected;
}

RefreshPolicy::Suppressor::Suppressor(RefreshPolicy *policy) : policy_(policy)
{
    policy_->setSuppressed(true);
}

RefreshPolicy::Suppressor::~Suppressor()
{
    policy_->setSuppressed(false);
}

RefreshPolicy::Suppressor RefreshPolicy::suppress()
{
    return Suppressor(this);
}

void RefreshPolicy::setSuppressed(bool suppressed)
{
    suppressed_ = suppressed;
}

void RefreshPolicy::refreshIfStale()
{
    if (!connected_ || suppressed_) {
        qCDebug(LogUi) << "RefreshPolicy::refreshIfStale: skipped (connected=" << connected_
                       << "suppress=" << suppressed_ << ")";
        return;
    }

    if (model_) {
        model_->refreshIfStale();
    }
}
