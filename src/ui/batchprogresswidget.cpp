#include "batchprogresswidget.h"

#include <QHBoxLayout>

BatchProgressWidget::BatchProgressWidget(int batchId, QWidget *parent)
    : QWidget(parent)
    , batchId_(batchId)
{
    setupUi();
}

void BatchProgressWidget::setupUi()
{
    setMaximumHeight(40);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(8);

    iconLabel_ = new QLabel();
    iconLabel_->setFixedWidth(24);
    iconLabel_->setAlignment(Qt::AlignCenter);
    layout->addWidget(iconLabel_);

    statusLabel_ = new QLabel();
    statusLabel_->setMinimumWidth(200);
    layout->addWidget(statusLabel_);

    progressBar_ = new QProgressBar();
    progressBar_->setMinimum(0);
    progressBar_->setMaximum(100);
    progressBar_->setValue(0);
    progressBar_->setTextVisible(false);
    layout->addWidget(progressBar_, 1);

    cancelButton_ = new QPushButton(tr("X"));
    cancelButton_->setMaximumWidth(30);
    cancelButton_->setToolTip(tr("Cancel this operation"));
    layout->addWidget(cancelButton_);

    connect(cancelButton_, &QPushButton::clicked, this, [this]() {
        emit cancelRequested(batchId_);
    });

    updateStateAppearance();
}

void BatchProgressWidget::updateProgress(const BatchProgress &progress)
{
    operationType_ = progress.operationType;

    // Determine state from progress
    if (progress.isScanning) {
        setState(State::Scanning);
        if (progress.filesDiscovered > 0) {
            statusLabel_->setText(tr("Scanning... (%1 dirs, %2 files)")
                .arg(progress.directoriesScanned)
                .arg(progress.filesDiscovered));
        } else {
            statusLabel_->setText(tr("Scanning... (%1 dirs)")
                .arg(progress.directoriesScanned));
        }
    } else if (progress.isCreatingDirectories && progress.directoriesToCreate > 0) {
        setState(State::Creating);
        progressBar_->setMaximum(100);
        int pct = (progress.directoriesCreated * 100) / progress.directoriesToCreate;
        progressBar_->setValue(pct);
        statusLabel_->setText(tr("Creating directories (%1 of %2)")
            .arg(progress.directoriesCreated)
            .arg(progress.directoriesToCreate));
    } else if (progress.isProcessingDelete) {
        setState(State::Active);
        progressBar_->setMaximum(100);
        int total = progress.deleteTotalCount;
        int completed = progress.deleteProgress;
        int pct = (total > 0) ? (completed * 100) / total : 0;
        progressBar_->setValue(pct);
        // Cap at total to avoid showing "17 of 16" when complete
        int displayItem = qMin(completed + 1, total);
        statusLabel_->setText(tr("Deleting %1 of %2 items...")
            .arg(displayItem)
            .arg(total));
    } else if (progress.totalItems > 0) {
        setState(State::Active);
        progressBar_->setMaximum(100);
        int completed = progress.completedItems + progress.failedItems;
        int pct = (completed * 100) / progress.totalItems;
        progressBar_->setValue(pct);

        QString actionVerb;
        switch (progress.operationType) {
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
        // Show "X of Y" where X is the item currently being processed (1-indexed)
        // Cap at totalItems to avoid showing "17 of 16" when complete
        int displayItem = qMin(completed + 1, progress.totalItems);
        statusLabel_->setText(tr("%1 %2 of %3 items...")
            .arg(actionVerb)
            .arg(displayItem)
            .arg(progress.totalItems));
    }

    // Update icon based on operation type
    iconLabel_->setText(operationIcon(operationType_));
}

void BatchProgressWidget::setState(State state)
{
    if (state_ == state) {
        return;
    }
    state_ = state;
    updateStateAppearance();
}

void BatchProgressWidget::setDescription(const QString &description)
{
    statusLabel_->setText(description);
}

void BatchProgressWidget::setActive(bool active)
{
    if (isActive_ == active) {
        return;
    }
    isActive_ = active;
    updateStateAppearance();
}

void BatchProgressWidget::setOperationType(OperationType type)
{
    operationType_ = type;
    iconLabel_->setText(operationIcon(type));
}

void BatchProgressWidget::updateStateAppearance()
{
    switch (state_) {
    case State::Queued:
        progressBar_->setVisible(false);
        setEnabled(true);
        // Slightly dimmed appearance for queued items
        setStyleSheet("QLabel { color: gray; }");
        break;

    case State::Scanning:
        progressBar_->setVisible(true);
        progressBar_->setMaximum(0);  // Indeterminate
        setEnabled(true);
        setStyleSheet("");
        break;

    case State::Creating:
        progressBar_->setVisible(true);
        progressBar_->setMaximum(100);
        setEnabled(true);
        setStyleSheet("");
        break;

    case State::Active:
        progressBar_->setVisible(true);
        progressBar_->setMaximum(100);
        setEnabled(true);
        setStyleSheet("");
        break;

    case State::Completed:
        progressBar_->setVisible(true);
        progressBar_->setValue(100);
        setEnabled(false);
        setStyleSheet("QLabel { color: green; }");
        break;
    }
}

QString BatchProgressWidget::operationIcon(OperationType type) const
{
    // Use simple text icons - could be replaced with actual icons later
    switch (type) {
    case OperationType::Upload:
        return QString::fromUtf8("\u2191");  // Up arrow
    case OperationType::Download:
        return QString::fromUtf8("\u2193");  // Down arrow
    case OperationType::Delete:
        return QString::fromUtf8("\u2717");  // X mark
    }
    return QString();
}
