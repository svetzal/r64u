#ifndef TRANSFERTIMEOUTMANAGER_H
#define TRANSFERTIMEOUTMANAGER_H

#include <QObject>
#include <QTimer>

/// Owns the per-operation timeout timer for TransferQueue.
/// Emits operationTimedOut() when a transfer takes longer than the threshold.
class TransferTimeoutManager : public QObject
{
    Q_OBJECT

public:
    static constexpr int OperationTimeoutMs = 300000;  // 5 minutes

    explicit TransferTimeoutManager(int timeoutMs = OperationTimeoutMs, QObject *parent = nullptr);

    void start();
    void stop();

signals:
    void operationTimedOut();

private slots:
    void onTimerExpired();

private:
    int timeoutMs_;
    QTimer *timer_;
};

#endif  // TRANSFERTIMEOUTMANAGER_H
