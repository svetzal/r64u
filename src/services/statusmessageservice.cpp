#include "statusmessageservice.h"

StatusMessageService::StatusMessageService(QObject *parent)
    : QObject(parent)
    , displayTimer_(new QTimer(this))
    , messageTimer_(new QTimer(this))
{
    // Display timer is single-shot for minimum display time
    displayTimer_->setSingleShot(true);
    connect(displayTimer_, &QTimer::timeout,
            this, &StatusMessageService::onDisplayTimerTimeout);

    // Message timer is single-shot for message timeout
    messageTimer_->setSingleShot(true);
    connect(messageTimer_, &QTimer::timeout,
            this, &StatusMessageService::onMessageTimeout);
}

StatusMessageService::~StatusMessageService() = default;

void StatusMessageService::showInfo(const QString &message, int timeout)
{
    showMessage(message, Priority::Info, timeout);
}

void StatusMessageService::showWarning(const QString &message, int timeout)
{
    showMessage(message, Priority::Warning, timeout);
}

void StatusMessageService::showError(const QString &message, int timeout)
{
    showMessage(message, Priority::Error, timeout);
}

void StatusMessageService::showMessage(const QString &message, Priority priority, int timeout)
{
    if (message.isEmpty()) {
        return;
    }

    // Use default timeout if not specified
    if (timeout <= 0) {
        timeout = defaultTimeoutForPriority(priority);
    }

    // If nothing is displaying, show immediately
    if (!isDisplaying_) {
        displayImmediately(message, priority, timeout);
        return;
    }

    // If new message has higher priority than current, interrupt
    if (priority > currentPriority_) {
        // Stop timers and display new message
        displayTimer_->stop();
        messageTimer_->stop();
        displayImmediately(message, priority, timeout);
        return;
    }

    // If same or lower priority, queue the message
    enqueueMessage(message, priority, timeout);
}

void StatusMessageService::clearMessages()
{
    displayTimer_->stop();
    messageTimer_->stop();
    messageQueue_.clear();
    currentMessage_.clear();
    currentPriority_ = Priority::Info;
    isDisplaying_ = false;
    emit displayMessage(QString(), 0);
    emit queueChanged(0);
}

int StatusMessageService::defaultTimeoutForPriority(Priority priority)
{
    switch (priority) {
    case Priority::Info:
        return 3000;   // 3 seconds for info
    case Priority::Warning:
        return 5000;   // 5 seconds for warnings
    case Priority::Error:
        return 8000;   // 8 seconds for errors - give user time to read
    }
    return 3000;
}

void StatusMessageService::onDisplayTimerTimeout()
{
    // Minimum display time has passed, now check if message should timeout
    // or if there's a queued message to show
    if (!messageQueue_.isEmpty()) {
        processNextMessage();
    }
    // Otherwise, let the message timer handle the full timeout
}

void StatusMessageService::onMessageTimeout()
{
    // Message has timed out
    isDisplaying_ = false;
    currentMessage_.clear();
    currentPriority_ = Priority::Info;

    // Check if there are queued messages
    if (!messageQueue_.isEmpty()) {
        processNextMessage();
    } else {
        // Clear the status bar
        emit displayMessage(QString(), 0);
    }
}

void StatusMessageService::enqueueMessage(const QString &message, Priority priority, int timeout)
{
    QueuedMessage qm;
    qm.text = message;
    qm.priority = priority;
    qm.timeout = timeout;

    // Insert based on priority - higher priority messages go first
    // Find insertion point
    int insertPos = 0;
    for (int i = 0; i < messageQueue_.size(); ++i) {
        if (messageQueue_[i].priority >= priority) {
            insertPos = i + 1;
        } else {
            break;
        }
    }

    // QQueue doesn't have insert, so we need to rebuild
    QQueue<QueuedMessage> newQueue;
    for (int i = 0; i < insertPos && !messageQueue_.isEmpty(); ++i) {
        newQueue.enqueue(messageQueue_.dequeue());
    }
    newQueue.enqueue(qm);
    while (!messageQueue_.isEmpty()) {
        newQueue.enqueue(messageQueue_.dequeue());
    }
    messageQueue_ = newQueue;

    emit queueChanged(messageQueue_.size());
}

void StatusMessageService::processNextMessage()
{
    if (messageQueue_.isEmpty()) {
        return;
    }

    QueuedMessage next = messageQueue_.dequeue();
    emit queueChanged(messageQueue_.size());

    displayImmediately(next.text, next.priority, next.timeout);
}

void StatusMessageService::displayImmediately(const QString &message, Priority priority, int timeout)
{
    currentMessage_ = message;
    currentPriority_ = priority;
    isDisplaying_ = true;

    // Emit signal to display
    emit displayMessage(message, timeout);

    // Start minimum display timer
    displayTimer_->start(minimumDisplayTime_);

    // Start message timeout timer
    messageTimer_->start(timeout);
}
