#ifndef TRANSFERPROGRESSCONTAINER_H
#define TRANSFERPROGRESSCONTAINER_H

#include <QWidget>
#include <QVBoxLayout>
#include <QMap>
#include <QTimer>

#include "models/transferqueue.h"

class TransferService;
class BatchProgressWidget;

/**
 * @brief Container widget managing multiple batch progress widgets.
 *
 * This widget creates and manages BatchProgressWidget instances for each
 * active or queued transfer batch. It shows/hides based on whether there
 * are any batches to display.
 *
 * Visual layout:
 * - Active batch shown at top with full progress
 * - Queued batches shown below with "Queued" state
 * - Widgets removed when batch completes (with brief delay for feedback)
 */
class TransferProgressContainer : public QWidget
{
    Q_OBJECT

public:
    explicit TransferProgressContainer(QWidget *parent = nullptr);

    /**
     * @brief Sets the transfer service for signal connections.
     * @param service The transfer service to monitor.
     */
    void setTransferService(TransferService *service);

signals:
    /**
     * @brief Emitted when a status message should be displayed.
     * @param message The message text.
     * @param timeout Display timeout in milliseconds.
     */
    void statusMessage(const QString &message, int timeout);

    /**
     * @brief Emitted when status messages should be cleared.
     */
    void clearStatusMessages();

private slots:
    void onQueueChanged();
    void onBatchStarted(int batchId);
    void onBatchProgressUpdate(int batchId, int completed, int total);
    void onBatchCompleted(int batchId);
    void onOperationStarted(const QString &fileName, OperationType type);
    void onOperationCompleted(const QString &fileName);
    void onOperationFailed(const QString &fileName, const QString &error);
    void onAllOperationsCompleted();
    void onOperationsCancelled();
    void onScanningStarted(const QString &folderName, OperationType type);
    void onScanningProgress(int directoriesScanned, int directoriesRemaining, int filesDiscovered);
    void onDirectoryCreationProgress(int created, int total);
    void onCancelRequested(int batchId);
    void onRemoveBatchWidget(int batchId);
    void onOverwriteConfirmationNeeded(const QString &fileName, OperationType type);
    void onFolderExistsConfirmationNeeded(const QStringList &folderNames);

private:
    void setupUi();
    void updateVisibility();
    BatchProgressWidget* findOrCreateWidget(int batchId);
    void updateAllBatchWidgets();

    TransferService *transferService_ = nullptr;
    QVBoxLayout *layout_ = nullptr;
    QMap<int, BatchProgressWidget*> widgets_;
    OperationType currentOperationType_ = OperationType::Download;
};

#endif // TRANSFERPROGRESSCONTAINER_H
