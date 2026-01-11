#include "errorhandler.h"

#include <QMessageBox>
#include <QPushButton>
#include <QDebug>

ErrorHandler::ErrorHandler(QWidget *parentWidget, QObject *parent)
    : QObject(parent)
    , parentWidget_(parentWidget)
{
}

void ErrorHandler::handleError(ErrorCategory category,
                               ErrorSeverity severity,
                               const QString &title,
                               const QString &details)
{
    // Log the error
    logError(category, severity, title, details);

    // Build the display message
    QString message = title;
    if (!details.isEmpty() && details != title) {
        message = QString("%1: %2").arg(title, details);
    }

    // Always show in status bar
    emit statusMessage(message, timeoutForSeverity(severity));

    // For critical errors, also show a dialog
    if (severity == ErrorSeverity::Critical) {
        showErrorDialog(title, details.isEmpty() ? title : details);
    }
}

void ErrorHandler::handleErrorWithRetry(ErrorCategory category,
                                        const QString &title,
                                        const QString &details,
                                        const std::function<void()> &retryCallback)
{
    // Log the error
    logError(category, ErrorSeverity::Warning, title, details);

    // Show status message
    QString message = title;
    if (!details.isEmpty() && details != title) {
        message = QString("%1: %2").arg(title, details);
    }
    emit statusMessage(message, timeoutForSeverity(ErrorSeverity::Warning));

    // Show retry dialog
    showRetryDialog(title, details.isEmpty() ? title : details, retryCallback);
}

void ErrorHandler::handleConnectionError(const QString &message)
{
    handleError(ErrorCategory::Connection,
                ErrorSeverity::Critical,
                tr("Connection Error"),
                message);
}

void ErrorHandler::handleOperationFailed(const QString &operation, const QString &error)
{
    handleError(ErrorCategory::FileOperation,
                ErrorSeverity::Warning,
                tr("%1 failed").arg(operation),
                error);
}

void ErrorHandler::handleDataError(const QString &message)
{
    handleError(ErrorCategory::FileOperation,
                ErrorSeverity::Warning,
                tr("Error"),
                message);
}

void ErrorHandler::showErrorDialog(const QString &title, const QString &message)
{
    QMessageBox::warning(parentWidget_, title, message);
}

bool ErrorHandler::showRetryDialog(const QString &title,
                                   const QString &message,
                                   const std::function<void()> &retryCallback)
{
    QMessageBox msgBox(parentWidget_);
    msgBox.setWindowTitle(title);
    msgBox.setText(message);
    msgBox.setIcon(QMessageBox::Warning);

    QPushButton *retryButton = msgBox.addButton(tr("Retry"), QMessageBox::AcceptRole);
    msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);

    msgBox.setDefaultButton(retryButton);
    msgBox.exec();

    if (msgBox.clickedButton() == retryButton) {
        if (retryCallback) {
            retryCallback();
        }
        return true;
    }
    return false;
}

void ErrorHandler::logError(ErrorCategory category,
                            ErrorSeverity severity,
                            const QString &title,
                            const QString &details)
{
    // Log to Qt debug output
    QString logMessage = QString("[%1/%2] %3")
        .arg(categoryToString(category),
             severityToString(severity),
             title);

    if (!details.isEmpty() && details != title) {
        logMessage += QString(": %1").arg(details);
    }

    switch (severity) {
    case ErrorSeverity::Info:
        qInfo().noquote() << logMessage;
        break;
    case ErrorSeverity::Warning:
        qWarning().noquote() << logMessage;
        break;
    case ErrorSeverity::Critical:
        qCritical().noquote() << logMessage;
        break;
    }

    // Emit signal for any listeners (monitoring, telemetry, etc.)
    emit errorLogged(category, severity, title, details);
}

int ErrorHandler::timeoutForSeverity(ErrorSeverity severity)
{
    switch (severity) {
    case ErrorSeverity::Info:
        return 3000;  // 3 seconds
    case ErrorSeverity::Warning:
        return 5000;  // 5 seconds
    case ErrorSeverity::Critical:
        return 0;     // No timeout - stays until replaced
    }
    return 5000;
}

QString ErrorHandler::categoryToString(ErrorCategory category)
{
    switch (category) {
    case ErrorCategory::Connection:
        return QStringLiteral("Connection");
    case ErrorCategory::FileOperation:
        return QStringLiteral("FileOp");
    case ErrorCategory::Validation:
        return QStringLiteral("Validation");
    case ErrorCategory::System:
        return QStringLiteral("System");
    }
    return QStringLiteral("Unknown");
}

QString ErrorHandler::severityToString(ErrorSeverity severity)
{
    switch (severity) {
    case ErrorSeverity::Info:
        return QStringLiteral("INFO");
    case ErrorSeverity::Warning:
        return QStringLiteral("WARN");
    case ErrorSeverity::Critical:
        return QStringLiteral("CRIT");
    }
    return QStringLiteral("UNKNOWN");
}
