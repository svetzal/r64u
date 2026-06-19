#include "errorhandler.h"

#include "ierroremitter.h"
#include "ierrorpresenter.h"
#include "statusmessageservice.h"

#include <QDebug>

ErrorHandler::ErrorHandler(IErrorPresenter *presenter, QObject *parent)
    : QObject(parent), presenter_(presenter)
{
}

void ErrorHandler::setPresenter(IErrorPresenter *presenter)
{
    presenter_ = presenter;
}

void ErrorHandler::registerSource(IErrorEmitter *source)
{
    if (!source) {
        return;
    }
    connect(source, &IErrorEmitter::errorReported, this, &ErrorHandler::handleError);
}

void ErrorHandler::handleError(ErrorCategory category, ErrorSeverity severity, const QString &title,
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

void ErrorHandler::handleErrorWithRetry(ErrorCategory category, const QString &title,
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

void ErrorHandler::info(ErrorCategory category, const QString &message)
{
    handleError(category, ErrorSeverity::Info, message);
}

void ErrorHandler::showErrorDialog(const QString &title, const QString &message)
{
    if (presenter_) {
        presenter_->showErrorDialog(title, message);
    }
}

bool ErrorHandler::showRetryDialog(const QString &title, const QString &message,
                                   const std::function<void()> &retryCallback)
{
    if (presenter_) {
        return presenter_->showRetryDialog(title, message, retryCallback);
    }
    return false;
}

void ErrorHandler::logError(ErrorCategory category, ErrorSeverity severity, const QString &title,
                            const QString &details)
{
    // Log to Qt debug output
    QString logMessage =
        QString("[%1/%2] %3").arg(categoryToString(category), severityToString(severity), title);

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
        return StatusMessageService::defaultTimeoutForPriority(
            StatusMessageService::Priority::Info);
    case ErrorSeverity::Warning:
        return StatusMessageService::defaultTimeoutForPriority(
            StatusMessageService::Priority::Warning);
    case ErrorSeverity::Critical:
        return 0;
    }
    return StatusMessageService::defaultTimeoutForPriority(StatusMessageService::Priority::Warning);
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
