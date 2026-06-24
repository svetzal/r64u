#include "transferhandlerbase.h"

#include "../core/ftpclientmixin.h"
#include "../utils/logging.h"

TransferHandlerBase::TransferHandlerBase(transfer::State &state, QObject *parent)
    : QObject(parent), state_(state)
{
}

void TransferHandlerBase::setFtpClient(IFtpClient *client)
{
    disconnectFtpClient(ftpClient_, this);
    ftpClient_ = client;
    if (ftpClient_) {
        connectFtpSignals();
    }
}

bool TransferHandlerBase::ftpConnected() const
{
    return ftpClient_ && ftpClient_->isConnected();
}

void TransferHandlerBase::notifyQueueChanged()
{
    emit queueChanged();
    emit scheduleProcessNextRequested();
}

void TransferHandlerBase::markCurrentComplete(transfer::TransferItem::Status status)
{
    if (state_.currentIndex < 0 || state_.currentIndex >= state_.items.size()) {
        qCWarning(LogTransfer) << "TransferHandlerBase::markCurrentComplete: invalid index"
                               << state_.currentIndex;
        return;
    }

    int completedIndex = state_.currentIndex;
    auto result = transfer::markItemComplete(state_, completedIndex, status);
    state_ = result.newState;

    emit itemDataChanged(completedIndex);

    if (result.batchId >= 0) {
        emit batchProgressRequested(result.batchId, result.batchIsComplete, false);
    }
}

void TransferHandlerBase::completeAndNotify()
{
    auto result = transfer::completeTransferOperation(state_);
    state_ = result.newState;
    notifyQueueChanged();
}
