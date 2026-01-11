/**
 * @file errorhandler.h
 * @brief Centralized error handling service for consistent error presentation.
 *
 * This service standardizes how errors are categorized, displayed, and logged
 * across the application, providing a consistent user experience.
 */

#ifndef ERRORHANDLER_H
#define ERRORHANDLER_H

#include <QObject>
#include <QString>
#include <functional>

class QWidget;

/**
 * @brief Categories of errors for appropriate handling.
 */
enum class ErrorCategory {
    Connection,     ///< Network/connection errors (FTP, REST)
    FileOperation,  ///< File transfer, delete, listing errors
    Validation,     ///< Input validation, configuration errors
    System          ///< General system/application errors
};

/**
 * @brief Severity levels determining how errors are displayed.
 */
enum class ErrorSeverity {
    Info,      ///< Informational - status bar only, short timeout
    Warning,   ///< Warning - status bar, longer timeout
    Critical   ///< Critical - status bar + dialog box
};

/**
 * @brief Centralized error handling service.
 *
 * ErrorHandler provides consistent error presentation across the application:
 * - Categorizes errors for appropriate handling
 * - Displays errors based on severity (status bar vs dialog)
 * - Supports retry callbacks for recoverable errors
 * - Logs errors for debugging
 *
 * @par Example usage:
 * @code
 * ErrorHandler *handler = new ErrorHandler(parentWidget, this);
 *
 * // Connect error signals from various sources
 * connect(connection, &DeviceConnection::connectionError,
 *         handler, &ErrorHandler::handleConnectionError);
 *
 * // Handle with custom severity
 * handler->handleError(ErrorCategory::FileOperation,
 *                      ErrorSeverity::Warning,
 *                      "Upload failed: file.txt",
 *                      "The file could not be uploaded");
 *
 * // Handle with retry option
 * handler->handleErrorWithRetry(ErrorCategory::Connection,
 *                               "Connection lost",
 *                               "Lost connection to device",
 *                               [this]() { reconnect(); });
 * @endcode
 */
class ErrorHandler : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs an error handler.
     * @param parentWidget Widget to use as parent for dialogs (not owned).
     * @param parent Optional parent QObject for memory management.
     */
    explicit ErrorHandler(QWidget *parentWidget, QObject *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~ErrorHandler() override = default;

    /// @name Generic Error Handling
    /// @{

    /**
     * @brief Handles an error with specified category and severity.
     * @param category The error category.
     * @param severity The error severity.
     * @param title Short error title/summary.
     * @param details Detailed error message.
     */
    void handleError(ErrorCategory category,
                     ErrorSeverity severity,
                     const QString &title,
                     const QString &details = QString());

    /**
     * @brief Handles an error with a retry option.
     * @param category The error category.
     * @param title Short error title/summary.
     * @param details Detailed error message.
     * @param retryCallback Function to call if user chooses retry.
     */
    void handleErrorWithRetry(ErrorCategory category,
                              const QString &title,
                              const QString &details,
                              const std::function<void()> &retryCallback);
    /// @}

    /// @name Convenience Methods for Common Error Sources
    /// @{

    /**
     * @brief Handles a connection error (critical severity).
     * @param message The error message.
     *
     * Connection errors are always critical and display a dialog.
     */
    void handleConnectionError(const QString &message);

    /**
     * @brief Handles a file operation error (warning severity).
     * @param operation The operation that failed (e.g., "upload", "download").
     * @param error The error message.
     */
    void handleOperationFailed(const QString &operation, const QString &error);

    /**
     * @brief Handles a model/data error (warning severity).
     * @param message The error message.
     *
     * Used for errors from RemoteFileModel and similar data sources.
     */
    void handleDataError(const QString &message);
    /// @}

signals:
    /**
     * @brief Emitted to display a status bar message.
     * @param message The message text.
     * @param timeout Display timeout in milliseconds (0 for no timeout).
     */
    void statusMessage(const QString &message, int timeout);

    /**
     * @brief Emitted when an error is logged (for debugging/monitoring).
     * @param category The error category.
     * @param severity The error severity.
     * @param title The error title.
     * @param details The error details.
     */
    void errorLogged(ErrorCategory category,
                     ErrorSeverity severity,
                     const QString &title,
                     const QString &details);

private:
    /**
     * @brief Shows a dialog for critical errors.
     * @param title Dialog title.
     * @param message Dialog message.
     */
    void showErrorDialog(const QString &title, const QString &message);

    /**
     * @brief Shows a retry dialog for recoverable errors.
     * @param title Dialog title.
     * @param message Dialog message.
     * @param retryCallback Function to call on retry.
     * @return True if user chose retry.
     */
    bool showRetryDialog(const QString &title,
                         const QString &message,
                         const std::function<void()> &retryCallback);

    /**
     * @brief Logs an error for debugging.
     * @param category The error category.
     * @param severity The error severity.
     * @param title The error title.
     * @param details The error details.
     */
    void logError(ErrorCategory category,
                  ErrorSeverity severity,
                  const QString &title,
                  const QString &details);

    /**
     * @brief Gets the status bar timeout for a severity level.
     * @param severity The error severity.
     * @return Timeout in milliseconds.
     */
    [[nodiscard]] static int timeoutForSeverity(ErrorSeverity severity);

    /**
     * @brief Converts category to string for logging.
     * @param category The error category.
     * @return String representation.
     */
    [[nodiscard]] static QString categoryToString(ErrorCategory category);

    /**
     * @brief Converts severity to string for logging.
     * @param severity The error severity.
     * @return String representation.
     */
    [[nodiscard]] static QString severityToString(ErrorSeverity severity);

    QWidget *parentWidget_ = nullptr;
};

#endif // ERRORHANDLER_H
