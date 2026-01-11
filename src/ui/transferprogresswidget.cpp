#include "transferprogresswidget.h"
#include "models/transferqueue.h"

#include <QHBoxLayout>

TransferProgressWidget::TransferProgressWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void TransferProgressWidget::setupUi()
{
    setMaximumHeight(60);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);

    statusLabel_ = new QLabel(tr("Ready"));
    statusLabel_->setMinimumWidth(200);
    layout->addWidget(statusLabel_);

    progressBar_ = new QProgressBar();
    progressBar_->setMinimum(0);
    progressBar_->setMaximum(100);
    progressBar_->setValue(0);
    layout->addWidget(progressBar_, 1);

    // Initially hide the widget
    setVisible(false);

    // Setup 2-second delay timer for progress display
    delayTimer_ = new QTimer(this);
    delayTimer_->setSingleShot(true);
    connect(delayTimer_, &QTimer::timeout,
            this, &TransferProgressWidget::onShowTransferProgress);
}

void TransferProgressWidget::setTransferQueue(TransferQueue *queue)
{
    if (transferQueue_) {
        disconnect(transferQueue_, nullptr, this, nullptr);
    }

    transferQueue_ = queue;

    if (transferQueue_) {
        connect(transferQueue_, &TransferQueue::transferStarted,
                this, &TransferProgressWidget::onTransferStarted);
        connect(transferQueue_, &TransferQueue::transferCompleted,
                this, &TransferProgressWidget::onTransferCompleted);
        connect(transferQueue_, &TransferQueue::transferFailed,
                this, &TransferProgressWidget::onTransferFailed);
        connect(transferQueue_, &TransferQueue::allTransfersCompleted,
                this, &TransferProgressWidget::onAllTransfersCompleted);
        connect(transferQueue_, &TransferQueue::queueChanged,
                this, &TransferProgressWidget::onTransferQueueChanged);
    }
}

void TransferProgressWidget::onTransferStarted(const QString &fileName)
{
    Q_UNUSED(fileName)

    if (!transferProgressPending_ && !isVisible()) {
        transferProgressPending_ = true;
        transferTotalCount_ = transferQueue_->rowCount();
        transferCompletedCount_ = 0;
        delayTimer_->start(2000);
    }
}

void TransferProgressWidget::onTransferCompleted(const QString &fileName)
{
    emit statusMessage(tr("Transfer complete: %1").arg(fileName), 3000);
    transferCompletedCount_++;
    onTransferQueueChanged();
}

void TransferProgressWidget::onTransferFailed(const QString &fileName, const QString &error)
{
    emit statusMessage(tr("Transfer failed: %1 - %2").arg(fileName, error), 5000);
    transferCompletedCount_++;
    onTransferQueueChanged();
}

void TransferProgressWidget::onTransferQueueChanged()
{
    transferTotalCount_ = transferQueue_->rowCount();

    if (isVisible()) {
        updateProgressDisplay();
    }
}

void TransferProgressWidget::onAllTransfersCompleted()
{
    delayTimer_->stop();
    transferProgressPending_ = false;

    setVisible(false);

    transferTotalCount_ = 0;
    transferCompletedCount_ = 0;

    progressBar_->setMaximum(100);
    progressBar_->setValue(0);
    statusLabel_->setText(tr("Ready"));
}

void TransferProgressWidget::onShowTransferProgress()
{
    transferProgressPending_ = false;

    if (transferQueue_->isProcessing() || transferQueue_->isScanning()) {
        setVisible(true);
        updateProgressDisplay();
    }
}

void TransferProgressWidget::updateProgressDisplay()
{
    if (transferQueue_->isScanning()) {
        progressBar_->setMaximum(0);
        statusLabel_->setText(tr("Scanning directories..."));
    } else if (transferTotalCount_ > 0) {
        progressBar_->setMaximum(100);
        int progress = (transferTotalCount_ > 0)
            ? (transferCompletedCount_ * 100) / transferTotalCount_
            : 0;
        progressBar_->setValue(progress);
        statusLabel_->setText(tr("Transferring %1 of %2 files...")
            .arg(transferCompletedCount_ + 1)
            .arg(transferTotalCount_));
    }
}
