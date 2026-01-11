#ifndef TRANSFERPROGRESSWIDGET_H
#define TRANSFERPROGRESSWIDGET_H

#include <QWidget>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

#include "models/transferqueue.h"

class TransferProgressWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TransferProgressWidget(QWidget *parent = nullptr);

    void setTransferQueue(TransferQueue *queue);

signals:
    void statusMessage(const QString &message, int timeout = 0);

private slots:
    void onOperationStarted(const QString &fileName, OperationType type);
    void onOperationCompleted(const QString &fileName);
    void onOperationFailed(const QString &fileName, const QString &error);
    void onQueueChanged();
    void onAllOperationsCompleted();
    void onOperationsCancelled();
    void onShowProgress();
    void onDeleteProgressUpdate(const QString &fileName, int current, int total);
    void onOverwriteConfirmationNeeded(const QString &fileName, OperationType type);

private:
    void setupUi();
    void updateProgressDisplay();

    // Dependencies (not owned)
    TransferQueue *transferQueue_ = nullptr;

    // State
    int operationTotalCount_ = 0;
    int operationCompletedCount_ = 0;
    bool progressPending_ = false;
    OperationType currentOperationType_ = OperationType::Upload;

    // UI widgets
    QProgressBar *progressBar_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QPushButton *cancelButton_ = nullptr;
    QTimer *delayTimer_ = nullptr;
};

#endif // TRANSFERPROGRESSWIDGET_H
