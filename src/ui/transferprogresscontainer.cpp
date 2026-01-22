#include "transferprogresscontainer.h"
#include "batchprogresswidget.h"
#include "services/transferservice.h"

#include <QSizePolicy>
#include <QDebug>
#include <QMessageBox>
#include <QPushButton>

TransferProgressContainer::TransferProgressContainer(QWidget *parent)
    : QWidget(parent)
{
    setupUi();

    // Create debounce timer for queueChanged signals
    queueChangedDebounceTimer_ = new QTimer(this);
    queueChangedDebounceTimer_->setSingleShot(true);
    connect(queueChangedDebounceTimer_, &QTimer::timeout,
            this, &TransferProgressContainer::processQueueChanged);
}

void TransferProgressContainer::setupUi()
{
    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->setSpacing(2);

    // Allow container to grow vertically to fit all batch widgets
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    // Start hidden - will show when batches are added
    setVisible(false);
}

void TransferProgressContainer::setTransferService(TransferService *service)
{
    if (transferService_) {
        disconnect(transferService_, nullptr, this, nullptr);
    }

    transferService_ = service;

    if (transferService_) {
        // Queue changed - catch new batches being added
        connect(transferService_, &TransferService::queueChanged,
                this, &TransferProgressContainer::onQueueChanged);

        // Batch lifecycle signals
        connect(transferService_, &TransferService::batchStarted,
                this, &TransferProgressContainer::onBatchStarted);
        connect(transferService_, &TransferService::batchProgressUpdate,
                this, &TransferProgressContainer::onBatchProgressUpdate);
        connect(transferService_, &TransferService::batchCompleted,
                this, &TransferProgressContainer::onBatchCompleted);

        // Operation signals (for per-file status messages)
        connect(transferService_, &TransferService::operationStarted,
                this, &TransferProgressContainer::onOperationStarted);
        connect(transferService_, &TransferService::operationCompleted,
                this, &TransferProgressContainer::onOperationCompleted);
        connect(transferService_, &TransferService::operationFailed,
                this, &TransferProgressContainer::onOperationFailed);
        connect(transferService_, &TransferService::allOperationsCompleted,
                this, &TransferProgressContainer::onAllOperationsCompleted);
        connect(transferService_, &TransferService::operationsCancelled,
                this, &TransferProgressContainer::onOperationsCancelled);

        // Scanning/creation progress
        connect(transferService_, &TransferService::scanningStarted,
                this, &TransferProgressContainer::onScanningStarted);
        connect(transferService_, &TransferService::scanningProgress,
                this, &TransferProgressContainer::onScanningProgress);
        connect(transferService_, &TransferService::directoryCreationProgress,
                this, &TransferProgressContainer::onDirectoryCreationProgress);

        // Confirmation dialogs
        connect(transferService_, &TransferService::overwriteConfirmationNeeded,
                this, &TransferProgressContainer::onOverwriteConfirmationNeeded);
        connect(transferService_, &TransferService::folderExistsConfirmationNeeded,
                this, &TransferProgressContainer::onFolderExistsConfirmationNeeded);
    }
}

void TransferProgressContainer::onQueueChanged()
{
    // Debounce rapid-fire queueChanged signals to prevent UI freeze
    // during fast transfers (e.g., many small files)
    queueChangedDebounceTimer_->start(kQueueChangedDebounceMs);
}

void TransferProgressContainer::processQueueChanged()
{
    if (!transferService_) {
        return;
    }

    // Create widgets for any new batches
    QList<int> allBatchIds = transferService_->allBatchIds();
    qDebug() << "TransferProgressContainer::processQueueChanged - batches:" << allBatchIds.size()
             << "widgets:" << widgets_.size() << "batchIds:" << allBatchIds;
    for (int batchId : allBatchIds) {
        if (!widgets_.contains(batchId)) {
            qDebug() << "  Creating widget for batch" << batchId;
            BatchProgressWidget *widget = findOrCreateWidget(batchId);
            BatchProgress progress = transferService_->batchProgress(batchId);

            // Set operation type (for icon)
            widget->setOperationType(progress.operationType);

            // Set initial description - use batch description with "queued" suffix
            QString desc;
            if (!progress.description.isEmpty()) {
                // Use the batch's own description (e.g., "Downloading folder1")
                desc = tr("%1 (queued)").arg(progress.description);
            } else {
                // Fallback to generic description
                switch (progress.operationType) {
                case OperationType::Upload:
                    desc = tr("Upload queued");
                    break;
                case OperationType::Download:
                    desc = tr("Download queued");
                    break;
                case OperationType::Delete:
                    desc = tr("Delete queued");
                    break;
                }
            }
            if (progress.totalItems > 0) {
                desc = tr("%1 - %2 items").arg(desc).arg(progress.totalItems);
            }
            widget->setDescription(desc);
            widget->setState(BatchProgressWidget::State::Queued);
        }
    }

    // Remove widgets for batches that no longer exist
    QList<int> widgetIds = widgets_.keys();
    for (int batchId : widgetIds) {
        if (!allBatchIds.contains(batchId)) {
            onRemoveBatchWidget(batchId);
        }
    }

    updateVisibility();
}

void TransferProgressContainer::onBatchStarted(int batchId)
{
    BatchProgressWidget *widget = findOrCreateWidget(batchId);

    // Mark this batch as active
    widget->setActive(true);

    // Update all widgets to reflect the new state
    updateAllBatchWidgets();
    updateVisibility();
}

void TransferProgressContainer::onBatchProgressUpdate(int batchId, int completed, int total)
{
    Q_UNUSED(completed)
    Q_UNUSED(total)

    // Update the active batch's widget
    if (widgets_.contains(batchId) && transferService_) {
        BatchProgress progress = transferService_->activeBatchProgress();
        if (progress.batchId == batchId) {
            widgets_[batchId]->updateProgress(progress);
        }
    }
}

void TransferProgressContainer::onBatchCompleted(int batchId)
{
    qDebug() << "TransferProgressContainer::onBatchCompleted - batchId:" << batchId
             << "widgets_.contains:" << widgets_.contains(batchId)
             << "widget count:" << widgets_.size();

    if (widgets_.contains(batchId)) {
        BatchProgressWidget *widget = widgets_[batchId];
        widget->setState(BatchProgressWidget::State::Completed);

        // Remove widget after brief delay for visual feedback
        QTimer::singleShot(500, this, [this, batchId]() {
            qDebug() << "TransferProgressContainer: Timer fired, removing widget for batch" << batchId;
            onRemoveBatchWidget(batchId);
        });
    } else {
        qDebug() << "TransferProgressContainer: No widget found for batch" << batchId;
    }
}

void TransferProgressContainer::onOperationStarted(const QString &fileName, OperationType type)
{
    Q_UNUSED(fileName)
    currentOperationType_ = type;
}

void TransferProgressContainer::onOperationCompleted(const QString &fileName)
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
}

void TransferProgressContainer::onOperationFailed(const QString &fileName, const QString &error)
{
    emit statusMessage(tr("Operation failed: %1 - %2").arg(fileName, error), 5000);
}

void TransferProgressContainer::onAllOperationsCompleted()
{
    emit clearStatusMessages();
    emit statusMessage(tr("All operations completed"), 3000);
}

void TransferProgressContainer::onOperationsCancelled()
{
    emit clearStatusMessages();
    emit statusMessage(tr("Operations cancelled"), 3000);
}

void TransferProgressContainer::onScanningStarted(const QString &folderName, OperationType type)
{
    currentOperationType_ = type;

    // Get current batch and update its widget
    if (transferService_ && transferService_->hasActiveBatch()) {
        BatchProgress progress = transferService_->activeBatchProgress();
        BatchProgressWidget *widget = findOrCreateWidget(progress.batchId);
        widget->setActive(true);
        widget->setState(BatchProgressWidget::State::Scanning);

        QString actionVerb = (type == OperationType::Delete) ? tr("Scanning for delete") : tr("Scanning");
        widget->setDescription(tr("%1: %2...").arg(actionVerb, folderName));

        updateVisibility();
    }
}

void TransferProgressContainer::onScanningProgress(int directoriesScanned, int directoriesRemaining, int filesDiscovered)
{
    Q_UNUSED(directoriesRemaining)

    if (transferService_ && transferService_->hasActiveBatch()) {
        BatchProgress progress = transferService_->activeBatchProgress();
        if (widgets_.contains(progress.batchId)) {
            widgets_[progress.batchId]->updateProgress(progress);
        }
    }
}

void TransferProgressContainer::onDirectoryCreationProgress(int created, int total)
{
    Q_UNUSED(created)
    Q_UNUSED(total)

    if (transferService_ && transferService_->hasActiveBatch()) {
        BatchProgress progress = transferService_->activeBatchProgress();
        if (widgets_.contains(progress.batchId)) {
            widgets_[progress.batchId]->updateProgress(progress);
        }
    }
}

void TransferProgressContainer::onCancelRequested(int batchId)
{
    if (transferService_) {
        transferService_->cancelBatch(batchId);
    }
}

void TransferProgressContainer::onRemoveBatchWidget(int batchId)
{
    if (widgets_.contains(batchId)) {
        BatchProgressWidget *widget = widgets_.take(batchId);
        layout_->removeWidget(widget);
        widget->deleteLater();
        updateVisibility();
    }
}

void TransferProgressContainer::updateVisibility()
{
    setVisible(!widgets_.isEmpty());
}

BatchProgressWidget* TransferProgressContainer::findOrCreateWidget(int batchId)
{
    if (widgets_.contains(batchId)) {
        return widgets_[batchId];
    }

    // Create new widget for this batch
    auto *widget = new BatchProgressWidget(batchId, this);
    widgets_[batchId] = widget;
    layout_->addWidget(widget);

    // Connect cancel signal
    connect(widget, &BatchProgressWidget::cancelRequested,
            this, &TransferProgressContainer::onCancelRequested);

    return widget;
}

void TransferProgressContainer::updateAllBatchWidgets()
{
    if (!transferService_) {
        return;
    }

    // Update the active batch
    if (transferService_->hasActiveBatch()) {
        BatchProgress progress = transferService_->activeBatchProgress();
        if (widgets_.contains(progress.batchId)) {
            widgets_[progress.batchId]->setActive(true);
            widgets_[progress.batchId]->updateProgress(progress);
        }
    }

    // Mark non-active batches as queued
    for (auto it = widgets_.begin(); it != widgets_.end(); ++it) {
        if (transferService_->hasActiveBatch()) {
            BatchProgress progress = transferService_->activeBatchProgress();
            if (it.key() != progress.batchId) {
                it.value()->setActive(false);
                it.value()->setState(BatchProgressWidget::State::Queued);
            }
        }
    }
}

void TransferProgressContainer::onOverwriteConfirmationNeeded(const QString &fileName, OperationType type)
{
    Q_UNUSED(type)

    // Use the top-level window as parent so dialog shows even if container is hidden
    QWidget *dialogParent = window();
    QMessageBox msgBox(dialogParent);
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
    } else {
        // Cancel button clicked OR dialog dismissed (Escape/X button)
        transferService_->respondToOverwrite(OverwriteResponse::Cancel);
    }
}

void TransferProgressContainer::onFolderExistsConfirmationNeeded(const QStringList &folderNames)
{
    // Use the top-level window as parent so dialog shows even if container is hidden
    QWidget *dialogParent = window();
    QMessageBox msgBox(dialogParent);

    QString title;
    QString message;

    if (folderNames.size() == 1) {
        title = tr("Folder Already Exists");
        message = tr("The folder '%1' already exists on the remote device.\n\n"
                     "What would you like to do?").arg(folderNames.first());
    } else {
        title = tr("Folders Already Exist");
        message = tr("The following %1 folders already exist on the remote device:\n\n"
                     "%2\n\n"
                     "What would you like to do?")
                      .arg(folderNames.size())
                      .arg(folderNames.join("\n"));
    }

    msgBox.setWindowTitle(title);
    msgBox.setText(message);
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
    } else {
        // Cancel button clicked OR dialog dismissed (Escape/X button)
        transferService_->respondToFolderExists(FolderExistsResponse::Cancel);
    }
}
