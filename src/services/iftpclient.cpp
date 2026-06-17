/**
 * @file iftpclient.cpp
 * @brief Implementation file for IFtpClient interface.
 *
 * This file exists to support Qt's MOC (Meta-Object Compiler) which requires
 * a .cpp file to generate signal/slot infrastructure for the interface.
 */

#include "iftpclient.h"

#include "errortypes.h"

IFtpClient::IFtpClient(QObject *parent) : IErrorEmitter(parent)
{
    connect(this, &IFtpClient::error, this, [this](const QString &message) {
        emit errorReported(ErrorCategory::FileOperation, ErrorSeverity::Warning, tr("Error"),
                           message);
    });
}
