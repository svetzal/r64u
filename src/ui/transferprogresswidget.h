#ifndef TRANSFERPROGRESSWIDGET_H
#define TRANSFERPROGRESSWIDGET_H

#include <QWidget>
#include <QProgressBar>
#include <QLabel>
#include <QTimer>

class TransferQueue;

class TransferProgressWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TransferProgressWidget(QWidget *parent = nullptr);

    void setTransferQueue(TransferQueue *queue);

signals:
    void statusMessage(const QString &message, int timeout = 0);

private slots:
    void onTransferStarted(const QString &fileName);
    void onTransferCompleted(const QString &fileName);
    void onTransferFailed(const QString &fileName, const QString &error);
    void onTransferQueueChanged();
    void onAllTransfersCompleted();
    void onShowTransferProgress();

private:
    void setupUi();
    void updateProgressDisplay();

    // Dependencies (not owned)
    TransferQueue *transferQueue_ = nullptr;

    // State
    int transferTotalCount_ = 0;
    int transferCompletedCount_ = 0;
    bool transferProgressPending_ = false;

    // UI widgets
    QProgressBar *progressBar_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QTimer *delayTimer_ = nullptr;
};

#endif // TRANSFERPROGRESSWIDGET_H
