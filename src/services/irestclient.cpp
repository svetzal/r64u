#include "irestclient.h"

IRestClient::IRestClient(QObject *parent) : IErrorEmitter(parent)
{
    // Forward operationFailed to the uniform IErrorEmitter signal
    connect(this, &IRestClient::operationFailed, this,
            [this](const QString &operation, const QString &error) {
                emit errorReported(ErrorCategory::FileOperation, ErrorSeverity::Warning,
                                   tr("%1 failed").arg(operation), error);
            });
    // Forward connectionError to the uniform IErrorEmitter signal
    connect(this, &IRestClient::connectionError, this, [this](const QString &error) {
        emit errorReported(ErrorCategory::Connection, ErrorSeverity::Critical,
                           tr("Connection Error"), error);
    });
}
