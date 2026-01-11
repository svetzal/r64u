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
            this, &TransferProgressWidget::onShowProgress);
}

void TransferProgressWidget::setTransferQueue(TransferQueue *queue)
{
    if (transferQueue_) {
        disconnect(transferQueue_, nullptr, this, nullptr);
    }

    transferQueue_ = queue;

    if (transferQueue_) {
        connect(transferQueue_, &TransferQueue::operationStarted,
                this, &TransferProgressWidget::onOperationStarted);
        connect(transferQueue_, &TransferQueue::operationCompleted,
                this, &TransferProgressWidget::onOperationCompleted);
        connect(transferQueue_, &TransferQueue::operationFailed,
                this, &TransferProgressWidget::onOperationFailed);
        connect(transferQueue_, &TransferQueue::allOperationsCompleted,
                this, &TransferProgressWidget::onAllOperationsCompleted);
        connect(transferQueue_, &TransferQueue::queueChanged,
                this, &TransferProgressWidget::onQueueChanged);
        connect(transferQueue_, &TransferQueue::deleteProgressUpdate,
                this, &TransferProgressWidget::onDeleteProgressUpdate);
    }
}

void TransferProgressWidget::onOperationStarted(const QString &fileName, OperationType type)
{
    Q_UNUSED(fileName)

    currentOperationType_ = type;

    if (!progressPending_ && !isVisible()) {
        progressPending_ = true;
        operationTotalCount_ = transferQueue_->rowCount();
        operationCompletedCount_ = 0;
        delayTimer_->start(2000);
    }
}

void TransferProgressWidget::onOperationCompleted(const QString &fileName)
{
    QString actionVerb;
    switch (currentOperationType_) {
    case OperationType::Upload:
        actionVerb = tr("Uploaded");
        break;
    case OperationType::Download:
        actionVerb = tr("Downloaded");
        break;
    case OperationType::Delete:
        actionVerb = tr("Deleted");
        break;
    }
    emit statusMessage(tr("%1: %2").arg(actionVerb, fileName), 3000);
    operationCompletedCount_++;
    onQueueChanged();
}

void TransferProgressWidget::onOperationFailed(const QString &fileName, const QString &error)
{
    emit statusMessage(tr("Operation failed: %1 - %2").arg(fileName, error), 5000);
    operationCompletedCount_++;
    onQueueChanged();
}

void TransferProgressWidget::onQueueChanged()
{
    operationTotalCount_ = transferQueue_->rowCount();

    if (isVisible()) {
        updateProgressDisplay();
    }
}

void TransferProgressWidget::onAllOperationsCompleted()
{
    delayTimer_->stop();
    progressPending_ = false;

    setVisible(false);

    operationTotalCount_ = 0;
    operationCompletedCount_ = 0;

    progressBar_->setMaximum(100);
    progressBar_->setValue(0);
    statusLabel_->setText(tr("Ready"));
}

void TransferProgressWidget::onShowProgress()
{
    progressPending_ = false;

    if (transferQueue_->isProcessing() || transferQueue_->isScanning()) {
        setVisible(true);
        updateProgressDisplay();
    }
}

void TransferProgressWidget::onDeleteProgressUpdate(const QString &fileName, int current, int total)
{
    Q_UNUSED(current)
    Q_UNUSED(total)
    emit statusMessage(tr("Deleted: %1").arg(fileName), 1000);
}

void TransferProgressWidget::updateProgressDisplay()
{
    if (transferQueue_->isScanning()) {
        progressBar_->setMaximum(0);
        if (transferQueue_->isScanningForDelete()) {
            statusLabel_->setText(tr("Scanning for delete..."));
        } else {
            statusLabel_->setText(tr("Scanning directories..."));
        }
    } else if (transferQueue_->isProcessingDelete()) {
        // Delete operations use their own progress tracking
        int total = transferQueue_->deleteTotalCount();
        int completed = transferQueue_->deleteProgress();

        progressBar_->setMaximum(100);
        int progress = (total > 0) ? (completed * 100) / total : 0;
        progressBar_->setValue(progress);

        statusLabel_->setText(tr("Deleting %1 of %2 items...")
            .arg(completed + 1)
            .arg(total));
    } else if (operationTotalCount_ > 0) {
        progressBar_->setMaximum(100);
        int progress = (operationTotalCount_ > 0)
            ? (operationCompletedCount_ * 100) / operationTotalCount_
            : 0;
        progressBar_->setValue(progress);

        QString actionVerb;
        switch (currentOperationType_) {
        case OperationType::Upload:
            actionVerb = tr("Uploading");
            break;
        case OperationType::Download:
            actionVerb = tr("Downloading");
            break;
        case OperationType::Delete:
            actionVerb = tr("Deleting");
            break;
        }
        statusLabel_->setText(tr("%1 %2 of %3 items...")
            .arg(actionVerb)
            .arg(operationCompletedCount_ + 1)
            .arg(operationTotalCount_));
    }
}
