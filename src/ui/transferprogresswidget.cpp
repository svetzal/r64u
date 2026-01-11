#include "transferprogresswidget.h"
#include "services/transferservice.h"
#include "models/transferqueue.h"

#include <QHBoxLayout>
#include <QMessageBox>

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

    cancelButton_ = new QPushButton(tr("Cancel"));
    cancelButton_->setMaximumWidth(80);
    layout->addWidget(cancelButton_);

    // Initially hide the widget
    setVisible(false);

    // Setup 2-second delay timer for progress display
    delayTimer_ = new QTimer(this);
    delayTimer_->setSingleShot(true);
    connect(delayTimer_, &QTimer::timeout,
            this, &TransferProgressWidget::onShowProgress);
}

void TransferProgressWidget::setTransferService(TransferService *service)
{
    if (transferService_) {
        disconnect(transferService_, nullptr, this, nullptr);
        disconnect(cancelButton_, nullptr, transferService_, nullptr);
    }

    transferService_ = service;

    if (transferService_) {
        connect(transferService_, &TransferService::operationStarted,
                this, &TransferProgressWidget::onOperationStarted);
        connect(transferService_, &TransferService::operationCompleted,
                this, &TransferProgressWidget::onOperationCompleted);
        connect(transferService_, &TransferService::operationFailed,
                this, &TransferProgressWidget::onOperationFailed);
        connect(transferService_, &TransferService::allOperationsCompleted,
                this, &TransferProgressWidget::onAllOperationsCompleted);
        connect(transferService_, &TransferService::operationsCancelled,
                this, &TransferProgressWidget::onOperationsCancelled);
        connect(transferService_, &TransferService::queueChanged,
                this, &TransferProgressWidget::onQueueChanged);
        connect(transferService_, &TransferService::deleteProgressUpdate,
                this, &TransferProgressWidget::onDeleteProgressUpdate);
        connect(transferService_, &TransferService::overwriteConfirmationNeeded,
                this, &TransferProgressWidget::onOverwriteConfirmationNeeded);
        connect(transferService_, &TransferService::folderExistsConfirmationNeeded,
                this, &TransferProgressWidget::onFolderExistsConfirmationNeeded);
        connect(cancelButton_, &QPushButton::clicked,
                transferService_, &TransferService::cancelAll);
    }
}

void TransferProgressWidget::onOperationStarted(const QString &fileName, OperationType type)
{
    Q_UNUSED(fileName)

    currentOperationType_ = type;

    if (!progressPending_ && !isVisible()) {
        progressPending_ = true;
        // Use activeAndPendingCount to exclude completed items from previous operations
        operationTotalCount_ = transferService_->activeAndPendingCount();
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
    emit statusMessage(tr("%1: %2").arg(actionVerb, fileName), 2000);
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
    // Calculate total as completed + remaining (excludes old completed items from previous operations)
    operationTotalCount_ = operationCompletedCount_ + transferService_->activeAndPendingCount();

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

    // Clear any queued status messages before showing the final message
    emit clearStatusMessages();
    emit statusMessage(tr("All operations completed"), 3000);
}

void TransferProgressWidget::onOperationsCancelled()
{
    delayTimer_->stop();
    progressPending_ = false;

    setVisible(false);

    operationTotalCount_ = 0;
    operationCompletedCount_ = 0;

    progressBar_->setMaximum(100);
    progressBar_->setValue(0);
    statusLabel_->setText(tr("Ready"));

    // Clear any queued status messages before showing the cancellation message
    emit clearStatusMessages();
    emit statusMessage(tr("Operations cancelled"), 3000);
}

void TransferProgressWidget::onShowProgress()
{
    progressPending_ = false;

    if (transferService_->isProcessing() || transferService_->isScanning()) {
        setVisible(true);
        updateProgressDisplay();
    }
}

void TransferProgressWidget::onDeleteProgressUpdate(const QString &fileName, int current, int total)
{
    // Use timeout 0 so message stays until replaced by next delete
    emit statusMessage(tr("Deleted %1 of %2: %3").arg(current).arg(total).arg(fileName), 0);
}

void TransferProgressWidget::updateProgressDisplay()
{
    if (transferService_->isScanning()) {
        progressBar_->setMaximum(0);
        if (transferService_->isScanningForDelete()) {
            statusLabel_->setText(tr("Scanning for delete..."));
        } else {
            statusLabel_->setText(tr("Scanning directories..."));
        }
    } else if (transferService_->isProcessingDelete()) {
        // Delete operations use their own progress tracking
        int total = transferService_->deleteTotalCount();
        int completed = transferService_->deleteProgress();

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

void TransferProgressWidget::onOverwriteConfirmationNeeded(const QString &fileName, OperationType type)
{
    QString typeStr = (type == OperationType::Download) ? tr("download") : tr("upload");

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("File Already Exists"));
    msgBox.setText(tr("The file '%1' already exists.\n\n"
                      "Do you want to overwrite it?").arg(fileName));
    msgBox.setIcon(QMessageBox::Question);

    QPushButton *overwriteButton = msgBox.addButton(tr("Overwrite"), QMessageBox::AcceptRole);
    QPushButton *overwriteAllButton = msgBox.addButton(tr("Overwrite All"), QMessageBox::AcceptRole);
    QPushButton *skipButton = msgBox.addButton(tr("Skip"), QMessageBox::RejectRole);
    QPushButton *cancelButton = msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);

    msgBox.setDefaultButton(skipButton);
    msgBox.exec();

    QAbstractButton *clicked = msgBox.clickedButton();

    if (clicked == overwriteButton) {
        transferService_->respondToOverwrite(OverwriteResponse::Overwrite);
    } else if (clicked == overwriteAllButton) {
        transferService_->respondToOverwrite(OverwriteResponse::OverwriteAll);
    } else if (clicked == skipButton) {
        transferService_->respondToOverwrite(OverwriteResponse::Skip);
    } else if (clicked == cancelButton) {
        transferService_->respondToOverwrite(OverwriteResponse::Cancel);
    }
}

void TransferProgressWidget::onFolderExistsConfirmationNeeded(const QString &folderName)
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Folder Already Exists"));
    msgBox.setText(tr("The folder '%1' already exists on the remote device.\n\n"
                      "What would you like to do?").arg(folderName));
    msgBox.setIcon(QMessageBox::Question);

    QPushButton *mergeButton = msgBox.addButton(tr("Merge"), QMessageBox::AcceptRole);
    QPushButton *replaceButton = msgBox.addButton(tr("Replace"), QMessageBox::DestructiveRole);
    QPushButton *cancelButton = msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);

    msgBox.setDefaultButton(mergeButton);
    msgBox.exec();

    QAbstractButton *clicked = msgBox.clickedButton();

    if (clicked == mergeButton) {
        transferService_->respondToFolderExists(FolderExistsResponse::Merge);
    } else if (clicked == replaceButton) {
        transferService_->respondToFolderExists(FolderExistsResponse::Replace);
    } else if (clicked == cancelButton) {
        transferService_->respondToFolderExists(FolderExistsResponse::Cancel);
    }
}
