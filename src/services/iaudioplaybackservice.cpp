/**
 * @file iaudioplaybackservice.cpp
 * @brief MOC implementation for IAudioPlaybackService interface.
 */

#include "iaudioplaybackservice.h"

IAudioPlaybackService::IAudioPlaybackService(QObject *parent) : IErrorEmitter(parent)
{
    // Forward errorOccurred to the uniform IErrorEmitter signal
    connect(this, &IAudioPlaybackService::errorOccurred, this, [this](const QString &error) {
        emit errorReported(ErrorCategory::System, ErrorSeverity::Warning, tr("Streaming Error"),
                           error);
    });
}
