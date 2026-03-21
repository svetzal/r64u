#ifndef TRANSFERQUEUEWIDGET_H
#define TRANSFERQUEUEWIDGET_H

#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QWidget>

class TransferQueue;

class TransferQueueWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TransferQueueWidget(QWidget *parent = nullptr);

    void setTransferQueue(TransferQueue *queue);

private slots:
    void onQueueChanged();
    void onClearCompleted();
    void onCancelAll();

private:
    void setupUi();
    void updateButtons();

    TransferQueue *queue_ = nullptr;
    QListView *listView_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QPushButton *clearButton_ = nullptr;
    QPushButton *cancelButton_ = nullptr;
};

#endif  // TRANSFERQUEUEWIDGET_H
