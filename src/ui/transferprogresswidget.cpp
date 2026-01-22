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
        // Note: overwriteConfirmationNeeded and folderExistsConfirmationNeeded signals
        // are handled by TransferProgressContainer, not this widget
        connect(transferService_, &TransferService::scanningStarted,
                this, &TransferProgressWidget::onScanningStarted);
        connect(transferService_, &TransferService::scanningProgress,
                this, &TransferProgressWidget::onScanningProgress);
        connect(transferService_, &TransferService::directoryCreationProgress,
                this, &TransferProgressWidget::onDirectoryCreationProgress);
        connect(cancelButton_, &QPushButton::clicked,
                transferService_, &TransferService::cancelAll);
    }
}

void TransferProgressWidget::onOperationStarted(const QString &fileName, OperationType type)
{
    Q_UNUSED(fileName)

    currentOperationType_ = type;

    // Start the delay timer if not already pending
    if (!progressPending_ && !isVisible()) {
        progressPending_ = true;
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

    if (isVisible()) {
        updateProgressDisplay();
    }
}

void TransferProgressWidget::onOperationFailed(const QString &fileName, const QString &error)
{
    emit statusMessage(tr("Operation failed: %1 - %2").arg(fileName, error), 5000);

    if (isVisible()) {
        updateProgressDisplay();
    }
}

void TransferProgressWidget::onQueueChanged()
{
    // Simply update the display - batch progress is now tracked in TransferQueue
    if (isVisible()) {
        updateProgressDisplay();
    }
}

void TransferProgressWidget::onAllOperationsCompleted()
{
    delayTimer_->stop();
    progressPending_ = false;

    setVisible(false);

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
    // Get batch progress from the service
    BatchProgress batchProgress = transferService_->activeBatchProgress();

    if (batchProgress.isCreatingDirectories && batchProgress.directoriesToCreate > 0) {
        // Directory creation in progress
        progressBar_->setMaximum(100);
        int progress = (batchProgress.directoriesCreated * 100) / batchProgress.directoriesToCreate;
        progressBar_->setValue(progress);
        statusLabel_->setText(tr("Creating directories... (%1 of %2)")
            .arg(batchProgress.directoriesCreated)
            .arg(batchProgress.directoriesToCreate));
    } else if (batchProgress.isScanning) {
        progressBar_->setMaximum(0);
        if (transferService_->isScanningForDelete()) {
            if (batchProgress.filesDiscovered > 0) {
                statusLabel_->setText(tr("Scanning for delete... (scanned %1 dirs, found %2 items)")
                    .arg(batchProgress.directoriesScanned)
                    .arg(batchProgress.filesDiscovered));
            } else {
                statusLabel_->setText(tr("Scanning for delete... (scanned %1 dirs)")
                    .arg(batchProgress.directoriesScanned));
            }
        } else {
            if (batchProgress.filesDiscovered > 0) {
                statusLabel_->setText(tr("Scanning directories... (scanned %1 dirs, found %2 files)")
                    .arg(batchProgress.directoriesScanned)
                    .arg(batchProgress.filesDiscovered));
            } else {
                statusLabel_->setText(tr("Scanning directories... (scanned %1 dirs)")
                    .arg(batchProgress.directoriesScanned));
            }
        }
    } else if (batchProgress.isProcessingDelete) {
        // Delete operations use their own progress tracking
        int total = batchProgress.deleteTotalCount;
        int completed = batchProgress.deleteProgress;

        progressBar_->setMaximum(100);
        int progress = (total > 0) ? (completed * 100) / total : 0;
        progressBar_->setValue(progress);

        // Cap at total to avoid showing "17 of 16" when complete
        int displayItem = qMin(completed + 1, total);
        statusLabel_->setText(tr("Deleting %1 of %2 items...")
            .arg(displayItem)
            .arg(total));
    } else if (batchProgress.isValid() && batchProgress.totalItems > 0) {
        progressBar_->setMaximum(100);
        int completed = batchProgress.completedItems + batchProgress.failedItems;
        int progress = (completed * 100) / batchProgress.totalItems;
        progressBar_->setValue(progress);

        QString actionVerb;
        switch (batchProgress.operationType) {
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
        // Cap at totalItems to avoid showing "17 of 16" when complete
        int displayItem = qMin(completed + 1, batchProgress.totalItems);
        if (!batchProgress.folderName.isEmpty()) {
            statusLabel_->setText(tr("%1 - %2 %3 of %4 items...")
                .arg(batchProgress.folderName)
                .arg(actionVerb)
                .arg(displayItem)
                .arg(batchProgress.totalItems));
        } else {
            statusLabel_->setText(tr("%1 %2 of %3 items...")
                .arg(actionVerb)
                .arg(displayItem)
                .arg(batchProgress.totalItems));
        }
    }
}

void TransferProgressWidget::onScanningStarted(const QString &folderName, OperationType type)
{
    currentOperationType_ = type;

    // Cancel any pending delay timer - scanning should show immediately
    delayTimer_->stop();
    progressPending_ = false;

    // Show the progress widget immediately for scanning
    setVisible(true);
    progressBar_->setMaximum(0);  // Indeterminate initially

    QString actionVerb = (type == OperationType::Delete) ? tr("Scanning for delete") : tr("Scanning");
    statusLabel_->setText(tr("%1: %2...").arg(actionVerb, folderName));
}

void TransferProgressWidget::onScanningProgress(int directoriesScanned, int directoriesRemaining, int filesDiscovered)
{
    // Ensure widget is visible
    if (!isVisible()) {
        setVisible(true);
    }

    // Update progress display
    if (directoriesRemaining > 0) {
        // Still scanning - show progress with indeterminate bar
        progressBar_->setMaximum(0);

        QString actionVerb = (currentOperationType_ == OperationType::Delete) ?
            tr("Scanning for delete") : tr("Scanning");

        if (filesDiscovered > 0) {
            statusLabel_->setText(tr("%1... (scanned %2 dirs, found %3 files, %4 dirs remaining)")
                .arg(actionVerb)
                .arg(directoriesScanned)
                .arg(filesDiscovered)
                .arg(directoriesRemaining));
        } else {
            statusLabel_->setText(tr("%1... (scanned %2 dirs, %3 remaining)")
                .arg(actionVerb)
                .arg(directoriesScanned)
                .arg(directoriesRemaining));
        }
    } else {
        // Scanning complete - show total found
        if (filesDiscovered > 0) {
            statusLabel_->setText(tr("Scan complete: found %1 files in %2 directories")
                .arg(filesDiscovered)
                .arg(directoriesScanned));
        }
        // The progress bar will update when actual transfers start
    }
}

void TransferProgressWidget::onDirectoryCreationProgress(int created, int total)
{
    // Ensure widget is visible
    if (!isVisible()) {
        setVisible(true);
    }

    // Show directory creation progress
    progressBar_->setMaximum(100);
    int progress = (total > 0) ? (created * 100) / total : 0;
    progressBar_->setValue(progress);

    statusLabel_->setText(tr("Creating directories... (%1 of %2)")
        .arg(created)
        .arg(total));
}
