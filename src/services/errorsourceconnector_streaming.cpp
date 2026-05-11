#include "audiostreamreceiver.h"
#include "errorhandler.h"
#include "istreamcontrolclient.h"
#include "streamcontrolclient.h"
#include "streamingservice.h"
#include "videostreamreceiver.h"

void ErrorHandler::connectStreamingServiceSources(StreamingService *ss)
{
    connect(ss, &StreamingService::error, this, &ErrorHandler::handleStreamingError);
    if (ss->videoReceiver()) {
        connect(ss->videoReceiver(), &VideoStreamReceiver::socketError, this,
                &ErrorHandler::handleStreamingError);
    }
    if (ss->audioReceiver()) {
        connect(ss->audioReceiver(), &AudioStreamReceiver::socketError, this,
                &ErrorHandler::handleStreamingError);
    }
    auto *streamControl = qobject_cast<StreamControlClient *>(ss->streamControl());
    if (streamControl) {
        connect(streamControl, &StreamControlClient::connectionError, this,
                &ErrorHandler::handleConnectionError);
    }
    if (ss->streamControl()) {
        connect(ss->streamControl(), &IStreamControlClient::commandFailed, this,
                [this](const QString &command, const QString &error) {
                    handleOperationFailed(command, error);
                });
    }
}
