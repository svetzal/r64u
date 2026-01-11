#ifndef STATUSMESSAGESERVICE_H
#define STATUSMESSAGESERVICE_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QQueue>

/**
 * @brief Service for coordinating status bar messages with priority queuing
 *
 * StatusMessageService provides centralized status message management with:
 * - Priority-based message queuing (errors take precedence over info)
 * - Flickering prevention through minimum display times
 * - Automatic timeout handling per severity level
 * - Consistent API for all components
 *
 * Usage:
 * @code
 * StatusMessageService *statusService = new StatusMessageService(this);
 * connect(statusService, &StatusMessageService::displayMessage,
 *         statusBar(), &QStatusBar::showMessage);
 *
 * // From any component:
 * statusService->showInfo("Transfer started");
 * statusService->showWarning("Connection slow");
 * statusService->showError("Transfer failed: timeout");
 * @endcode
 */
class StatusMessageService : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Message priority/severity levels
     */
    enum class Priority {
        Info = 0,      ///< Informational messages (lowest priority)
        Warning = 1,   ///< Warning messages (medium priority)
        Error = 2      ///< Error messages (highest priority)
    };
    Q_ENUM(Priority)

    explicit StatusMessageService(QObject *parent = nullptr);
    ~StatusMessageService() override;

    /**
     * @brief Show an informational message
     * @param message The message text
     * @param timeout Optional timeout in ms (0 = use default for severity)
     */
    void showInfo(const QString &message, int timeout = 0);

    /**
     * @brief Show a warning message
     * @param message The message text
     * @param timeout Optional timeout in ms (0 = use default for severity)
     */
    void showWarning(const QString &message, int timeout = 0);

    /**
     * @brief Show an error message
     * @param message The message text
     * @param timeout Optional timeout in ms (0 = use default for severity)
     */
    void showError(const QString &message, int timeout = 0);

    /**
     * @brief Show a message with explicit priority and timeout
     * @param message The message text
     * @param priority Message priority level
     * @param timeout Timeout in ms (0 = use default for priority)
     */
    void showMessage(const QString &message, Priority priority = Priority::Info, int timeout = 0);

    /**
     * @brief Clear the current message and any queued messages
     */
    void clearMessages();

    /**
     * @brief Get the default timeout for a given priority
     * @param priority The priority level
     * @return Timeout in milliseconds
     */
    static int defaultTimeoutForPriority(Priority priority);

    /**
     * @brief Get the minimum display time to prevent flickering
     * @return Minimum display time in milliseconds
     */
    int minimumDisplayTime() const { return minimumDisplayTime_; }

    /**
     * @brief Set the minimum display time to prevent flickering
     * @param ms Minimum display time in milliseconds
     */
    void setMinimumDisplayTime(int ms) { minimumDisplayTime_ = ms; }

    /**
     * @brief Check if a message is currently being displayed
     * @return true if a message is active
     */
    bool isDisplaying() const { return isDisplaying_; }

    /**
     * @brief Get the current message being displayed
     * @return The current message text, or empty if none
     */
    QString currentMessage() const { return currentMessage_; }

    /**
     * @brief Get the current message priority
     * @return The priority of the current message
     */
    Priority currentPriority() const { return currentPriority_; }

signals:
    /**
     * @brief Emitted when a message should be displayed
     * @param message The message text
     * @param timeout Display timeout in milliseconds
     */
    void displayMessage(const QString &message, int timeout);

    /**
     * @brief Emitted when the message queue changes
     * @param queueSize Number of messages waiting in queue
     */
    void queueChanged(int queueSize);

private slots:
    void onDisplayTimerTimeout();
    void onMessageTimeout();

private:
    struct QueuedMessage {
        QString text;
        Priority priority;
        int timeout;
    };

    void enqueueMessage(const QString &message, Priority priority, int timeout);
    void processNextMessage();
    void displayImmediately(const QString &message, Priority priority, int timeout);

    QQueue<QueuedMessage> messageQueue_;
    QTimer *displayTimer_;      // Minimum display time timer
    QTimer *messageTimer_;      // Current message timeout timer

    QString currentMessage_;
    Priority currentPriority_ = Priority::Info;
    bool isDisplaying_ = false;
    int minimumDisplayTime_ = 100;  // 100ms minimum to prevent flickering
};

#endif // STATUSMESSAGESERVICE_H
