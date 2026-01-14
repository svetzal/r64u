#ifndef BATCHPROGRESSWIDGET_H
#define BATCHPROGRESSWIDGET_H

#include <QWidget>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>

#include "models/transferqueue.h"

/**
 * @brief Widget displaying progress for a single transfer batch.
 *
 * Shows an icon, status label, progress bar, and cancel button.
 * Visual states:
 * - Scanning: Indeterminate progress, "Scanning folder..." text
 * - Active: Determinate progress, "X of Y items" text
 * - Queued: No progress bar, "Queued - N items" text, dimmed
 * - Completed: Brief "Completed" state before removal
 */
class BatchProgressWidget : public QWidget
{
    Q_OBJECT

public:
    enum class State {
        Queued,     ///< Waiting to start
        Scanning,   ///< Scanning directories
        Creating,   ///< Creating directories (upload)
        Active,     ///< Actively transferring
        Completed   ///< Finished (before removal)
    };

    explicit BatchProgressWidget(int batchId, QWidget *parent = nullptr);

    /**
     * @brief Updates the widget with progress information.
     * @param progress Current batch progress data.
     */
    void updateProgress(const BatchProgress &progress);

    /**
     * @brief Sets the display state of the widget.
     * @param state The visual state to display.
     */
    void setState(State state);

    /**
     * @brief Returns the batch ID this widget represents.
     */
    [[nodiscard]] int batchId() const { return batchId_; }

    /**
     * @brief Sets the operation description.
     * @param description Text like "Downloading folder1" or "Upload 5 files"
     */
    void setDescription(const QString &description);

    /**
     * @brief Sets whether this is the active batch (affects styling).
     */
    void setActive(bool active);

    /**
     * @brief Sets the operation type (updates the icon).
     * @param type The operation type (Download, Upload, Delete)
     */
    void setOperationType(OperationType type);

signals:
    /**
     * @brief Emitted when user clicks cancel for this batch.
     * @param batchId The batch ID to cancel.
     */
    void cancelRequested(int batchId);

private:
    void setupUi();
    void updateStateAppearance();
    QString operationIcon(OperationType type) const;

    int batchId_;
    State state_ = State::Queued;
    OperationType operationType_ = OperationType::Download;
    bool isActive_ = false;

    QLabel *iconLabel_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QProgressBar *progressBar_ = nullptr;
    QPushButton *cancelButton_ = nullptr;
};

#endif // BATCHPROGRESSWIDGET_H
