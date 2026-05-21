#include "refreshpolicymanager.h"

#include "models/remotefilemodel.h"
#include "utils/logging.h"

RefreshPolicyManager::RefreshPolicyManager(QObject *parent) : QObject(parent) {}

void RefreshPolicyManager::setModel(RemoteFileModel *model)
{
    model_ = model;
}

void RefreshPolicyManager::setConnected(bool connected)
{
    connected_ = connected;
}

RefreshPolicyManager::Suppressor::Suppressor(RefreshPolicyManager *policy) : policy_(policy)
{
    policy_->setSuppressed(true);
}

RefreshPolicyManager::Suppressor::~Suppressor()
{
    policy_->setSuppressed(false);
}

RefreshPolicyManager::Suppressor RefreshPolicyManager::suppress()
{
    return Suppressor(this);
}

void RefreshPolicyManager::setSuppressed(bool suppressed)
{
    suppressed_ = suppressed;
}

void RefreshPolicyManager::refreshIfStale()
{
    if (!connected_ || suppressed_) {
        qCDebug(LogUi) << "RefreshPolicyManager::refreshIfStale: skipped (connected=" << connected_
                       << "suppress=" << suppressed_ << ")";
        return;
    }

    if (model_) {
        model_->refreshIfStale();
    }
}
