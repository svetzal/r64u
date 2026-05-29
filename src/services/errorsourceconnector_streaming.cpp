#include "audiostreamreceiverservice.h"
#include "errorhandler.h"
#include "istreamcontrolservice.h"
#include "streamcontrolservice.h"
#include "streamingservice.h"
#include "videostreamreceiverservice.h"

void ErrorHandler::connectStreamingServiceSources(StreamingService *ss)
{
    connect(ss, &StreamingService::error, this, &ErrorHandler::handleStreamingError);
    if (ss->videoReceiver()) {
        connect(ss->videoReceiver(), &VideoStreamReceiverService::socketError, this,
                &ErrorHandler::handleStreamingError);
    }
    if (ss->audioReceiver()) {
        connect(ss->audioReceiver(), &AudioStreamReceiverService::socketError, this,
                &ErrorHandler::handleStreamingError);
    }
    auto *streamControl = qobject_cast<StreamControlService *>(ss->streamControl());
    if (streamControl) {
        connect(streamControl, &StreamControlService::connectionError, this,
                &ErrorHandler::handleConnectionError);
    }
    if (ss->streamControl()) {
        connect(ss->streamControl(), &IStreamControlService::commandFailed, this,
                [this](const QString &command, const QString &error) {
                    handleOperationFailed(command, error);
                });
    }
}
